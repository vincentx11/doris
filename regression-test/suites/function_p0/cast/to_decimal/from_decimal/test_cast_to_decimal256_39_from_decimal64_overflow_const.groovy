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


suite("test_cast_to_decimal256_39_from_decimal64_overflow_const") {

    // This test case is generated from the correspoinding be UT test case,
    // update this case if the correspoinding be UT test case is updated,
    // e.g.: ../run-be-ut.sh --run --filter=FunctionCastToDecimalTest.* --gen_regression_case
    sql "set debug_skip_fold_constant = true;"
    sql "set enable_decimal256 = true;"
    sql "set debug_skip_fold_constant = true;"
    def test_cast_to_decimal_39_38_from_decimal_10_0_overflow_24_test_data = ["""10""","""9999999998""","""9999999999"""]
    sql "set enable_strict_cast=true;"

    for (b in ["false", "true"]) {
        sql """set debug_skip_fold_constant = "${b}";"""
        for (test_str in test_cast_to_decimal_39_38_from_decimal_10_0_overflow_24_test_data) {
            test {
                sql """select cast(cast("${test_str}" as decimalv3(10, 0)) as decimalv3(39, 38));"""
                exception ""
            }
        }
    }
    sql "set enable_strict_cast=false;"

    for (test_str in test_cast_to_decimal_39_38_from_decimal_10_0_overflow_24_test_data) {
        qt_sql_test_cast_to_decimal_39_38_from_decimal_10_0_overflow_24 """select cast(cast("${test_str}" as decimalv3(10, 0)) as decimalv3(39, 38));"""
        testFoldConst("""select cast(cast("${test_str}" as decimalv3(10, 0)) as decimalv3(39, 38));""")
    }
    sql "set debug_skip_fold_constant = true;"
    def test_cast_to_decimal_39_38_from_decimal_10_1_overflow_25_test_data = ["""10.9""","""999999998.9""","""999999999.9"""]
    sql "set enable_strict_cast=true;"

    for (b in ["false", "true"]) {
        sql """set debug_skip_fold_constant = "${b}";"""
        for (test_str in test_cast_to_decimal_39_38_from_decimal_10_1_overflow_25_test_data) {
            test {
                sql """select cast(cast("${test_str}" as decimalv3(10, 1)) as decimalv3(39, 38));"""
                exception ""
            }
        }
    }
    sql "set enable_strict_cast=false;"

    for (test_str in test_cast_to_decimal_39_38_from_decimal_10_1_overflow_25_test_data) {
        qt_sql_test_cast_to_decimal_39_38_from_decimal_10_1_overflow_25 """select cast(cast("${test_str}" as decimalv3(10, 1)) as decimalv3(39, 38));"""
        testFoldConst("""select cast(cast("${test_str}" as decimalv3(10, 1)) as decimalv3(39, 38));""")
    }
    sql "set debug_skip_fold_constant = true;"
    def test_cast_to_decimal_39_38_from_decimal_18_0_overflow_28_test_data = ["""10""","""999999999999999998""","""999999999999999999"""]
    sql "set enable_strict_cast=true;"

    for (b in ["false", "true"]) {
        sql """set debug_skip_fold_constant = "${b}";"""
        for (test_str in test_cast_to_decimal_39_38_from_decimal_18_0_overflow_28_test_data) {
            test {
                sql """select cast(cast("${test_str}" as decimalv3(18, 0)) as decimalv3(39, 38));"""
                exception ""
            }
        }
    }
    sql "set enable_strict_cast=false;"

    for (test_str in test_cast_to_decimal_39_38_from_decimal_18_0_overflow_28_test_data) {
        qt_sql_test_cast_to_decimal_39_38_from_decimal_18_0_overflow_28 """select cast(cast("${test_str}" as decimalv3(18, 0)) as decimalv3(39, 38));"""
        testFoldConst("""select cast(cast("${test_str}" as decimalv3(18, 0)) as decimalv3(39, 38));""")
    }
    sql "set debug_skip_fold_constant = true;"
    def test_cast_to_decimal_39_38_from_decimal_18_1_overflow_29_test_data = ["""10.9""","""99999999999999998.9""","""99999999999999999.9"""]
    sql "set enable_strict_cast=true;"

    for (b in ["false", "true"]) {
        sql """set debug_skip_fold_constant = "${b}";"""
        for (test_str in test_cast_to_decimal_39_38_from_decimal_18_1_overflow_29_test_data) {
            test {
                sql """select cast(cast("${test_str}" as decimalv3(18, 1)) as decimalv3(39, 38));"""
                exception ""
            }
        }
    }
    sql "set enable_strict_cast=false;"

    for (test_str in test_cast_to_decimal_39_38_from_decimal_18_1_overflow_29_test_data) {
        qt_sql_test_cast_to_decimal_39_38_from_decimal_18_1_overflow_29 """select cast(cast("${test_str}" as decimalv3(18, 1)) as decimalv3(39, 38));"""
        testFoldConst("""select cast(cast("${test_str}" as decimalv3(18, 1)) as decimalv3(39, 38));""")
    }
    sql "set debug_skip_fold_constant = true;"
    def test_cast_to_decimal_39_39_from_decimal_10_0_overflow_32_test_data = ["""1""","""9999999998""","""9999999999"""]
    sql "set enable_strict_cast=true;"

    for (b in ["false", "true"]) {
        sql """set debug_skip_fold_constant = "${b}";"""
        for (test_str in test_cast_to_decimal_39_39_from_decimal_10_0_overflow_32_test_data) {
            test {
                sql """select cast(cast("${test_str}" as decimalv3(10, 0)) as decimalv3(39, 39));"""
                exception ""
            }
        }
    }
    sql "set enable_strict_cast=false;"

    for (test_str in test_cast_to_decimal_39_39_from_decimal_10_0_overflow_32_test_data) {
        qt_sql_test_cast_to_decimal_39_39_from_decimal_10_0_overflow_32 """select cast(cast("${test_str}" as decimalv3(10, 0)) as decimalv3(39, 39));"""
        testFoldConst("""select cast(cast("${test_str}" as decimalv3(10, 0)) as decimalv3(39, 39));""")
    }
    sql "set debug_skip_fold_constant = true;"
    def test_cast_to_decimal_39_39_from_decimal_10_1_overflow_33_test_data = ["""1.9""","""999999998.9""","""999999999.9"""]
    sql "set enable_strict_cast=true;"

    for (b in ["false", "true"]) {
        sql """set debug_skip_fold_constant = "${b}";"""
        for (test_str in test_cast_to_decimal_39_39_from_decimal_10_1_overflow_33_test_data) {
            test {
                sql """select cast(cast("${test_str}" as decimalv3(10, 1)) as decimalv3(39, 39));"""
                exception ""
            }
        }
    }
    sql "set enable_strict_cast=false;"

    for (test_str in test_cast_to_decimal_39_39_from_decimal_10_1_overflow_33_test_data) {
        qt_sql_test_cast_to_decimal_39_39_from_decimal_10_1_overflow_33 """select cast(cast("${test_str}" as decimalv3(10, 1)) as decimalv3(39, 39));"""
        testFoldConst("""select cast(cast("${test_str}" as decimalv3(10, 1)) as decimalv3(39, 39));""")
    }
    sql "set debug_skip_fold_constant = true;"
    def test_cast_to_decimal_39_39_from_decimal_10_9_overflow_34_test_data = ["""1.999999999""","""8.999999999""","""9.999999999"""]
    sql "set enable_strict_cast=true;"

    for (b in ["false", "true"]) {
        sql """set debug_skip_fold_constant = "${b}";"""
        for (test_str in test_cast_to_decimal_39_39_from_decimal_10_9_overflow_34_test_data) {
            test {
                sql """select cast(cast("${test_str}" as decimalv3(10, 9)) as decimalv3(39, 39));"""
                exception ""
            }
        }
    }
    sql "set enable_strict_cast=false;"

    for (test_str in test_cast_to_decimal_39_39_from_decimal_10_9_overflow_34_test_data) {
        qt_sql_test_cast_to_decimal_39_39_from_decimal_10_9_overflow_34 """select cast(cast("${test_str}" as decimalv3(10, 9)) as decimalv3(39, 39));"""
        testFoldConst("""select cast(cast("${test_str}" as decimalv3(10, 9)) as decimalv3(39, 39));""")
    }
    sql "set debug_skip_fold_constant = true;"
    def test_cast_to_decimal_39_39_from_decimal_18_0_overflow_36_test_data = ["""1""","""999999999999999998""","""999999999999999999"""]
    sql "set enable_strict_cast=true;"

    for (b in ["false", "true"]) {
        sql """set debug_skip_fold_constant = "${b}";"""
        for (test_str in test_cast_to_decimal_39_39_from_decimal_18_0_overflow_36_test_data) {
            test {
                sql """select cast(cast("${test_str}" as decimalv3(18, 0)) as decimalv3(39, 39));"""
                exception ""
            }
        }
    }
    sql "set enable_strict_cast=false;"

    for (test_str in test_cast_to_decimal_39_39_from_decimal_18_0_overflow_36_test_data) {
        qt_sql_test_cast_to_decimal_39_39_from_decimal_18_0_overflow_36 """select cast(cast("${test_str}" as decimalv3(18, 0)) as decimalv3(39, 39));"""
        testFoldConst("""select cast(cast("${test_str}" as decimalv3(18, 0)) as decimalv3(39, 39));""")
    }
    sql "set debug_skip_fold_constant = true;"
    def test_cast_to_decimal_39_39_from_decimal_18_1_overflow_37_test_data = ["""1.9""","""99999999999999998.9""","""99999999999999999.9"""]
    sql "set enable_strict_cast=true;"

    for (b in ["false", "true"]) {
        sql """set debug_skip_fold_constant = "${b}";"""
        for (test_str in test_cast_to_decimal_39_39_from_decimal_18_1_overflow_37_test_data) {
            test {
                sql """select cast(cast("${test_str}" as decimalv3(18, 1)) as decimalv3(39, 39));"""
                exception ""
            }
        }
    }
    sql "set enable_strict_cast=false;"

    for (test_str in test_cast_to_decimal_39_39_from_decimal_18_1_overflow_37_test_data) {
        qt_sql_test_cast_to_decimal_39_39_from_decimal_18_1_overflow_37 """select cast(cast("${test_str}" as decimalv3(18, 1)) as decimalv3(39, 39));"""
        testFoldConst("""select cast(cast("${test_str}" as decimalv3(18, 1)) as decimalv3(39, 39));""")
    }
    sql "set debug_skip_fold_constant = true;"
    def test_cast_to_decimal_39_39_from_decimal_18_17_overflow_38_test_data = ["""1.99999999999999999""","""8.99999999999999999""","""9.99999999999999999"""]
    sql "set enable_strict_cast=true;"

    for (b in ["false", "true"]) {
        sql """set debug_skip_fold_constant = "${b}";"""
        for (test_str in test_cast_to_decimal_39_39_from_decimal_18_17_overflow_38_test_data) {
            test {
                sql """select cast(cast("${test_str}" as decimalv3(18, 17)) as decimalv3(39, 39));"""
                exception ""
            }
        }
    }
    sql "set enable_strict_cast=false;"

    for (test_str in test_cast_to_decimal_39_39_from_decimal_18_17_overflow_38_test_data) {
        qt_sql_test_cast_to_decimal_39_39_from_decimal_18_17_overflow_38 """select cast(cast("${test_str}" as decimalv3(18, 17)) as decimalv3(39, 39));"""
        testFoldConst("""select cast(cast("${test_str}" as decimalv3(18, 17)) as decimalv3(39, 39));""")
    }
}