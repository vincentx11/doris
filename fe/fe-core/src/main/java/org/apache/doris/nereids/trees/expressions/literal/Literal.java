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

package org.apache.doris.nereids.trees.expressions.literal;

import org.apache.doris.analysis.BoolLiteral;
import org.apache.doris.analysis.IntLiteral;
import org.apache.doris.analysis.LiteralExpr;
import org.apache.doris.catalog.MysqlColType;
import org.apache.doris.catalog.Type;
import org.apache.doris.common.Config;
import org.apache.doris.common.util.ByteBufferUtil;
import org.apache.doris.mysql.MysqlProto;
import org.apache.doris.nereids.exceptions.AnalysisException;
import org.apache.doris.nereids.exceptions.CastException;
import org.apache.doris.nereids.exceptions.UnboundException;
import org.apache.doris.nereids.trees.expressions.Expression;
import org.apache.doris.nereids.trees.expressions.shape.LeafExpression;
import org.apache.doris.nereids.trees.expressions.visitor.ExpressionVisitor;
import org.apache.doris.nereids.types.BigIntType;
import org.apache.doris.nereids.types.CharType;
import org.apache.doris.nereids.types.DataType;
import org.apache.doris.nereids.types.DateTimeType;
import org.apache.doris.nereids.types.DateTimeV2Type;
import org.apache.doris.nereids.types.DateType;
import org.apache.doris.nereids.types.DecimalV2Type;
import org.apache.doris.nereids.types.DecimalV3Type;
import org.apache.doris.nereids.types.DoubleType;
import org.apache.doris.nereids.types.IntegerType;
import org.apache.doris.nereids.types.LargeIntType;
import org.apache.doris.nereids.types.SmallIntType;
import org.apache.doris.nereids.types.StringType;
import org.apache.doris.nereids.types.TimeV2Type;
import org.apache.doris.nereids.types.TinyIntType;
import org.apache.doris.nereids.types.VarcharType;
import org.apache.doris.nereids.types.coercion.IntegralType;

import com.google.common.collect.ImmutableList;
import org.apache.log4j.Logger;

import java.math.BigDecimal;
import java.math.BigInteger;
import java.math.MathContext;
import java.math.RoundingMode;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.Locale;
import java.util.Objects;
import java.util.Optional;

/**
 * All data type literal expression in Nereids.
 * TODO: Increase the implementation of sub expression. such as Integer.
 */
public abstract class Literal extends Expression implements LeafExpression {

    private static final Logger logger = Logger.getLogger(Literal.class);
    protected final DataType dataType;

    /**
     * Constructor for Literal.
     *
     * @param dataType logical data type in Nereids
     */
    public Literal(DataType dataType) {
        super(ImmutableList.of());
        this.dataType = Objects.requireNonNull(dataType);
    }

    /**
     * Get literal according to value type
     */
    public static Literal of(Object value) {
        if (value == null) {
            return new NullLiteral();
        } else if (value instanceof Byte) {
            return new TinyIntLiteral((Byte) value);
        } else if (value instanceof Short) {
            return new SmallIntLiteral((Short) value);
        } else if (value instanceof Integer) {
            return new IntegerLiteral((Integer) value);
        } else if (value instanceof Long) {
            return new BigIntLiteral((Long) value);
        } else if (value instanceof BigInteger) {
            return new LargeIntLiteral((BigInteger) value);
        } else if (value instanceof Float) {
            return new FloatLiteral((Float) value);
        } else if (value instanceof Double) {
            return new DoubleLiteral((Double) value);
        } else if (value instanceof BigDecimal) {
            if (Config.enable_decimal_conversion) {
                return new DecimalV3Literal((BigDecimal) value);
            } else {
                return new DecimalLiteral((BigDecimal) value);
            }
        } else if (value instanceof Boolean) {
            return BooleanLiteral.of((Boolean) value);
        } else if (value instanceof String) {
            return new StringLiteral((String) value);
        } else {
            throw new RuntimeException();
        }
    }

    public abstract Object getValue();

    /**
     * Map literal to double, and keep "<=" order.
     * for numeric literal (int/long/double/float), directly convert to double
     * for char/varchar/string, we take first 8 chars as a int64, and convert it to double
     * for other literals, getDouble() is not used.
     * <p>
     * And hence, we could express the range of a datatype, and used in stats derive.
     * for example:
     * 'abcxxxxxxxxxxx' is between ('abb', 'zzz')
     *
     * @return double representation of literal.
     */
    public double getDouble() {
        try {
            return Double.parseDouble(getValue().toString());
        } catch (Exception e) {
            return 0.0;
        }
    }

    public String getStringValue() {
        return String.valueOf(getValue());
    }

    @Override
    public DataType getDataType() throws UnboundException {
        return dataType;
    }

    @Override
    public String computeToSql() {
        return toString();
    }

    @Override
    public String getExpressionName() {
        if (!this.exprName.isPresent()) {
            this.exprName = Optional.of("literal");
        }
        return this.exprName.get();
    }

    @Override
    public boolean nullable() throws UnboundException {
        return this instanceof NullLiteral;
    }

    @Override
    public <R, C> R accept(ExpressionVisitor<R, C> visitor, C context) {
        return visitor.visitLiteral(this, context);
    }

    /**
     * literal expr compare.
     */
    public Expression deprecatingCheckedCastTo(DataType targetType) throws AnalysisException {
        if (getDataType().isNumericType()) {
            String desc = getStringValue();
            BigDecimal val = new BigDecimal(desc);
            BigDecimal maxVal = val;
            BigDecimal minVal = val;
            if (targetType.isTinyIntType()) {
                maxVal = new BigDecimal(Byte.MAX_VALUE);
                minVal = new BigDecimal(Byte.MIN_VALUE);
            } else if (targetType.isSmallIntType()) {
                maxVal = new BigDecimal(Short.MAX_VALUE);
                minVal = new BigDecimal(Short.MIN_VALUE);
            } else if (targetType.isIntegerType()) {
                maxVal = new BigDecimal(Integer.MAX_VALUE);
                minVal = new BigDecimal(Integer.MIN_VALUE);
            } else if (targetType.isBigIntType()) {
                maxVal = new BigDecimal(Long.MAX_VALUE);
                minVal = new BigDecimal(Long.MIN_VALUE);
            } else if (targetType.isLargeIntType()) {
                maxVal = new BigDecimal(LargeIntType.MAX_VALUE);
                minVal = new BigDecimal(LargeIntType.MIN_VALUE);
            } else if (targetType.isFloatType()) {
                maxVal = new BigDecimal(Float.MAX_VALUE);
                minVal = BigDecimal.valueOf(-Float.MAX_VALUE);
            } else if (targetType.isDoubleType()) {
                maxVal = new BigDecimal(Double.MAX_VALUE);
                minVal = BigDecimal.valueOf(-Double.MAX_VALUE);
            }

            if (val.compareTo(maxVal) > 0 || val.compareTo(minVal) < 0) {
                throw new AnalysisException(
                        String.format("%s can't cast to %s", desc, targetType));
            }
        }
        return deprecatingUncheckedCastTo(targetType);
    }

    protected Expression deprecatingUncheckedCastTo(DataType targetType) throws AnalysisException {
        if (this.dataType.equals(targetType)) {
            return this;
        }
        if (this instanceof NullLiteral) {
            return new NullLiteral(targetType);
        }
        // TODO support string to complex
        String desc = getStringValue();
        // convert boolean to byte string value to support cast boolean to numeric in FE.
        if (this.equals(BooleanLiteral.TRUE)) {
            desc = "1";
        } else if (this.equals(BooleanLiteral.FALSE)) {
            desc = "0";
        }
        if (targetType.isBooleanType()) {
            try {
                // convert any non-zero numeric literal to true if target type is boolean
                long value = Long.parseLong(desc);
                if (value == 0) {
                    return Literal.of(false);
                } else {
                    return Literal.of(true);
                }
            } catch (Exception e) {
                // ignore
            }
            if ("0".equals(desc) || "false".equals(desc.toLowerCase(Locale.ROOT))) {
                return Literal.of(false);
            }
            if ("1".equals(desc) || "true".equals(desc.toLowerCase(Locale.ROOT))) {
                return Literal.of(true);
            }
        }
        if (targetType instanceof IntegralType) {
            // do trailing zeros to avoid number parse error when cast to integral type
            BigDecimal bigDecimal = new BigDecimal(desc);
            if (bigDecimal.stripTrailingZeros().scale() <= 0) {
                desc = bigDecimal.stripTrailingZeros().toPlainString();
            }
        }
        if (targetType.isTinyIntType()) {
            return Literal.of(Byte.valueOf(desc));
        } else if (targetType.isSmallIntType()) {
            return Literal.of(Short.valueOf(desc));
        } else if (targetType.isIntegerType()) {
            return Literal.of(Integer.valueOf(desc));
        } else if (targetType.isBigIntType()) {
            return Literal.of(Long.valueOf(desc));
        } else if (targetType.isLargeIntType()) {
            return Literal.of(new BigDecimal(desc).toBigInteger());
        } else if (targetType.isFloatType()) {
            return Literal.of(Double.valueOf(desc).floatValue());
        } else if (targetType.isDoubleType()) {
            return Literal.of(Double.parseDouble(desc));
        } else if (targetType.isCharType()) {
            if (((CharType) targetType).getLen() >= desc.length()) {
                return new CharLiteral(desc, ((CharType) targetType).getLen());
            }
        } else if (targetType.isVarcharType()) {
            if (this.dataType.isDoubleType() || this.dataType.isFloatType()) {
                int pointZeroIndex = findPointZeroIndex(desc);
                if (pointZeroIndex > -1) {
                    return new VarcharLiteral(desc.substring(0, pointZeroIndex), ((VarcharType) targetType).getLen());
                }
            }
            return new VarcharLiteral(desc, ((VarcharType) targetType).getLen());
        } else if (targetType instanceof StringType) {
            if (this.dataType.isDoubleType() || this.dataType.isFloatType()) {
                int pointZeroIndex = findPointZeroIndex(desc);
                if (pointZeroIndex > -1) {
                    return new StringLiteral(desc.substring(0, pointZeroIndex));
                }
            }
            return new StringLiteral(desc);
        } else if (targetType.isDateType()) {
            return new DateLiteral(desc);
        } else if (targetType.isDateTimeType()) {
            return new DateTimeLiteral(desc);
        } else if (targetType.isDecimalV2Type()) {
            return new DecimalLiteral((DecimalV2Type) targetType, new BigDecimal(desc));
        } else if (targetType.isDecimalV3Type()) {
            return new DecimalV3Literal((DecimalV3Type) targetType, new BigDecimal(desc));
        } else if (targetType.isDateV2Type()) {
            return new DateV2Literal(desc);
        } else if (targetType.isDateTimeV2Type()) {
            return new DateTimeV2Literal((DateTimeV2Type) targetType, desc);
        } else if (targetType.isJsonType()) {
            return new JsonLiteral(desc);
        } else if (targetType.isIPv4Type()) {
            return new IPv4Literal(desc);
        } else if (targetType.isIPv6Type()) {
            return new IPv6Literal(desc);
        } else if (targetType.isTimeType()) {
            if (this.dataType.isStringLikeType()) { // could parse in FE
                return new TimeV2Literal((TimeV2Type) targetType, desc);
            }
            throw new AnalysisException("cast to TimeType only in BE now");
        }
        throw new AnalysisException("cannot cast " + desc + " from type " + this.dataType + " to type " + targetType);
    }

    public Expression checkedCastWithFallback(DataType targetType) {
        try {
            return checkedCastTo(targetType);
        } catch (Exception e) {
            return deprecatingCheckedCastTo(targetType);
        }
    }

    public Expression uncheckedCastWithFallback(DataType targetType) {
        try {
            return uncheckedCastTo(targetType);
        } catch (Exception e) {
            return deprecatingUncheckedCastTo(targetType);
        }
    }

    /**
     * literal expr compare.
     */
    @Override
    public Expression checkedCastTo(DataType targetType) throws AnalysisException {
        if (getDataType().isNumericType()) {
            String desc = getStringValue();
            if (numericOverflow(desc, targetType)) {
                throw new CastException(String.format("%s can't cast to %s, overflow.", desc, targetType));
            }
        }
        return uncheckedCastTo(targetType);
    }

    protected boolean numericOverflow(String desc, DataType targetType) {
        if (this instanceof FloatLiteral || this instanceof DoubleLiteral) {
            if (DoubleLiteral.POS_INF_NAME.contains(desc.toLowerCase())
                    || DoubleLiteral.NEG_INF_NAME.contains(desc.toLowerCase())
                    || DoubleLiteral.NAN_NAME.contains(desc.toLowerCase())) {
                return false;
            }
        }
        BigDecimal val = new BigDecimal(desc);
        return numericOverflow(val, targetType);
    }

    protected boolean numericOverflow(BigDecimal value, DataType targetType) {
        BigDecimal maxVal = value;
        BigDecimal minVal = value;
        if (targetType.isTinyIntType()) {
            maxVal = new BigDecimal(Byte.MAX_VALUE);
            minVal = new BigDecimal(Byte.MIN_VALUE);
        } else if (targetType.isSmallIntType()) {
            maxVal = new BigDecimal(Short.MAX_VALUE);
            minVal = new BigDecimal(Short.MIN_VALUE);
        } else if (targetType.isIntegerType()) {
            maxVal = new BigDecimal(Integer.MAX_VALUE);
            minVal = new BigDecimal(Integer.MIN_VALUE);
        } else if (targetType.isBigIntType()) {
            maxVal = new BigDecimal(Long.MAX_VALUE);
            minVal = new BigDecimal(Long.MIN_VALUE);
        } else if (targetType.isLargeIntType()) {
            maxVal = new BigDecimal(LargeIntType.MAX_VALUE);
            minVal = new BigDecimal(LargeIntType.MIN_VALUE);
        }
        BigInteger integerValue = value.toBigInteger();
        return integerValue.compareTo(maxVal.toBigInteger()) > 0
                || integerValue.compareTo(minVal.toBigInteger()) < 0;
    }

    protected Expression getDecimalLiteral(BigDecimal bigDecimal, DataType targetType) {
        int pReal = bigDecimal.precision();
        int sReal = bigDecimal.scale();
        int pTarget = targetType.isDecimalV2Type()
                ? ((DecimalV2Type) targetType).getPrecision() : ((DecimalV3Type) targetType).getPrecision();
        int sTarget = targetType.isDecimalV2Type()
                ? ((DecimalV2Type) targetType).getScale() : ((DecimalV3Type) targetType).getScale();
        if (bigDecimal.compareTo(BigDecimal.ZERO) != 0 && pTarget - sTarget < pReal - sReal) {
            throw new CastException(String.format("%s can't cast to %s in strict mode.", getValue(), targetType));
        }
        BigDecimal result = bigDecimal.setScale(sTarget, RoundingMode.HALF_UP)
                .round(new MathContext(pTarget, RoundingMode.HALF_UP));
        logger.info("getDecimalLiteral orig bigDecimal: " + bigDecimal
                + ", targetType: " + targetType + ", result big decimal: " + result);
        if (targetType.isDecimalV2Type()) {
            return new DecimalLiteral((DecimalV2Type) targetType, result);
        } else {
            return new DecimalV3Literal((DecimalV3Type) targetType, result);
        }
    }

    @Override
    protected Expression uncheckedCastTo(DataType targetType) throws AnalysisException {
        if (this.dataType.equals(targetType)) {
            return this;
        }
        if (this instanceof NullLiteral) {
            return new NullLiteral(targetType);
        }
        if (targetType.isStringLikeType()) {
            return deprecatingUncheckedCastTo(targetType);
        }
        throw new AnalysisException(String.format("Cast from %s to %s not supported", this, targetType));
    }

    private static int findPointZeroIndex(String str) {
        int pointIndex = -1;
        for (int i = 0; i < str.length(); ++i) {
            char c = str.charAt(i);
            if (pointIndex > 0 && c != '0') {
                return -1;
            } else if (pointIndex == -1 && c == '.') {
                pointIndex = i;
            }
        }
        return pointIndex;
    }

    /** fromLegacyLiteral */
    public static Literal fromLegacyLiteral(LiteralExpr literalExpr, Type type) {
        DataType dataType = DataType.fromCatalogType(type);
        if (literalExpr instanceof org.apache.doris.analysis.MaxLiteral) {
            return new MaxLiteral(dataType);
        } else if (literalExpr instanceof org.apache.doris.analysis.NullLiteral) {
            return new NullLiteral(dataType);
        }
        switch (type.getPrimitiveType()) {
            case TINYINT: {
                IntLiteral intLiteral = (IntLiteral) literalExpr;
                return new TinyIntLiteral((byte) intLiteral.getValue());
            }
            case SMALLINT: {
                IntLiteral intLiteral = (IntLiteral) literalExpr;
                return new SmallIntLiteral((short) intLiteral.getValue());
            }
            case INT: {
                IntLiteral intLiteral = (IntLiteral) literalExpr;
                return new IntegerLiteral((int) intLiteral.getValue());
            }
            case BIGINT: {
                IntLiteral intLiteral = (IntLiteral) literalExpr;
                return new BigIntLiteral(intLiteral.getValue());
            }
            case LARGEINT: {
                org.apache.doris.analysis.LargeIntLiteral intLiteral
                        = (org.apache.doris.analysis.LargeIntLiteral) literalExpr;
                return new LargeIntLiteral(intLiteral.getRealValue());
            }
            case DATEV2: {
                org.apache.doris.analysis.DateLiteral dateLiteral = (org.apache.doris.analysis.DateLiteral) literalExpr;
                return new DateV2Literal(dateLiteral.getYear(), dateLiteral.getMonth(), dateLiteral.getDay());
            }
            case DATE: {
                org.apache.doris.analysis.DateLiteral dateLiteral = (org.apache.doris.analysis.DateLiteral) literalExpr;
                return new DateLiteral(dateLiteral.getYear(), dateLiteral.getMonth(), dateLiteral.getDay());
            }
            case DATETIME: {
                org.apache.doris.analysis.DateLiteral dateLiteral = (org.apache.doris.analysis.DateLiteral) literalExpr;
                return new DateTimeLiteral(
                        DateTimeType.INSTANCE, dateLiteral.getYear(), dateLiteral.getMonth(), dateLiteral.getDay(),
                        dateLiteral.getHour(), dateLiteral.getMinute(), dateLiteral.getSecond(),
                        dateLiteral.getMicrosecond()
                );
            }
            case DATETIMEV2: {
                org.apache.doris.analysis.DateLiteral dateLiteral = (org.apache.doris.analysis.DateLiteral) literalExpr;
                return new DateTimeV2Literal(
                        (DateTimeV2Type) DateType.fromCatalogType(type),
                        dateLiteral.getYear(), dateLiteral.getMonth(), dateLiteral.getDay(),
                        dateLiteral.getHour(), dateLiteral.getMinute(), dateLiteral.getSecond(),
                        dateLiteral.getMicrosecond()
                );
            }
            case BOOLEAN: {
                return ((BoolLiteral) literalExpr).getValue() ? BooleanLiteral.TRUE : BooleanLiteral.FALSE;
            }
            case CHAR: {
                return new CharLiteral(literalExpr.getStringValue(), ((CharType) dataType).getLen());
            }
            case VARCHAR: {
                return new VarcharLiteral(literalExpr.getStringValue(), ((VarcharType) dataType).getLen());
            }
            case STRING: {
                return new StringLiteral(literalExpr.getStringValue());
            }
            case FLOAT: {
                org.apache.doris.analysis.FloatLiteral floatLiteral
                        = (org.apache.doris.analysis.FloatLiteral) literalExpr;
                return new FloatLiteral((float) floatLiteral.getValue());
            }
            case DOUBLE: {
                org.apache.doris.analysis.FloatLiteral floatLiteral
                        = (org.apache.doris.analysis.FloatLiteral) literalExpr;
                return new DoubleLiteral(floatLiteral.getValue());
            }
            case DECIMALV2: {
                org.apache.doris.analysis.DecimalLiteral decimalLiteral
                        = (org.apache.doris.analysis.DecimalLiteral) literalExpr;
                BigDecimal clonedValue = decimalLiteral.getValue().add(BigDecimal.ZERO);
                return new DecimalLiteral((DecimalV2Type) dataType, clonedValue);
            }
            case DECIMAL32:
            case DECIMAL64:
            case DECIMAL128:
            case DECIMAL256: {
                org.apache.doris.analysis.DecimalLiteral decimalLiteral
                        = (org.apache.doris.analysis.DecimalLiteral) literalExpr;
                BigDecimal clonedValue = decimalLiteral.getValue().add(BigDecimal.ZERO);
                return new DecimalV3Literal((DecimalV3Type) dataType, clonedValue);
            }
            case JSONB: return new JsonLiteral(literalExpr.getStringValue());
            case IPV4: return new IPv4Literal(literalExpr.getStringValue());
            case IPV6: return new IPv6Literal(literalExpr.getStringValue());
            case TIMEV2: return new TimeV2Literal((TimeV2Type) dataType, literalExpr.getStringValue());
            default: {
                throw new AnalysisException("Unsupported convert the " + literalExpr.getType()
                        + " of legacy literal to nereids literal");
            }
        }
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (o == null || getClass() != o.getClass()) {
            return false;
        }
        Literal other = (Literal) o;
        return Objects.equals(getValue(), other.getValue());
    }

    @Override
    protected int computeHashCode() {
        return Objects.hashCode(getValue());
    }

    @Override
    public int fastChildrenHashCode() {
        return Objects.hashCode(getValue());
    }

    @Override
    public String toString() {
        return String.valueOf(getValue());
    }

    @Override
    public String getFingerprint() {
        return "?";
    }

    public abstract LiteralExpr toLegacyLiteral();

    public boolean isStringLikeLiteral() {
        return dataType.isStringLikeType();
    }

    /** whether is ZERO value **/
    public boolean isZero() {
        if (isNullLiteral()) {
            return false;
        }
        if (dataType.isTinyIntType()) {
            return getValue().equals((byte) 0);
        } else if (dataType.isSmallIntType()) {
            return getValue().equals((short) 0);
        } else if (dataType.isIntegerType()) {
            return getValue().equals(0);
        } else if (dataType.isBigIntType()) {
            return getValue().equals(0L);
        } else if (dataType.isLargeIntType()) {
            return getValue().equals(BigInteger.ZERO);
        } else if (dataType.isFloatType()) {
            return getValue().equals(0.0f);
        } else if (dataType.isDoubleType()) {
            return getValue().equals(0.0);
        } else if (dataType.isDecimalV2Type()) {
            return getValue().equals(BigDecimal.ZERO);
        } else if (dataType.isDecimalV3Type()) {
            return getValue().equals(BigDecimal.ZERO);
        }
        return false;
    }

    /**
    ** get paramter length, port from  mysql get_param_length
    **/
    public static int getParmLen(ByteBuffer data) {
        int maxLen = data.remaining();
        if (maxLen < 1) {
            return 0;
        }
        // get and advance 1 byte
        int len = MysqlProto.readInt1(data);
        if (len == 252) {
            if (maxLen < 3) {
                return 0;
            }
            // get and advance 2 bytes
            return MysqlProto.readInt2(data);
        } else if (len == 253) {
            if (maxLen < 4) {
                return 0;
            }
            // get and advance 3 bytes
            return MysqlProto.readInt3(data);
        } else if (len == 254) {
            /*
            In our client-server protocol all numbers bigger than 2^24
            stored as 8 bytes with uint8korr. Here we always know that
            parameter length is less than 2^4 so we don't look at the second
            4 bytes. But still we need to obey the protocol hence 9 in the
            assignment below.
            */
            if (maxLen < 9) {
                return 0;
            }
            len = MysqlProto.readInt4(data);
            MysqlProto.readFixedString(data, 4);
            return len;
        } else if (len == 255) {
            return 0;
        } else {
            return len;
        }
    }

    /**
     * Retrieves a Literal object based on the MySQL type and the data provided.
     *
     * @param mysqlType the MySQL type identifier
     * @param isUnsigned true if it is an unsigned type
     * @param data      the ByteBuffer containing the data
     * @return a Literal object corresponding to the MySQL type
     * @throws AnalysisException if the MySQL type is unsupported or if data conversion fails
     * @link  <a href="https://dev.mysql.com/doc/dev/mysql-server/latest/page_protocol_binary_resultset.html">...</a>.
     */
    public static Literal getLiteralByMysqlType(MysqlColType mysqlType, boolean isUnsigned, ByteBuffer data)
            throws AnalysisException {
        Literal literal = null;
        // If this is an unsigned numeric type, we convert it by using larger data types. For example, we can use
        // small int to represent unsigned tiny int (0-255), big int to represent unsigned ints (0-2 ^ 32-1),
        // and so on.
        switch (mysqlType) {
            case MYSQL_TYPE_TINY:
                literal = !isUnsigned
                    ? new TinyIntLiteral(data.get()) :
                        new SmallIntLiteral(ByteBufferUtil.getUnsignedByte(data));
                break;
            case MYSQL_TYPE_SHORT:
                literal = !isUnsigned
                    ? new SmallIntLiteral((short) data.getChar()) :
                        new IntegerLiteral(ByteBufferUtil.getUnsignedShort(data));
                break;
            case MYSQL_TYPE_LONG:
                literal = !isUnsigned
                    ? new IntegerLiteral(data.getInt()) :
                        new BigIntLiteral(ByteBufferUtil.getUnsignedInt(data));
                break;
            case MYSQL_TYPE_LONGLONG:
                literal = !isUnsigned
                    ? new BigIntLiteral(data.getLong()) :
                        new LargeIntLiteral(new BigInteger(Long.toUnsignedString(data.getLong())));
                break;
            case MYSQL_TYPE_FLOAT:
                literal = new FloatLiteral(data.getFloat());
                break;
            case MYSQL_TYPE_DOUBLE:
                literal = new DoubleLiteral(data.getDouble());
                break;
            case MYSQL_TYPE_DECIMAL:
            case MYSQL_TYPE_NEWDECIMAL:
                literal = handleDecimalLiteral(data);
                break;
            case MYSQL_TYPE_DATE:
                literal = handleDateLiteral(data);
                break;
            case MYSQL_TYPE_DATETIME:
            case MYSQL_TYPE_TIMESTAMP:
            case MYSQL_TYPE_TIMESTAMP2:
                literal = handleDateTimeLiteral(data);
                break;
            case MYSQL_TYPE_STRING:
            case MYSQL_TYPE_VARSTRING:
                literal = handleStringLiteral(data);
                break;
            case MYSQL_TYPE_VARCHAR:
                literal = handleVarcharLiteral(data);
                break;
            default:
                throw new AnalysisException("Unsupported MySQL type: " + mysqlType);
        }
        return literal;
    }

    private static Literal handleDecimalLiteral(ByteBuffer data) throws AnalysisException {
        int len = getParmLen(data);
        byte[] bytes = new byte[len];
        data.get(bytes);
        try {
            String value = new String(bytes);
            BigDecimal v = new BigDecimal(value);
            if (Config.enable_decimal_conversion) {
                return new DecimalV3Literal(v);
            }
            return new DecimalLiteral(v);
        } catch (NumberFormatException e) {
            throw new AnalysisException("Invalid decimal literal", e);
        }
    }

    private static Literal handleDateLiteral(ByteBuffer data) {
        int len = getParmLen(data);
        if (len >= 4) {
            int year = (int) data.getChar();
            int month = (int) data.get();
            int day = (int) data.get();
            if (Config.enable_date_conversion) {
                return new DateV2Literal(year, month, day);
            }
            return new DateLiteral(year, month, day);
        } else {
            if (Config.enable_date_conversion) {
                return new DateV2Literal(0, 1, 1);
            }
            return new DateLiteral(0, 1, 1);
        }
    }

    private static Literal handleDateTimeLiteral(ByteBuffer data) {
        int len = getParmLen(data);
        if (len >= 4) {
            int year = (int) data.getChar();
            int month = (int) data.get();
            int day = (int) data.get();
            int hour = 0;
            int minute = 0;
            int second = 0;
            int microsecond = 0;
            if (len > 4) {
                hour = (int) data.get();
                minute = (int) data.get();
                second = (int) data.get();
            }
            if (len > 7) {
                microsecond = data.getInt();
            }
            if (Config.enable_date_conversion) {
                return new DateTimeV2Literal(DateTimeV2Type.MAX, year, month, day, hour, minute, second, microsecond);
            }
            return new DateTimeLiteral(DateTimeType.INSTANCE, year, month, day, hour, minute, second, microsecond);
        } else {
            if (Config.enable_date_conversion) {
                return new DateTimeV2Literal(0, 1, 1, 0, 0, 0);
            }
            return new DateTimeLiteral(0, 1, 1, 0, 0, 0);
        }
    }

    private static Literal handleStringLiteral(ByteBuffer data) {
        int strLen = getParmLen(data);
        strLen = Math.min(strLen, data.remaining());
        byte[] bytes = new byte[strLen];
        data.get(bytes);
        // ATTN: use fixed StandardCharsets.UTF_8 to avoid unexpected charset in
        // different environment
        return new StringLiteral(new String(bytes, StandardCharsets.UTF_8));
    }

    private static Literal handleVarcharLiteral(ByteBuffer data) {
        int strLen = getParmLen(data);
        strLen = Math.min(strLen, data.remaining());
        byte[] bytes = new byte[strLen];
        data.get(bytes);
        // ATTN: use fixed StandardCharsets.UTF_8 to avoid unexpected charset in
        // different environment
        return new VarcharLiteral(new String(bytes, StandardCharsets.UTF_8));
    }

    /**convertToTypedLiteral*/
    public static Literal convertToTypedLiteral(Object value, DataType dataType) {
        Number number = (Number) value;
        if (dataType.equals(TinyIntType.INSTANCE)) {
            return new TinyIntLiteral(number.byteValue());
        } else if (dataType.equals(SmallIntType.INSTANCE)) {
            return new SmallIntLiteral(number.shortValue());
        } else if (dataType.equals(IntegerType.INSTANCE)) {
            return new IntegerLiteral(number.intValue());
        } else if (dataType.equals(BigIntType.INSTANCE)) {
            return new BigIntLiteral(number.longValue());
        } else if (dataType.equals(DoubleType.INSTANCE)) {
            return new DoubleLiteral(number.doubleValue());
        }
        return null;
    }
}
