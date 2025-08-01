// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

package org.apache.doris.nereids.util;

import org.apache.doris.catalog.TableIf.TableType;
import org.apache.doris.common.Config;
import org.apache.doris.common.MaterializedViewException;
import org.apache.doris.common.NereidsException;
import org.apache.doris.common.Pair;
import org.apache.doris.common.UserException;
import org.apache.doris.nereids.CascadesContext;
import org.apache.doris.nereids.analyzer.Scope;
import org.apache.doris.nereids.analyzer.UnboundSlot;
import org.apache.doris.nereids.exceptions.AnalysisException;
import org.apache.doris.nereids.properties.PhysicalProperties;
import org.apache.doris.nereids.rules.analysis.ExpressionAnalyzer;
import org.apache.doris.nereids.rules.expression.ExpressionRewrite;
import org.apache.doris.nereids.rules.expression.ExpressionRewriteContext;
import org.apache.doris.nereids.rules.expression.ExpressionRuleExecutor;
import org.apache.doris.nereids.rules.expression.rules.FoldConstantRule;
import org.apache.doris.nereids.rules.expression.rules.ReplaceVariableByLiteral;
import org.apache.doris.nereids.trees.SuperClassId;
import org.apache.doris.nereids.trees.TreeNode;
import org.apache.doris.nereids.trees.expressions.Alias;
import org.apache.doris.nereids.trees.expressions.And;
import org.apache.doris.nereids.trees.expressions.Cast;
import org.apache.doris.nereids.trees.expressions.ComparisonPredicate;
import org.apache.doris.nereids.trees.expressions.CompoundPredicate;
import org.apache.doris.nereids.trees.expressions.EqualTo;
import org.apache.doris.nereids.trees.expressions.ExprId;
import org.apache.doris.nereids.trees.expressions.Expression;
import org.apache.doris.nereids.trees.expressions.InPredicate;
import org.apache.doris.nereids.trees.expressions.IsNull;
import org.apache.doris.nereids.trees.expressions.MarkJoinSlotReference;
import org.apache.doris.nereids.trees.expressions.NamedExpression;
import org.apache.doris.nereids.trees.expressions.Not;
import org.apache.doris.nereids.trees.expressions.Or;
import org.apache.doris.nereids.trees.expressions.Slot;
import org.apache.doris.nereids.trees.expressions.SlotReference;
import org.apache.doris.nereids.trees.expressions.WindowExpression;
import org.apache.doris.nereids.trees.expressions.functions.agg.Avg;
import org.apache.doris.nereids.trees.expressions.functions.agg.Max;
import org.apache.doris.nereids.trees.expressions.functions.agg.Min;
import org.apache.doris.nereids.trees.expressions.functions.agg.Sum;
import org.apache.doris.nereids.trees.expressions.literal.BooleanLiteral;
import org.apache.doris.nereids.trees.expressions.literal.ComparableLiteral;
import org.apache.doris.nereids.trees.expressions.literal.Literal;
import org.apache.doris.nereids.trees.expressions.literal.NullLiteral;
import org.apache.doris.nereids.trees.expressions.literal.StringLiteral;
import org.apache.doris.nereids.trees.expressions.visitor.DefaultExpressionRewriter;
import org.apache.doris.nereids.trees.expressions.visitor.DefaultExpressionVisitor;
import org.apache.doris.nereids.trees.plans.Plan;
import org.apache.doris.nereids.trees.plans.logical.LogicalEmptyRelation;
import org.apache.doris.nereids.trees.plans.logical.LogicalUnion;
import org.apache.doris.nereids.trees.plans.visitor.ExpressionLineageReplacer;
import org.apache.doris.nereids.types.BooleanType;
import org.apache.doris.nereids.types.coercion.NumericType;
import org.apache.doris.qe.ConnectContext;

import com.google.common.base.Preconditions;
import com.google.common.base.Predicate;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableList.Builder;
import com.google.common.collect.ImmutableMap;
import com.google.common.collect.ImmutableSet;
import com.google.common.collect.Lists;
import com.google.common.collect.Maps;
import com.google.common.collect.Sets;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.BitSet;
import java.util.Collection;
import java.util.Deque;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.function.Function;
import java.util.stream.Collectors;

/**
 * Expression rewrite helper class.
 */
public class ExpressionUtils {

    public static final List<Expression> EMPTY_CONDITION = ImmutableList.of();

    public static List<Expression> extractConjunction(Expression expr) {
        return extract(And.class, expr);
    }

    public static Set<Expression> extractConjunctionToSet(Expression expr) {
        Set<Expression> exprSet = Sets.newLinkedHashSet();
        extract(And.class, expr, exprSet);
        return exprSet;
    }

    public static List<Expression> extractDisjunction(Expression expr) {
        return extract(Or.class, expr);
    }

    /**
     * Split predicates with `And/Or` form recursively.
     * Some examples for `And`:
     * <p>
     * a and b -> a, b
     * (a and b) and c -> a, b, c
     * (a or b) and (c and d) -> (a or b), c , d
     * <p>
     * Stop recursion when meeting `Or`, so this func will ignore `And` inside `Or`.
     * Warning examples:
     * (a and b) or c -> (a and b) or c
     */
    public static List<Expression> extract(CompoundPredicate expr) {
        return extract(expr.getClass(), expr);
    }

    private static List<Expression> extract(Class<? extends Expression> type, Expression expr) {
        List<Expression> result = Lists.newArrayList();
        Deque<Expression> stack = new ArrayDeque<>();
        stack.push(expr);
        while (!stack.isEmpty()) {
            Expression current = stack.pop();
            if (type.isInstance(current)) {
                for (Expression child : current.children()) {
                    stack.push(child);
                }
            } else {
                result.add(current);
            }
        }
        result = Lists.reverse(result);
        return result;
    }

    private static void extract(Class<? extends Expression> type, Expression expr, Collection<Expression> result) {
        result.addAll(extract(type, expr));
    }

    public static Optional<Pair<Slot, Slot>> extractEqualSlot(Expression expr) {
        if (expr instanceof EqualTo && expr.child(0).isSlot() && expr.child(1).isSlot()) {
            return Optional.of(Pair.of((Slot) expr.child(0), (Slot) expr.child(1)));
        }
        return Optional.empty();
    }

    public static Optional<Expression> optionalAnd(List<Expression> expressions) {
        if (expressions.isEmpty()) {
            return Optional.empty();
        } else {
            return Optional.of(ExpressionUtils.and(expressions));
        }
    }

    /**
     * And two list.
     */
    public static Optional<Expression> optionalAnd(List<Expression> left, List<Expression> right) {
        if (left.isEmpty() && right.isEmpty()) {
            return Optional.empty();
        } else if (left.isEmpty()) {
            return optionalAnd(right);
        } else if (right.isEmpty()) {
            return optionalAnd(left);
        } else {
            return Optional.of(new And(optionalAnd(left).get(), optionalAnd(right).get()));
        }
    }

    public static Optional<Expression> optionalAnd(Expression... expressions) {
        return optionalAnd(Lists.newArrayList(expressions));
    }

    public static Optional<Expression> optionalAnd(Collection<Expression> collection) {
        return optionalAnd(ImmutableList.copyOf(collection));
    }

    /**
     *  AND / OR expression, also remove duplicate expression, boolean literal
     */
    public static Expression compound(boolean isAnd, Collection<Expression> expressions) {
        return isAnd ? and(expressions) : or(expressions);
    }

    /**
     *  AND expression, also remove duplicate expression, boolean literal
     */
    public static Expression and(Collection<Expression> expressions) {
        if (expressions.size() == 1) {
            return expressions.iterator().next();
        }
        Set<Expression> distinctExpressions = Sets.newLinkedHashSetWithExpectedSize(expressions.size());
        for (Expression expression : expressions) {
            if (expression.equals(BooleanLiteral.FALSE)) {
                return BooleanLiteral.FALSE;
            } else if (!expression.equals(BooleanLiteral.TRUE)) {
                distinctExpressions.add(expression);
            }
        }

        List<Expression> exprList = Lists.newArrayList(distinctExpressions);
        if (exprList.isEmpty()) {
            return BooleanLiteral.TRUE;
        } else if (exprList.size() == 1) {
            return exprList.get(0);
        } else {
            return new And(exprList);
        }
    }

    /**
     *  AND expression, also remove duplicate expression, boolean literal
     */
    public static Expression and(Expression... expressions) {
        return and(Lists.newArrayList(expressions));
    }

    public static Optional<Expression> optionalOr(List<Expression> expressions) {
        if (expressions.isEmpty()) {
            return Optional.empty();
        } else {
            return Optional.of(ExpressionUtils.or(expressions));
        }
    }

    /**
     *  OR expression, also remove duplicate expression, boolean literal
     */
    public static Expression or(Expression... expressions) {
        return or(Lists.newArrayList(expressions));
    }

    /**
     *  OR expression, also remove duplicate expression, boolean literal
     */
    public static Expression or(Collection<Expression> expressions) {
        if (expressions.size() == 1) {
            return expressions.iterator().next();
        }
        Set<Expression> distinctExpressions = Sets.newLinkedHashSetWithExpectedSize(expressions.size());
        for (Expression expression : expressions) {
            if (expression.equals(BooleanLiteral.TRUE)) {
                return BooleanLiteral.TRUE;
            } else if (!expression.equals(BooleanLiteral.FALSE)) {
                distinctExpressions.add(expression);
            }
        }

        List<Expression> exprList = Lists.newArrayList(distinctExpressions);
        if (exprList.isEmpty()) {
            return BooleanLiteral.FALSE;
        } else if (exprList.size() == 1) {
            return exprList.get(0);
        } else {
            return new Or(exprList);
        }
    }

    public static Expression falseOrNull(Expression expression) {
        if (expression.nullable()) {
            return new And(new IsNull(expression), new NullLiteral(BooleanType.INSTANCE));
        } else {
            return BooleanLiteral.FALSE;
        }
    }

    public static Expression trueOrNull(Expression expression) {
        if (expression.nullable()) {
            return new Or(new Not(new IsNull(expression)), new NullLiteral(BooleanType.INSTANCE));
        } else {
            return BooleanLiteral.TRUE;
        }
    }

    public static Expression toInPredicateOrEqualTo(Expression reference, Collection<? extends Expression> values) {
        if (values.size() < 2) {
            return or(values.stream().map(value -> new EqualTo(reference, value)).collect(Collectors.toList()));
        } else {
            return new InPredicate(reference, ImmutableList.copyOf(values));
        }
    }

    public static Expression shuttleExpressionWithLineage(Expression expression, Plan plan, BitSet tableBitSet) {
        return shuttleExpressionWithLineage(Lists.newArrayList(expression),
                plan, ImmutableSet.of(), ImmutableSet.of(), tableBitSet).get(0);
    }

    public static List<? extends Expression> shuttleExpressionWithLineage(List<? extends Expression> expressions,
            Plan plan, BitSet tableBitSet) {
        return shuttleExpressionWithLineage(expressions, plan, ImmutableSet.of(), ImmutableSet.of(), tableBitSet);
    }

    /**
     * Replace the slot in expressions with the lineage identifier from specifiedbaseTable sets or target table types
     * example as following:
     * select a + 10 as a1, d from (
     * select b - 5 as a, d from table
     * );
     * op expression before is: a + 10 as a1, d. after is: b - 5 + 10, d
     * todo to get from plan struct info
     */
    public static List<? extends Expression> shuttleExpressionWithLineage(List<? extends Expression> expressions,
            Plan plan,
            Set<TableType> targetTypes,
            Set<String> tableIdentifiers,
            BitSet tableBitSet) {
        if (expressions.isEmpty()) {
            return ImmutableList.of();
        }
        ExpressionLineageReplacer.ExpressionReplaceContext replaceContext =
                new ExpressionLineageReplacer.ExpressionReplaceContext(
                        expressions.stream().map(Expression.class::cast).collect(Collectors.toList()),
                        targetTypes,
                        tableIdentifiers,
                        tableBitSet);

        plan.accept(ExpressionLineageReplacer.INSTANCE, replaceContext);
        // Replace expressions by expression map
        List<Expression> replacedExpressions = replaceContext.getReplacedExpressions();
        if (expressions.size() != replacedExpressions.size()) {
            throw new NereidsException("shuttle expression fail",
                    new MaterializedViewException("shuttle expression fail"));
        }
        return replacedExpressions;
    }

    /**
     * Choose the minimum slot from input parameter.
     */
    public static <S extends NamedExpression> S selectMinimumColumn(Collection<S> slots) {
        Preconditions.checkArgument(!slots.isEmpty());
        S minSlot = null;
        for (S slot : slots) {
            if (minSlot == null) {
                minSlot = slot;
            } else {
                int slotDataTypeWidth = slot.getDataType().width();
                if (slotDataTypeWidth < 0) {
                    continue;
                }
                minSlot = slotDataTypeWidth < minSlot.getDataType().width()
                        || minSlot.getDataType().width() <= 0 ? slot : minSlot;
            }
        }
        return minSlot;
    }

    /**
     * Check whether the input expression is a {@link org.apache.doris.nereids.trees.expressions.Slot}
     * or at least one {@link Cast} on a {@link org.apache.doris.nereids.trees.expressions.Slot}
     * <p>
     * for example:
     * - SlotReference to a column:
     * col
     * - Cast on SlotReference:
     * cast(int_col as string)
     * cast(cast(int_col as long) as string)
     *
     * @param expr input expression
     * @return Return Optional[ExprId] of underlying slot reference if input expression is a slot or cast on slot.
     *         Otherwise, return empty optional result.
     */
    public static Optional<ExprId> isSlotOrCastOnSlot(Expression expr) {
        return extractSlotOrCastOnSlot(expr).map(Slot::getExprId);
    }

    /**
     * Check whether the input expression is a {@link org.apache.doris.nereids.trees.expressions.Slot}
     * or at least one {@link Cast} on a {@link org.apache.doris.nereids.trees.expressions.Slot}
     */
    public static Optional<Slot> extractSlotOrCastOnSlot(Expression expr) {
        while (expr instanceof Cast) {
            expr = expr.child(0);
        }

        if (expr instanceof SlotReference) {
            return Optional.of((Slot) expr);
        } else {
            return Optional.empty();
        }
    }

    /**
     * Generate replaceMap Slot -> Expression from NamedExpression[Expression as name]
     */
    public static Map<Slot, Expression> generateReplaceMap(List<? extends NamedExpression> namedExpressions) {
        Map<Slot, Expression> replaceMap = Maps.newLinkedHashMapWithExpectedSize(namedExpressions.size());
        for (NamedExpression namedExpression : namedExpressions) {
            if (namedExpression instanceof Alias) {
                // Avoid cast to alias, retrieving the first child expression.
                Slot slot = namedExpression.toSlot();
                replaceMap.putIfAbsent(slot, namedExpression.child(0));
            }
        }
        return replaceMap;
    }

    /**
     * replace NameExpression.
     */
    public static NamedExpression replaceNameExpression(NamedExpression expr,
            Map<? extends Expression, ? extends Expression> replaceMap) {
        Expression newExpr = replace(expr, replaceMap);
        if (newExpr instanceof NamedExpression) {
            return (NamedExpression) newExpr;
        } else {
            return new Alias(expr.getExprId(), newExpr, expr.getName());
        }
    }

    /**
     * Replace expression node with predicate in the expression tree by `replaceMap` in top-down manner.
     */
    public static Expression replaceIf(Expression expr, Map<? extends Expression, ? extends Expression> replaceMap,
            Predicate<Expression> predicate) {
        return expr.rewriteDownShortCircuitDown(e -> {
            Expression replacedExpr = replaceMap.get(e);
            return replacedExpr == null ? e : replacedExpr;
        }, predicate);
    }

    /**
     * Replace expression node in the expression tree by `replaceMap` in top-down manner.
     * For example.
     * <pre>
     * input expression: a > 1
     * replaceMap: a -> b + c
     *
     * output:
     * b + c > 1
     * </pre>
     */
    public static Expression replace(Expression expr, Map<? extends Expression, ? extends Expression> replaceMap) {
        return expr.rewriteDownShortCircuit(e -> {
            Expression replacedExpr = replaceMap.get(e);
            return replacedExpr == null ? e : replacedExpr;
        });
    }

    /**
     * Replace expression node in the expression tree by `replaceMap` in top-down manner.
     * For example.
     * <pre>
     * input expression: a > 1
     * replaceMap: d -> b + c, transferMap: a -> d
     * firstly try to get mapping expression from replaceMap by a, if can not then
     * get mapping d from transferMap by a
     * and get mapping b + c from replaceMap by d
     * output:
     * b + c > 1
     * </pre>
     */
    public static Expression replace(Expression expr, Map<? extends Expression, ? extends Expression> replaceMap,
            Map<? extends Expression, ? extends Expression> transferMap) {
        return expr.rewriteDownShortCircuit(e -> {
            Expression replacedExpr = replaceMap.get(e);
            if (replacedExpr != null) {
                return replacedExpr;
            }
            replacedExpr = replaceMap.get(transferMap.get(e));
            return replacedExpr == null ? e : replacedExpr;
        });
    }

    public static List<Expression> replace(List<Expression> exprs,
            Map<? extends Expression, ? extends Expression> replaceMap) {
        ImmutableList.Builder<Expression> result = ImmutableList.builderWithExpectedSize(exprs.size());
        for (Expression expr : exprs) {
            result.add(replace(expr, replaceMap));
        }
        return result.build();
    }

    public static Set<Expression> replace(Set<Expression> exprs,
            Map<? extends Expression, ? extends Expression> replaceMap) {
        ImmutableSet.Builder<Expression> result = ImmutableSet.builderWithExpectedSize(exprs.size());
        for (Expression expr : exprs) {
            result.add(replace(expr, replaceMap));
        }
        return result.build();
    }

    /**
     * Replace expression node in the expression tree by `replaceMap` in top-down manner.
     */
    public static List<NamedExpression> replaceNamedExpressions(List<? extends NamedExpression> namedExpressions,
            Map<? extends Expression, ? extends Expression> replaceMap) {
        Builder<NamedExpression> replaceExprs = ImmutableList.builderWithExpectedSize(namedExpressions.size());
        for (NamedExpression namedExpression : namedExpressions) {
            NamedExpression newExpr = replaceNameExpression(namedExpression, replaceMap);
            if (newExpr.getExprId().equals(namedExpression.getExprId())) {
                replaceExprs.add(newExpr);
            } else {
                replaceExprs.add(new Alias(namedExpression.getExprId(), newExpr, namedExpression.getName()));
            }
        }
        return replaceExprs.build();
    }

    public static <E extends Expression> List<E> rewriteDownShortCircuit(
            Collection<E> exprs, Function<Expression, Expression> rewriteFunction) {
        ImmutableList.Builder<E> result = ImmutableList.builderWithExpectedSize(exprs.size());
        for (E expr : exprs) {
            result.add((E) expr.rewriteDownShortCircuit(rewriteFunction));
        }
        return result.build();
    }

    private static class ExpressionReplacer
            extends DefaultExpressionRewriter<Map<? extends Expression, ? extends Expression>> {
        public static final ExpressionReplacer INSTANCE = new ExpressionReplacer();

        private ExpressionReplacer() {
        }

        @Override
        public Expression visit(Expression expr, Map<? extends Expression, ? extends Expression> replaceMap) {
            if (replaceMap.containsKey(expr)) {
                return replaceMap.get(expr);
            }
            return super.visit(expr, replaceMap);
        }
    }

    /**
     * merge arguments into an expression array
     *
     * @param arguments instance of Expression or Expression Array
     * @return Expression Array
     */
    public static List<Expression> mergeArguments(Object... arguments) {
        Builder<Expression> builder = ImmutableList.builder();
        for (Object argument : arguments) {
            if (argument instanceof Expression[]) {
                builder.addAll(Arrays.asList((Expression[]) argument));
            } else {
                builder.add((Expression) argument);
            }
        }
        return builder.build();
    }

    /** isAllLiteral */
    public static boolean isAllLiteral(List<Expression> children) {
        for (Expression child : children) {
            if (!(child instanceof Literal)) {
                return false;
            }
        }
        return true;
    }

    /**
     * return true if all children are literal but not null literal.
     */
    public static boolean isAllNonNullComparableLiteral(List<Expression> children) {
        for (Expression child : children) {
            if ((!(child instanceof ComparableLiteral)) || (child instanceof NullLiteral)) {
                return false;
            }
        }
        return true;
    }

    /** matchNumericType */
    public static boolean matchNumericType(List<Expression> children) {
        for (Expression child : children) {
            if (!child.getDataType().isNumericType()) {
                return false;
            }
        }
        return true;
    }

    /** matchDateLikeType */
    public static boolean matchDateLikeType(List<Expression> children) {
        for (Expression child : children) {
            if (!child.getDataType().isDateLikeType()) {
                return false;
            }
        }
        return true;
    }

    /** hasNullLiteral */
    public static boolean hasNullLiteral(List<Expression> children) {
        for (Expression child : children) {
            if (child instanceof NullLiteral) {
                return true;
            }
        }
        return false;
    }

    /** hasOnlyMetricType */
    public static boolean hasOnlyMetricType(List<Expression> children) {
        for (Expression child : children) {
            if (child.getDataType().isOnlyMetricType()) {
                return true;
            }
        }
        return false;
    }

    /**
     * canInferNotNullForMarkSlot
     */
    public static boolean canInferNotNullForMarkSlot(Expression predicate, ExpressionRewriteContext ctx) {
        /*
         * assume predicate is from LogicalFilter
         * the idea is replacing each mark join slot with null and false literal then run FoldConstant rule
         * if the evaluate result are:
         * 1. all true
         * 2. all null and false (in logicalFilter, we discard both null and false values)
         * the mark slot can be non-nullable boolean
         * and in semi join, we can safely change the mark conjunct to hash conjunct
         */
        ImmutableList<Literal> literals =
                ImmutableList.of(new NullLiteral(BooleanType.INSTANCE), BooleanLiteral.FALSE);
        List<MarkJoinSlotReference> markJoinSlotReferenceList =
                new ArrayList<>((predicate.collect(MarkJoinSlotReference.class::isInstance)));
        int markSlotSize = markJoinSlotReferenceList.size();
        int maxMarkSlotCount = 4;
        // if the conjunct has mark slot, and maximum 4 mark slots(for performance)
        if (markSlotSize > 0 && markSlotSize <= maxMarkSlotCount) {
            Map<Expression, Expression> replaceMap = Maps.newHashMap();
            boolean meetTrue = false;
            boolean meetNullOrFalse = false;
            /*
             * markSlotSize = 1 -> loopCount = 2  ---- 0, 1
             * markSlotSize = 2 -> loopCount = 4  ---- 00, 01, 10, 11
             * markSlotSize = 3 -> loopCount = 8  ---- 000, 001, 010, 011, 100, 101, 110, 111
             * markSlotSize = 4 -> loopCount = 16 ---- 0000, 0001, ... 1111
             */
            int loopCount = 1 << markSlotSize;
            for (int i = 0; i < loopCount; ++i) {
                replaceMap.clear();
                /*
                 * replace each mark slot with null or false
                 * literals.get(0) -> NullLiteral(BooleanType.INSTANCE)
                 * literals.get(1) -> BooleanLiteral.FALSE
                 */
                for (int j = 0; j < markSlotSize; ++j) {
                    replaceMap.put(markJoinSlotReferenceList.get(j), literals.get((i >> j) & 1));
                }
                Expression evalResult = FoldConstantRule.evaluate(
                        ExpressionUtils.replace(predicate, replaceMap),
                        ctx
                );

                if (evalResult.equals(BooleanLiteral.TRUE)) {
                    if (meetNullOrFalse) {
                        return false;
                    } else {
                        meetTrue = true;
                    }
                } else if ((isNullOrFalse(evalResult))) {
                    if (meetTrue) {
                        return false;
                    } else {
                        meetNullOrFalse = true;
                    }
                } else {
                    return false;
                }
            }
            return true;
        }
        return false;
    }

    private static boolean isNullOrFalse(Expression expression) {
        return expression.isNullLiteral() || expression.equals(BooleanLiteral.FALSE);
    }

    /**
     * infer notNulls slot from predicate
     */
    public static Set<Slot> inferNotNullSlots(Set<Expression> predicates, CascadesContext cascadesContext) {
        ImmutableSet.Builder<Slot> notNullSlots = ImmutableSet.builderWithExpectedSize(predicates.size());
        for (Expression predicate : predicates) {
            for (Slot slot : predicate.getInputSlots()) {
                Map<Expression, Expression> replaceMap = new HashMap<>();
                Literal nullLiteral = new NullLiteral(slot.getDataType());
                replaceMap.put(slot, nullLiteral);
                Expression evalExpr = FoldConstantRule.evaluate(
                        ExpressionUtils.replace(predicate, replaceMap),
                        new ExpressionRewriteContext(cascadesContext)
                );
                if (evalExpr.isNullLiteral() || BooleanLiteral.FALSE.equals(evalExpr)) {
                    notNullSlots.add(slot);
                }
            }
        }
        return notNullSlots.build();
    }

    /**
     * infer notNulls slot from predicate
     */
    public static Set<Expression> inferNotNull(Set<Expression> predicates, CascadesContext cascadesContext) {
        ImmutableSet.Builder<Expression> newPredicates = ImmutableSet.builderWithExpectedSize(predicates.size());
        for (Slot slot : inferNotNullSlots(predicates, cascadesContext)) {
            newPredicates.add(new Not(new IsNull(slot), false));
        }
        return newPredicates.build();
    }

    /**
     * infer notNulls slot from predicate but these slots must be in the given slots.
     */
    public static Set<Expression> inferNotNull(Set<Expression> predicates, Set<Slot> slots,
            CascadesContext cascadesContext) {
        ImmutableSet.Builder<Expression> newPredicates = ImmutableSet.builderWithExpectedSize(predicates.size());
        for (Slot slot : inferNotNullSlots(predicates, cascadesContext)) {
            if (slots.contains(slot)) {
                newPredicates.add(new Not(new IsNull(slot), true));
            }
        }
        return newPredicates.build();
    }

    public static boolean isGeneratedNotNull(Expression expression) {
        return expression instanceof Not
                && ((Not) expression).isGeneratedIsNotNull()
                && ((Not) expression).child() instanceof IsNull;
    }

    /** flatExpressions */
    public static <E extends Expression> List<E> flatExpressions(List<List<E>> expressionLists) {
        int num = 0;
        for (List<E> expressionList : expressionLists) {
            num += expressionList.size();
        }

        ImmutableList.Builder<E> flatten = ImmutableList.builderWithExpectedSize(num);
        for (List<E> expressionList : expressionLists) {
            flatten.addAll(expressionList);
        }
        return flatten.build();
    }

    /** containsTypes */
    public static boolean containsTypes(
            Collection<? extends Expression> expressions, Collection<Class<? extends Expression>> types) {
        return containsTypes(expressions, types.toArray(new Class[0]));
    }

    /** containsTypes */
    public static boolean containsTypes(
            Collection<? extends Expression> expressions, Class<? extends Expression>... types) {
        if (types.length == 1) {
            int classId = SuperClassId.getClassId(types[0]);
            for (Expression expression : expressions) {
                if (expression.getAllChildrenTypes().get(classId)) {
                    return true;
                }
            }
        } else {
            BitSet typeIds = new BitSet();
            for (Class<?> type : types) {
                typeIds.set(SuperClassId.getClassId(type));
            }
            for (Expression expression : expressions) {
                if (expression.getAllChildrenTypes().intersects(typeIds)) {
                    return true;
                }
            }
        }
        return false;
    }

    /** allMatch */
    public static boolean allMatch(
            Collection<? extends Expression> expressions, Predicate<Expression> predicate) {
        for (Expression expression : expressions) {
            if (!predicate.test(expression)) {
                return false;
            }
        }
        return true;
    }

    /** anyMatch */
    public static boolean anyMatch(
            Collection<? extends Expression> expressions, Predicate<Expression> predicate) {
        for (Expression expression : expressions) {
            if (predicate.test(expression)) {
                return true;
            }
        }
        return false;
    }

    /** deapAnyMatch */
    public static boolean deapAnyMatch(
            Collection<? extends Expression> expressions, Predicate<TreeNode<Expression>> predicate) {
        for (Expression expression : expressions) {
            if (expression.anyMatch(expr -> expr.anyMatch(predicate))) {
                return true;
            }
        }
        return false;
    }

    /** deapNoneMatch */
    public static boolean deapNoneMatch(
            Collection<? extends Expression> expressions, Predicate<TreeNode<Expression>> predicate) {
        for (Expression expression : expressions) {
            if (expression.anyMatch(expr -> expr.anyMatch(predicate))) {
                return false;
            }
        }
        return true;
    }

    public static <E> Set<E> collect(Collection<? extends Expression> expressions,
            Predicate<TreeNode<Expression>> predicate) {
        ImmutableSet.Builder<E> set = ImmutableSet.builder();
        for (Expression expr : expressions) {
            set.addAll(expr.collectToList(predicate));
        }
        return set.build();
    }

    public static <E> List<E> collectToList(Collection<? extends Expression> expressions,
            Predicate<TreeNode<Expression>> predicate) {
        ImmutableList.Builder<E> list = ImmutableList.builder();
        for (Expression expr : expressions) {
            list.addAll(expr.collectToList(predicate));
        }
        return list.build();
    }

    /**
     * extract uniform slot for the given predicate, such as a = 1 and b = 2
     */
    public static ImmutableMap<Slot, Expression> extractUniformSlot(Expression expression) {
        ImmutableMap.Builder<Slot, Expression> builder = new ImmutableMap.Builder<>();
        if (expression instanceof And) {
            expression.children().forEach(child -> builder.putAll(extractUniformSlot(child)));
        }
        if (expression instanceof EqualTo) {
            if (isInjective(expression.child(0)) && expression.child(1).isConstant()) {
                builder.put((Slot) expression.child(0), expression.child(1));
            }
        }
        return builder.build();
    }

    // TODO: Add more injective functions
    public static boolean isInjective(Expression expression) {
        return expression instanceof Slot;
    }

    // if the input is unique,  the output of agg is unique, too
    public static boolean isInjectiveAgg(Expression agg) {
        return agg instanceof Sum || agg instanceof Avg || agg instanceof Max || agg instanceof Min;
    }

    public static <E> Set<E> mutableCollect(List<? extends Expression> expressions,
            Predicate<TreeNode<Expression>> predicate) {
        Set<E> set = new HashSet<>();
        for (Expression expr : expressions) {
            set.addAll(expr.collect(predicate));
        }
        return set;
    }

    /** collectAll */
    public static <E> List<E> collectAll(Collection<? extends Expression> expressions,
            Predicate<TreeNode<Expression>> predicate) {
        switch (expressions.size()) {
            case 0: return ImmutableList.of();
            default: {
                ImmutableList.Builder<E> result = ImmutableList.builder();
                for (Expression expr : expressions) {
                    result.addAll((Set) expr.collect(predicate));
                }
                return result.build();
            }
        }
    }

    public static List<List<Expression>> rollupToGroupingSets(List<Expression> rollupExpressions) {
        List<List<Expression>> groupingSets = Lists.newArrayList();
        for (int end = rollupExpressions.size(); end >= 0; --end) {
            groupingSets.add(rollupExpressions.subList(0, end));
        }
        return groupingSets;
    }

    /**
     * check and maybe commute for predications except not pred.
     */
    public static Optional<Expression> checkAndMaybeCommute(Expression expression) {
        if (expression instanceof Not) {
            return Optional.empty();
        }
        if (expression instanceof InPredicate) {
            InPredicate predicate = ((InPredicate) expression);
            if (!predicate.getCompareExpr().isSlot()
                    || predicate.getOptions().size() > Config.max_distribution_pruner_recursion_depth) {
                return Optional.empty();
            }
            return Optional.ofNullable(predicate.optionsAreLiterals() ? expression : null);
        } else if (expression instanceof ComparisonPredicate) {
            ComparisonPredicate predicate = ((ComparisonPredicate) expression);
            if (predicate.left() instanceof Literal) {
                predicate = predicate.commute();
            }
            return Optional.ofNullable(predicate.left().isSlot() && predicate.right().isLiteral() ? predicate : null);
        } else if (expression instanceof IsNull) {
            return Optional.ofNullable(((IsNull) expression).child().isSlot() ? expression : null);
        }
        return Optional.empty();
    }

    public static List<List<Expression>> cubeToGroupingSets(List<Expression> cubeExpressions) {
        List<List<Expression>> groupingSets = Lists.newArrayList();
        cubeToGroupingSets(cubeExpressions, 0, Lists.newArrayList(), groupingSets);
        return groupingSets;
    }

    private static void cubeToGroupingSets(List<Expression> cubeExpressions, int activeIndex,
            List<Expression> currentGroupingSet, List<List<Expression>> groupingSets) {
        if (activeIndex == cubeExpressions.size()) {
            groupingSets.add(currentGroupingSet);
            return;
        }

        // use current expression
        List<Expression> newCurrentGroupingSet = Lists.newArrayList(currentGroupingSet);
        newCurrentGroupingSet.add(cubeExpressions.get(activeIndex));
        cubeToGroupingSets(cubeExpressions, activeIndex + 1, newCurrentGroupingSet, groupingSets);

        // skip current expression
        cubeToGroupingSets(cubeExpressions, activeIndex + 1, currentGroupingSet, groupingSets);
    }

    /**
     * Get input slot set from list of expressions.
     */
    public static Set<Slot> getInputSlotSet(Collection<? extends Expression> exprs) {
        Set<Slot> set = new HashSet<>();
        for (Expression expr : exprs) {
            set.addAll(expr.getInputSlots());
        }
        return set;
    }

    public static Expression getExpressionCoveredByCast(Expression expression) {
        while (expression instanceof Cast) {
            expression = ((Cast) expression).child();
        }
        return expression;
    }

    /**
     * the expressions can be used as runtime filter targets
     */
    public static Expression getSingleNumericSlotOrExpressionCoveredByCast(Expression expression) {
        if (expression.getInputSlots().size() == 1) {
            Slot slot = expression.getInputSlots().iterator().next();
            if (slot.getDataType() instanceof NumericType) {
                return expression.getInputSlots().iterator().next();
            }
        }
        // for other datatype, only support cast.
        // example: T1 join T2 on subStr(T1.a, 1,4) = subStr(T2.a, 1,4)
        // the cost of subStr is too high, and hence we do not generate RF subStr(T2.a, 1,4)->subStr(T1.a, 1,4)
        while (expression instanceof Cast) {
            expression = ((Cast) expression).child();
        }
        return expression;
    }

    /**
     * To check whether a slot is constant after passing through a filter
     */
    public static boolean checkSlotConstant(Slot slot, Set<Expression> predicates) {
        return predicates.stream().anyMatch(predicate -> {
                    if (predicate instanceof EqualTo) {
                        EqualTo equalTo = (EqualTo) predicate;
                        return (equalTo.left() instanceof Literal && equalTo.right().equals(slot))
                                || (equalTo.right() instanceof Literal && equalTo.left().equals(slot));
                    }
                    return false;
                }
        );
    }

    /**
     * Check the expression is inferred or not, if inferred return true, nor return false
     */
    public static boolean isInferred(Expression expression) {
        return expression.accept(new DefaultExpressionVisitor<Boolean, Void>() {

            @Override
            public Boolean visit(Expression expr, Void context) {
                boolean inferred = expr.isInferred();
                if (expr.isInferred() || expr.children().isEmpty()) {
                    return inferred;
                }
                inferred = true;
                for (Expression child : expr.children()) {
                    inferred = inferred && child.accept(this, context);
                }
                return inferred;
            }
        }, null);
    }

    /** distinctSlotByName */
    public static List<Slot> distinctSlotByName(List<Slot> slots) {
        Set<String> existSlotNames = new HashSet<>(slots.size() * 2);
        Builder<Slot> distinctSlots = ImmutableList.builderWithExpectedSize(slots.size());
        for (Slot slot : slots) {
            String name = slot.getName();
            if (existSlotNames.add(name)) {
                distinctSlots.add(slot);
            }
        }
        return distinctSlots.build();
    }

    /** containsWindowExpression */
    public static boolean containsWindowExpression(List<NamedExpression> expressions) {
        for (NamedExpression expression : expressions) {
            if (expression.containsType(WindowExpression.class)) {
                return true;
            }
        }
        return false;
    }

    /** filter */
    public static <E extends Expression> List<E> filter(List<? extends Expression> expressions, Class<E> clazz) {
        ImmutableList.Builder<E> result = ImmutableList.builderWithExpectedSize(expressions.size());
        for (Expression expression : expressions) {
            if (clazz.isInstance(expression)) {
                result.add((E) expression);
            }
        }
        return result.build();
    }

    /** test whether unionConstExprs satisfy conjuncts */
    public static boolean unionConstExprsSatisfyConjuncts(LogicalUnion union, Set<Expression> conjuncts) {
        CascadesContext tempCascadeContext = CascadesContext.initContext(
                ConnectContext.get().getStatementContext(), union, PhysicalProperties.ANY);
        ExpressionRewriteContext rewriteContext = new ExpressionRewriteContext(tempCascadeContext);
        for (List<NamedExpression> constOutput : union.getConstantExprsList()) {
            Map<Expression, Expression> replaceMap = new HashMap<>();
            for (int i = 0; i < constOutput.size(); i++) {
                Expression output = constOutput.get(i);
                if (output instanceof Alias) {
                    replaceMap.put(union.getOutput().get(i), ((Alias) output).child());
                } else {
                    replaceMap.put(union.getOutput().get(i), output);
                }
            }
            for (Expression conjunct : conjuncts) {
                Expression res = FoldConstantRule.evaluate(ExpressionUtils.replace(conjunct, replaceMap),
                        rewriteContext);
                if (!res.equals(BooleanLiteral.TRUE)) {
                    return false;
                }
            }
        }
        return true;
    }

    /** check constant value the expression */
    public static Optional<Literal> checkConstantExpr(Expression expr, Optional<ExpressionRewriteContext> context) {
        if (expr instanceof Literal) {
            return Optional.of((Literal) expr);
        } else if (expr instanceof Alias) {
            return checkConstantExpr(((Alias) expr).child(), context);
        } else if (expr.isConstant() && context.isPresent()) {
            Expression evalExpr = FoldConstantRule.evaluate(expr, context.get());
            if (evalExpr instanceof Literal) {
                return Optional.of((Literal) evalExpr);
            }
        }

        return Optional.empty();
    }

    /** analyze the unbound expression and fold it to literal */
    public static Literal analyzeAndFoldToLiteral(ConnectContext ctx, Expression expression) throws UserException {
        Scope scope = new Scope(new ArrayList<>());
        LogicalEmptyRelation plan = new LogicalEmptyRelation(
                ConnectContext.get().getStatementContext().getNextRelationId(),
                new ArrayList<>());
        CascadesContext cascadesContext = CascadesContext.initContext(ctx.getStatementContext(), plan,
                PhysicalProperties.ANY);
        ExpressionAnalyzer analyzer = new ExpressionAnalyzer(null, scope, cascadesContext, false, false);
        Expression boundExpr = UnboundSlotRewriter.INSTANCE.rewrite(expression, null);
        Expression analyzedExpr;
        try {
            analyzedExpr = analyzer.analyze(boundExpr, new ExpressionRewriteContext(cascadesContext));
        } catch (AnalysisException e) {
            throw new UserException(expression + " must be constant value");
        }
        ExpressionRewriteContext context = new ExpressionRewriteContext(cascadesContext);
        ExpressionRuleExecutor executor = new ExpressionRuleExecutor(ImmutableList.of(
                ExpressionRewrite.bottomUp(ReplaceVariableByLiteral.INSTANCE)
        ));
        Expression rewrittenExpression = executor.rewrite(analyzedExpr, context);
        Expression foldExpression = FoldConstantRule.evaluate(rewrittenExpression, context);
        if (foldExpression instanceof Literal) {
            return (Literal) foldExpression;
        } else {
            throw new UserException(expression + " must be constant value");
        }
    }

    /**
     * mergeList
     */
    public static List<Expression> mergeList(List<Expression> list1, List<Expression> list2) {
        ImmutableList.Builder<Expression> builder = ImmutableList.builder();
        for (Expression expression : list1) {
            if (expression != null) {
                builder.add(expression);
            }
        }
        for (Expression expression : list2) {
            if (expression != null) {
                builder.add(expression);
            }
        }
        return builder.build();
    }

    private static class UnboundSlotRewriter extends DefaultExpressionRewriter<Void> {
        public static final UnboundSlotRewriter INSTANCE = new UnboundSlotRewriter();

        public Expression rewrite(Expression e, Void ctx) {
            return e.accept(this, ctx);
        }

        @Override
        public Expression visitUnboundSlot(UnboundSlot unboundSlot, Void ctx) {
            // set exec_mem_limit=21G, '21G' will be parsed as unbound slot
            // we need to rewrite it to String Literal '21G'
            return new StringLiteral(unboundSlot.getName());
        }
    }

    /**
     * format a list of slots
     */
    public static String slotListShapeInfo(List<Slot> materializedSlots) {
        StringBuilder shapeBuilder = new StringBuilder();
        shapeBuilder.append("(");
        boolean isFirst = true;
        for (Slot slot : materializedSlots) {
            if (isFirst) {
                shapeBuilder.append(slot.shapeInfo());
                isFirst = false;
            } else {
                shapeBuilder.append(",").append(slot.shapeInfo());
            }
        }
        shapeBuilder.append(")");
        return shapeBuilder.toString();
    }
}
