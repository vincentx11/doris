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


suite("test_cast_to_decimal64_10_10_from_decimal32") {

    // This test case is generated from the correspoinding be UT test case,
    // update this case if the correspoinding be UT test case is updated,
    // e.g.: ../run-be-ut.sh --run --filter=FunctionCastToDecimalTest.* --gen_regression_case
    sql "drop table if exists test_cast_to_decimal_10_10_from_decimal_1_0_0_nullable;"
    sql "create table test_cast_to_decimal_10_10_from_decimal_1_0_0_nullable(f1 int, f2 decimalv3(1, 0)) properties('replication_num'='1');"
    sql """insert into test_cast_to_decimal_10_10_from_decimal_1_0_0_nullable values (0, "0")
      ,(1, null);
    """

    sql "set enable_strict_cast=true;"
    qt_sql_0_strict 'select f1, cast(f2 as decimalv3(10, 10)) from test_cast_to_decimal_10_10_from_decimal_1_0_0_nullable order by 1;'

    sql "set enable_strict_cast=false;"
    qt_sql_0_non_strict 'select f1, cast(f2 as decimalv3(10, 10)) from test_cast_to_decimal_10_10_from_decimal_1_0_0_nullable order by 1;'

    sql "drop table if exists test_cast_to_decimal_10_10_from_decimal_1_0_0_not_nullable;"
    sql "create table test_cast_to_decimal_10_10_from_decimal_1_0_0_not_nullable(f1 int, f2 decimalv3(1, 0)) properties('replication_num'='1');"
    sql """insert into test_cast_to_decimal_10_10_from_decimal_1_0_0_not_nullable values (0, "0");
    """

    sql "set enable_strict_cast=true;"
    qt_sql_0_strict 'select f1, cast(f2 as decimalv3(10, 10)) from test_cast_to_decimal_10_10_from_decimal_1_0_0_not_nullable order by 1;'

    sql "set enable_strict_cast=false;"
    qt_sql_0_non_strict 'select f1, cast(f2 as decimalv3(10, 10)) from test_cast_to_decimal_10_10_from_decimal_1_0_0_not_nullable order by 1;'

    sql "drop table if exists test_cast_to_decimal_10_10_from_decimal_1_1_1_nullable;"
    sql "create table test_cast_to_decimal_10_10_from_decimal_1_1_1_nullable(f1 int, f2 decimalv3(1, 1)) properties('replication_num'='1');"
    sql """insert into test_cast_to_decimal_10_10_from_decimal_1_1_1_nullable values (0, "0.0"),(1, "0.1"),(2, "0.8"),(3, "0.9")
      ,(4, null);
    """

    sql "set enable_strict_cast=true;"
    qt_sql_1_strict 'select f1, cast(f2 as decimalv3(10, 10)) from test_cast_to_decimal_10_10_from_decimal_1_1_1_nullable order by 1;'

    sql "set enable_strict_cast=false;"
    qt_sql_1_non_strict 'select f1, cast(f2 as decimalv3(10, 10)) from test_cast_to_decimal_10_10_from_decimal_1_1_1_nullable order by 1;'

    sql "drop table if exists test_cast_to_decimal_10_10_from_decimal_1_1_1_not_nullable;"
    sql "create table test_cast_to_decimal_10_10_from_decimal_1_1_1_not_nullable(f1 int, f2 decimalv3(1, 1)) properties('replication_num'='1');"
    sql """insert into test_cast_to_decimal_10_10_from_decimal_1_1_1_not_nullable values (0, "0.0"),(1, "0.1"),(2, "0.8"),(3, "0.9");
    """

    sql "set enable_strict_cast=true;"
    qt_sql_1_strict 'select f1, cast(f2 as decimalv3(10, 10)) from test_cast_to_decimal_10_10_from_decimal_1_1_1_not_nullable order by 1;'

    sql "set enable_strict_cast=false;"
    qt_sql_1_non_strict 'select f1, cast(f2 as decimalv3(10, 10)) from test_cast_to_decimal_10_10_from_decimal_1_1_1_not_nullable order by 1;'

    sql "drop table if exists test_cast_to_decimal_10_10_from_decimal_9_0_2_nullable;"
    sql "create table test_cast_to_decimal_10_10_from_decimal_9_0_2_nullable(f1 int, f2 decimalv3(9, 0)) properties('replication_num'='1');"
    sql """insert into test_cast_to_decimal_10_10_from_decimal_9_0_2_nullable values (0, "0")
      ,(1, null);
    """

    sql "set enable_strict_cast=true;"
    qt_sql_2_strict 'select f1, cast(f2 as decimalv3(10, 10)) from test_cast_to_decimal_10_10_from_decimal_9_0_2_nullable order by 1;'

    sql "set enable_strict_cast=false;"
    qt_sql_2_non_strict 'select f1, cast(f2 as decimalv3(10, 10)) from test_cast_to_decimal_10_10_from_decimal_9_0_2_nullable order by 1;'

    sql "drop table if exists test_cast_to_decimal_10_10_from_decimal_9_0_2_not_nullable;"
    sql "create table test_cast_to_decimal_10_10_from_decimal_9_0_2_not_nullable(f1 int, f2 decimalv3(9, 0)) properties('replication_num'='1');"
    sql """insert into test_cast_to_decimal_10_10_from_decimal_9_0_2_not_nullable values (0, "0");
    """

    sql "set enable_strict_cast=true;"
    qt_sql_2_strict 'select f1, cast(f2 as decimalv3(10, 10)) from test_cast_to_decimal_10_10_from_decimal_9_0_2_not_nullable order by 1;'

    sql "set enable_strict_cast=false;"
    qt_sql_2_non_strict 'select f1, cast(f2 as decimalv3(10, 10)) from test_cast_to_decimal_10_10_from_decimal_9_0_2_not_nullable order by 1;'

    sql "drop table if exists test_cast_to_decimal_10_10_from_decimal_9_1_3_nullable;"
    sql "create table test_cast_to_decimal_10_10_from_decimal_9_1_3_nullable(f1 int, f2 decimalv3(9, 1)) properties('replication_num'='1');"
    sql """insert into test_cast_to_decimal_10_10_from_decimal_9_1_3_nullable values (0, "0.0"),(1, "0.1"),(2, "0.8"),(3, "0.9")
      ,(4, null);
    """

    sql "set enable_strict_cast=true;"
    qt_sql_3_strict 'select f1, cast(f2 as decimalv3(10, 10)) from test_cast_to_decimal_10_10_from_decimal_9_1_3_nullable order by 1;'

    sql "set enable_strict_cast=false;"
    qt_sql_3_non_strict 'select f1, cast(f2 as decimalv3(10, 10)) from test_cast_to_decimal_10_10_from_decimal_9_1_3_nullable order by 1;'

    sql "drop table if exists test_cast_to_decimal_10_10_from_decimal_9_1_3_not_nullable;"
    sql "create table test_cast_to_decimal_10_10_from_decimal_9_1_3_not_nullable(f1 int, f2 decimalv3(9, 1)) properties('replication_num'='1');"
    sql """insert into test_cast_to_decimal_10_10_from_decimal_9_1_3_not_nullable values (0, "0.0"),(1, "0.1"),(2, "0.8"),(3, "0.9");
    """

    sql "set enable_strict_cast=true;"
    qt_sql_3_strict 'select f1, cast(f2 as decimalv3(10, 10)) from test_cast_to_decimal_10_10_from_decimal_9_1_3_not_nullable order by 1;'

    sql "set enable_strict_cast=false;"
    qt_sql_3_non_strict 'select f1, cast(f2 as decimalv3(10, 10)) from test_cast_to_decimal_10_10_from_decimal_9_1_3_not_nullable order by 1;'

    sql "drop table if exists test_cast_to_decimal_10_10_from_decimal_9_8_4_nullable;"
    sql "create table test_cast_to_decimal_10_10_from_decimal_9_8_4_nullable(f1 int, f2 decimalv3(9, 8)) properties('replication_num'='1');"
    sql """insert into test_cast_to_decimal_10_10_from_decimal_9_8_4_nullable values (0, "0.00000000"),(1, "0.00000001"),(2, "0.00000009"),(3, "0.09999999"),(4, "0.90000000"),(5, "0.90000001"),(6, "0.99999998"),(7, "0.99999999")
      ,(8, null);
    """

    sql "set enable_strict_cast=true;"
    qt_sql_4_strict 'select f1, cast(f2 as decimalv3(10, 10)) from test_cast_to_decimal_10_10_from_decimal_9_8_4_nullable order by 1;'

    sql "set enable_strict_cast=false;"
    qt_sql_4_non_strict 'select f1, cast(f2 as decimalv3(10, 10)) from test_cast_to_decimal_10_10_from_decimal_9_8_4_nullable order by 1;'

    sql "drop table if exists test_cast_to_decimal_10_10_from_decimal_9_8_4_not_nullable;"
    sql "create table test_cast_to_decimal_10_10_from_decimal_9_8_4_not_nullable(f1 int, f2 decimalv3(9, 8)) properties('replication_num'='1');"
    sql """insert into test_cast_to_decimal_10_10_from_decimal_9_8_4_not_nullable values (0, "0.00000000"),(1, "0.00000001"),(2, "0.00000009"),(3, "0.09999999"),(4, "0.90000000"),(5, "0.90000001"),(6, "0.99999998"),(7, "0.99999999");
    """

    sql "set enable_strict_cast=true;"
    qt_sql_4_strict 'select f1, cast(f2 as decimalv3(10, 10)) from test_cast_to_decimal_10_10_from_decimal_9_8_4_not_nullable order by 1;'

    sql "set enable_strict_cast=false;"
    qt_sql_4_non_strict 'select f1, cast(f2 as decimalv3(10, 10)) from test_cast_to_decimal_10_10_from_decimal_9_8_4_not_nullable order by 1;'

    sql "drop table if exists test_cast_to_decimal_10_10_from_decimal_9_9_5_nullable;"
    sql "create table test_cast_to_decimal_10_10_from_decimal_9_9_5_nullable(f1 int, f2 decimalv3(9, 9)) properties('replication_num'='1');"
    sql """insert into test_cast_to_decimal_10_10_from_decimal_9_9_5_nullable values (0, "0.000000000"),(1, "0.000000001"),(2, "0.000000009"),(3, "0.099999999"),(4, "0.900000000"),(5, "0.900000001"),(6, "0.999999998"),(7, "0.999999999")
      ,(8, null);
    """

    sql "set enable_strict_cast=true;"
    qt_sql_5_strict 'select f1, cast(f2 as decimalv3(10, 10)) from test_cast_to_decimal_10_10_from_decimal_9_9_5_nullable order by 1;'

    sql "set enable_strict_cast=false;"
    qt_sql_5_non_strict 'select f1, cast(f2 as decimalv3(10, 10)) from test_cast_to_decimal_10_10_from_decimal_9_9_5_nullable order by 1;'

    sql "drop table if exists test_cast_to_decimal_10_10_from_decimal_9_9_5_not_nullable;"
    sql "create table test_cast_to_decimal_10_10_from_decimal_9_9_5_not_nullable(f1 int, f2 decimalv3(9, 9)) properties('replication_num'='1');"
    sql """insert into test_cast_to_decimal_10_10_from_decimal_9_9_5_not_nullable values (0, "0.000000000"),(1, "0.000000001"),(2, "0.000000009"),(3, "0.099999999"),(4, "0.900000000"),(5, "0.900000001"),(6, "0.999999998"),(7, "0.999999999");
    """

    sql "set enable_strict_cast=true;"
    qt_sql_5_strict 'select f1, cast(f2 as decimalv3(10, 10)) from test_cast_to_decimal_10_10_from_decimal_9_9_5_not_nullable order by 1;'

    sql "set enable_strict_cast=false;"
    qt_sql_5_non_strict 'select f1, cast(f2 as decimalv3(10, 10)) from test_cast_to_decimal_10_10_from_decimal_9_9_5_not_nullable order by 1;'

}