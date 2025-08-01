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

package org.apache.doris.nereids.rules.analysis;

import org.apache.doris.datasource.hive.HMSExternalTable;
import org.apache.doris.nereids.analyzer.UnboundRelation;
import org.apache.doris.nereids.rules.Rule;
import org.apache.doris.nereids.rules.RuleType;
import org.apache.doris.nereids.trees.expressions.Expression;
import org.apache.doris.nereids.trees.plans.Plan;
import org.apache.doris.nereids.trees.plans.logical.LogicalAggregate;
import org.apache.doris.nereids.trees.plans.logical.LogicalCheckPolicy;
import org.apache.doris.nereids.trees.plans.logical.LogicalCheckPolicy.RelatedPolicy;
import org.apache.doris.nereids.trees.plans.logical.LogicalFilter;
import org.apache.doris.nereids.trees.plans.logical.LogicalHudiScan;
import org.apache.doris.nereids.trees.plans.logical.LogicalPlan;
import org.apache.doris.nereids.trees.plans.logical.LogicalProject;
import org.apache.doris.nereids.trees.plans.logical.LogicalRelation;
import org.apache.doris.nereids.trees.plans.logical.LogicalSubQueryAlias;
import org.apache.doris.nereids.trees.plans.logical.LogicalView;
import org.apache.doris.nereids.util.ExpressionUtils;

import com.google.common.collect.ImmutableList;

import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

/**
 * CheckPolicy.
 */
public class CheckPolicy implements AnalysisRuleFactory {

    @Override
    public List<Rule> buildRules() {
        return ImmutableList.of(
                RuleType.CHECK_ROW_POLICY.build(
                        logicalCheckPolicy(any().when(child -> !(child instanceof UnboundRelation))).thenApply(ctx -> {
                            LogicalCheckPolicy<Plan> checkPolicy = ctx.root;
                            LogicalFilter<Plan> upperFilter = null;
                            Plan upAgg = null;

                            Plan child = checkPolicy.child();
                            // Because the unique table will automatically include a filter condition
                            if ((child instanceof LogicalFilter)) {
                                upperFilter = (LogicalFilter) child;
                                if (child.child(0) instanceof LogicalRelation) {
                                    child = child.child(0);
                                } else if (child.child(0) instanceof LogicalAggregate
                                        && child.child(0).child(0) instanceof LogicalRelation) {
                                    upAgg = child.child(0);
                                    child = child.child(0).child(0);
                                }
                            }
                            if ((child instanceof LogicalAggregate) && child.child(0) instanceof LogicalRelation) {
                                upAgg = child;
                                child = child.child(0);
                            }
                            if (!(child instanceof LogicalRelation || isView(child))
                                    || ctx.connectContext.getSessionVariable().isPlayNereidsDump()) {
                                return ctx.root.child();
                            }
                            LogicalPlan relation = child instanceof LogicalSubQueryAlias ? (LogicalPlan) child.child(0)
                                    : (LogicalPlan) child;
                            Set<Expression> combineFilter = new LinkedHashSet<>();

                            // replace incremental params as AND expression
                            if (relation instanceof LogicalHudiScan) {
                                LogicalHudiScan hudiScan = (LogicalHudiScan) relation;
                                if (hudiScan.getTable() instanceof HMSExternalTable) {
                                    combineFilter.addAll(hudiScan.generateIncrementalExpression(
                                            hudiScan.getLogicalProperties().getOutput()));
                                }
                            }

                            RelatedPolicy relatedPolicy = checkPolicy.findPolicy(relation, ctx.cascadesContext);
                            relatedPolicy.rowPolicyFilter.ifPresent(expression -> combineFilter.addAll(
                                    ExpressionUtils.extractConjunctionToSet(expression)));
                            Plan result = upAgg != null ? upAgg.withChildren(child) : child;
                            if (upperFilter != null) {
                                combineFilter.addAll(upperFilter.getConjuncts());
                            }
                            if (!combineFilter.isEmpty()) {
                                result = new LogicalFilter<>(combineFilter, result);
                            }
                            if (relatedPolicy.dataMaskProjects.isPresent()) {
                                result = new LogicalProject<>(relatedPolicy.dataMaskProjects.get(), result);
                            }

                            return result;
                        })
                )
        );
    }

    // logicalView() or logicalSubQueryAlias(logicalView())
    private boolean isView(Plan plan) {
        if (plan instanceof LogicalView) {
            return true;
        }
        if (plan instanceof LogicalSubQueryAlias && plan.children().size() > 0 && plan.child(
                0) instanceof LogicalView) {
            return true;
        }
        return false;
    }
}
