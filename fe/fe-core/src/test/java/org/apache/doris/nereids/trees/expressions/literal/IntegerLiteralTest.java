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

import org.apache.doris.nereids.trees.expressions.Expression;
import org.apache.doris.nereids.trees.expressions.literal.format.IntegerChecker;
import org.apache.doris.nereids.types.BooleanType;
import org.apache.doris.nereids.types.DateTimeV2Type;
import org.apache.doris.nereids.types.DateType;
import org.apache.doris.nereids.types.DoubleType;
import org.apache.doris.nereids.types.FloatType;

import org.junit.jupiter.api.Assertions;
import org.junit.jupiter.api.Test;

import java.math.BigInteger;
import java.util.function.Function;

public class IntegerLiteralTest {
    @Test
    public void testChecker() {
        assertValid(
                "1",
                "+1",
                "-1",
                "456"
        );

        assertInValid(
                "1.0",
                "1e3",
                "abc"
        );
    }

    private void assertValid(String...validString) {
        for (String str : validString) {
            check(str, s -> new IntegerLiteral(new BigInteger(s).intValueExact()));
        }
    }

    private void assertInValid(String...validString) {
        for (String str : validString) {
            Assertions.assertThrows(
                    Throwable.class,
                    () -> check(str, s -> new IntegerLiteral(new BigInteger(s).intValueExact()))
            );
        }
    }

    private <T extends IntegerLikeLiteral> T check(String str, Function<String, T> literalBuilder) {
        Assertions.assertTrue(IntegerChecker.isValidInteger(str), "Invalid integer: " + str);
        return literalBuilder.apply(str);
    }

    @Test
    void testUncheckedCastTo() {
        // To boolean
        IntegerLiteral d1 = new IntegerLiteral(0);
        Assertions.assertFalse(((BooleanLiteral) d1.uncheckedCastTo(BooleanType.INSTANCE)).getValue());
        d1 = new IntegerLiteral(2);
        Assertions.assertTrue(((BooleanLiteral) d1.uncheckedCastTo(BooleanType.INSTANCE)).getValue());
        d1 = new IntegerLiteral(-3);
        Assertions.assertTrue(((BooleanLiteral) d1.uncheckedCastTo(BooleanType.INSTANCE)).getValue());

        // To float
        Expression expression = d1.uncheckedCastTo(FloatType.INSTANCE);
        Assertions.assertInstanceOf(FloatLiteral.class, expression);
        Assertions.assertEquals((float) -3, ((FloatLiteral) expression).getValue());

        // To double
        expression = d1.uncheckedCastTo(DoubleType.INSTANCE);
        Assertions.assertInstanceOf(DoubleLiteral.class, expression);
        Assertions.assertEquals((float) -3, ((DoubleLiteral) expression).getValue());

        // To date
        d1 = new IntegerLiteral(1231);
        expression = d1.uncheckedCastTo(DateType.INSTANCE);
        Assertions.assertInstanceOf(DateLiteral.class, expression);
        Assertions.assertEquals(2000, ((DateLiteral) expression).year);
        Assertions.assertEquals(12, ((DateLiteral) expression).month);
        Assertions.assertEquals(31, ((DateLiteral) expression).day);

        d1 = new IntegerLiteral(91231);
        expression = d1.uncheckedCastTo(DateType.INSTANCE);
        Assertions.assertInstanceOf(DateLiteral.class, expression);
        Assertions.assertEquals(2009, ((DateLiteral) expression).year);
        Assertions.assertEquals(12, ((DateLiteral) expression).month);
        Assertions.assertEquals(31, ((DateLiteral) expression).day);

        d1 = new IntegerLiteral(701231);
        expression = d1.uncheckedCastTo(DateType.INSTANCE);
        Assertions.assertInstanceOf(DateLiteral.class, expression);
        Assertions.assertEquals(1970, ((DateLiteral) expression).year);
        Assertions.assertEquals(12, ((DateLiteral) expression).month);
        Assertions.assertEquals(31, ((DateLiteral) expression).day);

        // to datetime
        d1 = new IntegerLiteral(123);
        expression = d1.uncheckedCastTo(DateTimeV2Type.of(5));
        Assertions.assertInstanceOf(DateTimeV2Literal.class, expression);
        Assertions.assertEquals(2000, ((DateTimeV2Literal) expression).year);
        Assertions.assertEquals(1, ((DateTimeV2Literal) expression).month);
        Assertions.assertEquals(23, ((DateTimeV2Literal) expression).day);
        Assertions.assertEquals(0, ((DateTimeV2Literal) expression).hour);
        Assertions.assertEquals(0, ((DateTimeV2Literal) expression).minute);
        Assertions.assertEquals(0, ((DateTimeV2Literal) expression).second);
        Assertions.assertEquals(0, ((DateTimeV2Literal) expression).microSecond);

        d1 = new IntegerLiteral(1231);
        expression = d1.uncheckedCastTo(DateTimeV2Type.of(3));
        Assertions.assertInstanceOf(DateTimeV2Literal.class, expression);
        Assertions.assertEquals(2000, ((DateTimeV2Literal) expression).year);
        Assertions.assertEquals(12, ((DateTimeV2Literal) expression).month);
        Assertions.assertEquals(31, ((DateTimeV2Literal) expression).day);
        Assertions.assertEquals(0, ((DateTimeV2Literal) expression).hour);
        Assertions.assertEquals(0, ((DateTimeV2Literal) expression).minute);
        Assertions.assertEquals(0, ((DateTimeV2Literal) expression).second);
        Assertions.assertEquals(0, ((DateTimeV2Literal) expression).microSecond);

        d1 = new IntegerLiteral(31231);
        expression = d1.uncheckedCastTo(DateTimeV2Type.of(3));
        Assertions.assertInstanceOf(DateTimeV2Literal.class, expression);
        Assertions.assertEquals(2003, ((DateTimeV2Literal) expression).year);
        Assertions.assertEquals(12, ((DateTimeV2Literal) expression).month);
        Assertions.assertEquals(31, ((DateTimeV2Literal) expression).day);
        Assertions.assertEquals(0, ((DateTimeV2Literal) expression).hour);
        Assertions.assertEquals(0, ((DateTimeV2Literal) expression).minute);
        Assertions.assertEquals(0, ((DateTimeV2Literal) expression).second);
        Assertions.assertEquals(0, ((DateTimeV2Literal) expression).microSecond);

        d1 = new IntegerLiteral(701231);
        expression = d1.uncheckedCastTo(DateTimeV2Type.of(3));
        Assertions.assertInstanceOf(DateTimeV2Literal.class, expression);
        Assertions.assertEquals(1970, ((DateTimeV2Literal) expression).year);
        Assertions.assertEquals(12, ((DateTimeV2Literal) expression).month);
        Assertions.assertEquals(31, ((DateTimeV2Literal) expression).day);
        Assertions.assertEquals(0, ((DateTimeV2Literal) expression).hour);
        Assertions.assertEquals(0, ((DateTimeV2Literal) expression).minute);
        Assertions.assertEquals(0, ((DateTimeV2Literal) expression).second);
        Assertions.assertEquals(0, ((DateTimeV2Literal) expression).microSecond);

        d1 = new IntegerLiteral(691231);
        expression = d1.uncheckedCastTo(DateTimeV2Type.of(3));
        Assertions.assertInstanceOf(DateTimeV2Literal.class, expression);
        Assertions.assertEquals(2069, ((DateTimeV2Literal) expression).year);
        Assertions.assertEquals(12, ((DateTimeV2Literal) expression).month);
        Assertions.assertEquals(31, ((DateTimeV2Literal) expression).day);
        Assertions.assertEquals(0, ((DateTimeV2Literal) expression).hour);
        Assertions.assertEquals(0, ((DateTimeV2Literal) expression).minute);
        Assertions.assertEquals(0, ((DateTimeV2Literal) expression).second);
        Assertions.assertEquals(0, ((DateTimeV2Literal) expression).microSecond);
    }
}
