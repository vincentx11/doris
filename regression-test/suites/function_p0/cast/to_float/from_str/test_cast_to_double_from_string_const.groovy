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


suite("test_cast_to_double_from_string_const") {

    // This test case is generated from the correspoinding be UT test case,
    // update this case if the correspoinding be UT test case is updated,
    // e.g.: ../run-be-ut.sh --run --filter=FunctionCastToDecimalTest.* --gen_regression_case
    sql "set debug_skip_fold_constant = true;"

    sql "set enable_strict_cast=true;"
    def const_sql_0_0 = """select "0", cast("0" as double);"""
    qt_sql_0_0_strict "${const_sql_0_0}"
    testFoldConst("${const_sql_0_0}")
    def const_sql_0_1 = """select "1", cast("1" as double);"""
    qt_sql_0_1_strict "${const_sql_0_1}"
    testFoldConst("${const_sql_0_1}")
    def const_sql_0_2 = """select "9", cast("9" as double);"""
    qt_sql_0_2_strict "${const_sql_0_2}"
    testFoldConst("${const_sql_0_2}")
    def const_sql_0_3 = """select "100000", cast("100000" as double);"""
    qt_sql_0_3_strict "${const_sql_0_3}"
    testFoldConst("${const_sql_0_3}")
    def const_sql_0_4 = """select "123456", cast("123456" as double);"""
    qt_sql_0_4_strict "${const_sql_0_4}"
    testFoldConst("${const_sql_0_4}")
    def const_sql_0_5 = """select "999999", cast("999999" as double);"""
    qt_sql_0_5_strict "${const_sql_0_5}"
    testFoldConst("${const_sql_0_5}")
    def const_sql_0_6 = """select "0.", cast("0." as double);"""
    qt_sql_0_6_strict "${const_sql_0_6}"
    testFoldConst("${const_sql_0_6}")
    def const_sql_0_7 = """select "1.", cast("1." as double);"""
    qt_sql_0_7_strict "${const_sql_0_7}"
    testFoldConst("${const_sql_0_7}")
    def const_sql_0_8 = """select "9.", cast("9." as double);"""
    qt_sql_0_8_strict "${const_sql_0_8}"
    testFoldConst("${const_sql_0_8}")
    def const_sql_0_9 = """select "100000.", cast("100000." as double);"""
    qt_sql_0_9_strict "${const_sql_0_9}"
    testFoldConst("${const_sql_0_9}")
    def const_sql_0_10 = """select "123456.", cast("123456." as double);"""
    qt_sql_0_10_strict "${const_sql_0_10}"
    testFoldConst("${const_sql_0_10}")
    def const_sql_0_11 = """select "999999.", cast("999999." as double);"""
    qt_sql_0_11_strict "${const_sql_0_11}"
    testFoldConst("${const_sql_0_11}")
    def const_sql_0_12 = """select ".0", cast(".0" as double);"""
    qt_sql_0_12_strict "${const_sql_0_12}"
    testFoldConst("${const_sql_0_12}")
    def const_sql_0_13 = """select ".1", cast(".1" as double);"""
    qt_sql_0_13_strict "${const_sql_0_13}"
    testFoldConst("${const_sql_0_13}")
    def const_sql_0_14 = """select ".9", cast(".9" as double);"""
    qt_sql_0_14_strict "${const_sql_0_14}"
    testFoldConst("${const_sql_0_14}")
    def const_sql_0_15 = """select ".000001", cast(".000001" as double);"""
    qt_sql_0_15_strict "${const_sql_0_15}"
    testFoldConst("${const_sql_0_15}")
    def const_sql_0_16 = """select ".000009", cast(".000009" as double);"""
    qt_sql_0_16_strict "${const_sql_0_16}"
    testFoldConst("${const_sql_0_16}")
    def const_sql_0_17 = """select ".100000", cast(".100000" as double);"""
    qt_sql_0_17_strict "${const_sql_0_17}"
    testFoldConst("${const_sql_0_17}")
    def const_sql_0_18 = """select ".900000", cast(".900000" as double);"""
    qt_sql_0_18_strict "${const_sql_0_18}"
    testFoldConst("${const_sql_0_18}")
    def const_sql_0_19 = """select ".100001", cast(".100001" as double);"""
    qt_sql_0_19_strict "${const_sql_0_19}"
    testFoldConst("${const_sql_0_19}")
    def const_sql_0_20 = """select ".900009", cast(".900009" as double);"""
    qt_sql_0_20_strict "${const_sql_0_20}"
    testFoldConst("${const_sql_0_20}")
    def const_sql_0_21 = """select "0.123456", cast("0.123456" as double);"""
    qt_sql_0_21_strict "${const_sql_0_21}"
    testFoldConst("${const_sql_0_21}")
    def const_sql_0_22 = """select ".999999", cast(".999999" as double);"""
    qt_sql_0_22_strict "${const_sql_0_22}"
    testFoldConst("${const_sql_0_22}")
    def const_sql_0_23 = """select "0.0", cast("0.0" as double);"""
    qt_sql_0_23_strict "${const_sql_0_23}"
    testFoldConst("${const_sql_0_23}")
    def const_sql_0_24 = """select "1.0", cast("1.0" as double);"""
    qt_sql_0_24_strict "${const_sql_0_24}"
    testFoldConst("${const_sql_0_24}")
    def const_sql_0_25 = """select "9.0", cast("9.0" as double);"""
    qt_sql_0_25_strict "${const_sql_0_25}"
    testFoldConst("${const_sql_0_25}")
    def const_sql_0_26 = """select "100000.0", cast("100000.0" as double);"""
    qt_sql_0_26_strict "${const_sql_0_26}"
    testFoldConst("${const_sql_0_26}")
    def const_sql_0_27 = """select "999999.0", cast("999999.0" as double);"""
    qt_sql_0_27_strict "${const_sql_0_27}"
    testFoldConst("${const_sql_0_27}")
    def const_sql_0_28 = """select "123456.0", cast("123456.0" as double);"""
    qt_sql_0_28_strict "${const_sql_0_28}"
    testFoldConst("${const_sql_0_28}")
    def const_sql_0_29 = """select "0.000000", cast("0.000000" as double);"""
    qt_sql_0_29_strict "${const_sql_0_29}"
    testFoldConst("${const_sql_0_29}")
    def const_sql_0_30 = """select "0.000001", cast("0.000001" as double);"""
    qt_sql_0_30_strict "${const_sql_0_30}"
    testFoldConst("${const_sql_0_30}")
    def const_sql_0_31 = """select "0.000009", cast("0.000009" as double);"""
    qt_sql_0_31_strict "${const_sql_0_31}"
    testFoldConst("${const_sql_0_31}")
    def const_sql_0_32 = """select "0.100001", cast("0.100001" as double);"""
    qt_sql_0_32_strict "${const_sql_0_32}"
    testFoldConst("${const_sql_0_32}")
    def const_sql_0_33 = """select "0.900009", cast("0.900009" as double);"""
    qt_sql_0_33_strict "${const_sql_0_33}"
    testFoldConst("${const_sql_0_33}")
    def const_sql_0_34 = """select "1.000000", cast("1.000000" as double);"""
    qt_sql_0_34_strict "${const_sql_0_34}"
    testFoldConst("${const_sql_0_34}")
    def const_sql_0_35 = """select "9.000000", cast("9.000000" as double);"""
    qt_sql_0_35_strict "${const_sql_0_35}"
    testFoldConst("${const_sql_0_35}")
    def const_sql_0_36 = """select "1.00001", cast("1.00001" as double);"""
    qt_sql_0_36_strict "${const_sql_0_36}"
    testFoldConst("${const_sql_0_36}")
    def const_sql_0_37 = """select "9.00001", cast("9.00001" as double);"""
    qt_sql_0_37_strict "${const_sql_0_37}"
    testFoldConst("${const_sql_0_37}")
    def const_sql_0_38 = """select "100000.000000", cast("100000.000000" as double);"""
    qt_sql_0_38_strict "${const_sql_0_38}"
    testFoldConst("${const_sql_0_38}")
    def const_sql_0_39 = """select "100001.000000", cast("100001.000000" as double);"""
    qt_sql_0_39_strict "${const_sql_0_39}"
    testFoldConst("${const_sql_0_39}")
    def const_sql_0_40 = """select "999999.000000", cast("999999.000000" as double);"""
    qt_sql_0_40_strict "${const_sql_0_40}"
    testFoldConst("${const_sql_0_40}")
    def const_sql_0_41 = """select "123456.000000", cast("123456.000000" as double);"""
    qt_sql_0_41_strict "${const_sql_0_41}"
    testFoldConst("${const_sql_0_41}")
    def const_sql_0_42 = """select "123.456", cast("123.456" as double);"""
    qt_sql_0_42_strict "${const_sql_0_42}"
    testFoldConst("${const_sql_0_42}")
    def const_sql_0_43 = """select "999.999", cast("999.999" as double);"""
    qt_sql_0_43_strict "${const_sql_0_43}"
    testFoldConst("${const_sql_0_43}")
    def const_sql_0_44 = """select "nan", cast("nan" as double);"""
    qt_sql_0_44_strict "${const_sql_0_44}"
    testFoldConst("${const_sql_0_44}")
    def const_sql_0_45 = """select "NaN", cast("NaN" as double);"""
    qt_sql_0_45_strict "${const_sql_0_45}"
    testFoldConst("${const_sql_0_45}")
    def const_sql_0_46 = """select "NAN", cast("NAN" as double);"""
    qt_sql_0_46_strict "${const_sql_0_46}"
    testFoldConst("${const_sql_0_46}")
    def const_sql_0_47 = """select "inf", cast("inf" as double);"""
    qt_sql_0_47_strict "${const_sql_0_47}"
    testFoldConst("${const_sql_0_47}")
    def const_sql_0_48 = """select "Inf", cast("Inf" as double);"""
    qt_sql_0_48_strict "${const_sql_0_48}"
    testFoldConst("${const_sql_0_48}")
    def const_sql_0_49 = """select "INF", cast("INF" as double);"""
    qt_sql_0_49_strict "${const_sql_0_49}"
    testFoldConst("${const_sql_0_49}")
    def const_sql_0_50 = """select "infinity", cast("infinity" as double);"""
    qt_sql_0_50_strict "${const_sql_0_50}"
    testFoldConst("${const_sql_0_50}")
    def const_sql_0_51 = """select "Infinity", cast("Infinity" as double);"""
    qt_sql_0_51_strict "${const_sql_0_51}"
    testFoldConst("${const_sql_0_51}")
    def const_sql_0_52 = """select "INFINITY", cast("INFINITY" as double);"""
    qt_sql_0_52_strict "${const_sql_0_52}"
    testFoldConst("${const_sql_0_52}")
    def const_sql_0_53 = """select "1.7976931348623157e+308", cast("1.7976931348623157e+308" as double);"""
    qt_sql_0_53_strict "${const_sql_0_53}"
    testFoldConst("${const_sql_0_53}")
    def const_sql_0_54 = """select "2.2250738585072014e-308", cast("2.2250738585072014e-308" as double);"""
    qt_sql_0_54_strict "${const_sql_0_54}"
    testFoldConst("${const_sql_0_54}")
    def const_sql_0_55 = """select "-0", cast("-0" as double);"""
    qt_sql_0_55_strict "${const_sql_0_55}"
    testFoldConst("${const_sql_0_55}")
    def const_sql_0_56 = """select "-1", cast("-1" as double);"""
    qt_sql_0_56_strict "${const_sql_0_56}"
    testFoldConst("${const_sql_0_56}")
    def const_sql_0_57 = """select "-9", cast("-9" as double);"""
    qt_sql_0_57_strict "${const_sql_0_57}"
    testFoldConst("${const_sql_0_57}")
    def const_sql_0_58 = """select "-100000", cast("-100000" as double);"""
    qt_sql_0_58_strict "${const_sql_0_58}"
    testFoldConst("${const_sql_0_58}")
    def const_sql_0_59 = """select "-123456", cast("-123456" as double);"""
    qt_sql_0_59_strict "${const_sql_0_59}"
    testFoldConst("${const_sql_0_59}")
    def const_sql_0_60 = """select "-999999", cast("-999999" as double);"""
    qt_sql_0_60_strict "${const_sql_0_60}"
    testFoldConst("${const_sql_0_60}")
    def const_sql_0_61 = """select "-0.", cast("-0." as double);"""
    qt_sql_0_61_strict "${const_sql_0_61}"
    testFoldConst("${const_sql_0_61}")
    def const_sql_0_62 = """select "-1.", cast("-1." as double);"""
    qt_sql_0_62_strict "${const_sql_0_62}"
    testFoldConst("${const_sql_0_62}")
    def const_sql_0_63 = """select "-9.", cast("-9." as double);"""
    qt_sql_0_63_strict "${const_sql_0_63}"
    testFoldConst("${const_sql_0_63}")
    def const_sql_0_64 = """select "-100000.", cast("-100000." as double);"""
    qt_sql_0_64_strict "${const_sql_0_64}"
    testFoldConst("${const_sql_0_64}")
    def const_sql_0_65 = """select "-123456.", cast("-123456." as double);"""
    qt_sql_0_65_strict "${const_sql_0_65}"
    testFoldConst("${const_sql_0_65}")
    def const_sql_0_66 = """select "-999999.", cast("-999999." as double);"""
    qt_sql_0_66_strict "${const_sql_0_66}"
    testFoldConst("${const_sql_0_66}")
    def const_sql_0_67 = """select "-.0", cast("-.0" as double);"""
    qt_sql_0_67_strict "${const_sql_0_67}"
    testFoldConst("${const_sql_0_67}")
    def const_sql_0_68 = """select "-.1", cast("-.1" as double);"""
    qt_sql_0_68_strict "${const_sql_0_68}"
    testFoldConst("${const_sql_0_68}")
    def const_sql_0_69 = """select "-.9", cast("-.9" as double);"""
    qt_sql_0_69_strict "${const_sql_0_69}"
    testFoldConst("${const_sql_0_69}")
    def const_sql_0_70 = """select "-.000001", cast("-.000001" as double);"""
    qt_sql_0_70_strict "${const_sql_0_70}"
    testFoldConst("${const_sql_0_70}")
    def const_sql_0_71 = """select "-.000009", cast("-.000009" as double);"""
    qt_sql_0_71_strict "${const_sql_0_71}"
    testFoldConst("${const_sql_0_71}")
    def const_sql_0_72 = """select "-.100000", cast("-.100000" as double);"""
    qt_sql_0_72_strict "${const_sql_0_72}"
    testFoldConst("${const_sql_0_72}")
    def const_sql_0_73 = """select "-.900000", cast("-.900000" as double);"""
    qt_sql_0_73_strict "${const_sql_0_73}"
    testFoldConst("${const_sql_0_73}")
    def const_sql_0_74 = """select "-.100001", cast("-.100001" as double);"""
    qt_sql_0_74_strict "${const_sql_0_74}"
    testFoldConst("${const_sql_0_74}")
    def const_sql_0_75 = """select "-.900009", cast("-.900009" as double);"""
    qt_sql_0_75_strict "${const_sql_0_75}"
    testFoldConst("${const_sql_0_75}")
    def const_sql_0_76 = """select "-0.123456", cast("-0.123456" as double);"""
    qt_sql_0_76_strict "${const_sql_0_76}"
    testFoldConst("${const_sql_0_76}")
    def const_sql_0_77 = """select "-.999999", cast("-.999999" as double);"""
    qt_sql_0_77_strict "${const_sql_0_77}"
    testFoldConst("${const_sql_0_77}")
    def const_sql_0_78 = """select "-0.0", cast("-0.0" as double);"""
    qt_sql_0_78_strict "${const_sql_0_78}"
    testFoldConst("${const_sql_0_78}")
    def const_sql_0_79 = """select "-1.0", cast("-1.0" as double);"""
    qt_sql_0_79_strict "${const_sql_0_79}"
    testFoldConst("${const_sql_0_79}")
    def const_sql_0_80 = """select "-9.0", cast("-9.0" as double);"""
    qt_sql_0_80_strict "${const_sql_0_80}"
    testFoldConst("${const_sql_0_80}")
    def const_sql_0_81 = """select "-100000.0", cast("-100000.0" as double);"""
    qt_sql_0_81_strict "${const_sql_0_81}"
    testFoldConst("${const_sql_0_81}")
    def const_sql_0_82 = """select "-999999.0", cast("-999999.0" as double);"""
    qt_sql_0_82_strict "${const_sql_0_82}"
    testFoldConst("${const_sql_0_82}")
    def const_sql_0_83 = """select "-123456.0", cast("-123456.0" as double);"""
    qt_sql_0_83_strict "${const_sql_0_83}"
    testFoldConst("${const_sql_0_83}")
    def const_sql_0_84 = """select "-0.000000", cast("-0.000000" as double);"""
    qt_sql_0_84_strict "${const_sql_0_84}"
    testFoldConst("${const_sql_0_84}")
    def const_sql_0_85 = """select "-0.000001", cast("-0.000001" as double);"""
    qt_sql_0_85_strict "${const_sql_0_85}"
    testFoldConst("${const_sql_0_85}")
    def const_sql_0_86 = """select "-0.000009", cast("-0.000009" as double);"""
    qt_sql_0_86_strict "${const_sql_0_86}"
    testFoldConst("${const_sql_0_86}")
    def const_sql_0_87 = """select "-0.100001", cast("-0.100001" as double);"""
    qt_sql_0_87_strict "${const_sql_0_87}"
    testFoldConst("${const_sql_0_87}")
    def const_sql_0_88 = """select "-0.900009", cast("-0.900009" as double);"""
    qt_sql_0_88_strict "${const_sql_0_88}"
    testFoldConst("${const_sql_0_88}")
    def const_sql_0_89 = """select "-1.000000", cast("-1.000000" as double);"""
    qt_sql_0_89_strict "${const_sql_0_89}"
    testFoldConst("${const_sql_0_89}")
    def const_sql_0_90 = """select "-9.000000", cast("-9.000000" as double);"""
    qt_sql_0_90_strict "${const_sql_0_90}"
    testFoldConst("${const_sql_0_90}")
    def const_sql_0_91 = """select "-1.00001", cast("-1.00001" as double);"""
    qt_sql_0_91_strict "${const_sql_0_91}"
    testFoldConst("${const_sql_0_91}")
    def const_sql_0_92 = """select "-9.00001", cast("-9.00001" as double);"""
    qt_sql_0_92_strict "${const_sql_0_92}"
    testFoldConst("${const_sql_0_92}")
    def const_sql_0_93 = """select "-100000.000000", cast("-100000.000000" as double);"""
    qt_sql_0_93_strict "${const_sql_0_93}"
    testFoldConst("${const_sql_0_93}")
    def const_sql_0_94 = """select "-100001.000000", cast("-100001.000000" as double);"""
    qt_sql_0_94_strict "${const_sql_0_94}"
    testFoldConst("${const_sql_0_94}")
    def const_sql_0_95 = """select "-999999.000000", cast("-999999.000000" as double);"""
    qt_sql_0_95_strict "${const_sql_0_95}"
    testFoldConst("${const_sql_0_95}")
    def const_sql_0_96 = """select "-123456.000000", cast("-123456.000000" as double);"""
    qt_sql_0_96_strict "${const_sql_0_96}"
    testFoldConst("${const_sql_0_96}")
    def const_sql_0_97 = """select "-123.456", cast("-123.456" as double);"""
    qt_sql_0_97_strict "${const_sql_0_97}"
    testFoldConst("${const_sql_0_97}")
    def const_sql_0_98 = """select "-999.999", cast("-999.999" as double);"""
    qt_sql_0_98_strict "${const_sql_0_98}"
    testFoldConst("${const_sql_0_98}")
    def const_sql_0_99 = """select "-nan", cast("-nan" as double);"""
    qt_sql_0_99_strict "${const_sql_0_99}"
    testFoldConst("${const_sql_0_99}")
    def const_sql_0_100 = """select "-NaN", cast("-NaN" as double);"""
    qt_sql_0_100_strict "${const_sql_0_100}"
    testFoldConst("${const_sql_0_100}")
    def const_sql_0_101 = """select "-NAN", cast("-NAN" as double);"""
    qt_sql_0_101_strict "${const_sql_0_101}"
    testFoldConst("${const_sql_0_101}")
    def const_sql_0_102 = """select "-inf", cast("-inf" as double);"""
    qt_sql_0_102_strict "${const_sql_0_102}"
    testFoldConst("${const_sql_0_102}")
    def const_sql_0_103 = """select "-Inf", cast("-Inf" as double);"""
    qt_sql_0_103_strict "${const_sql_0_103}"
    testFoldConst("${const_sql_0_103}")
    def const_sql_0_104 = """select "-INF", cast("-INF" as double);"""
    qt_sql_0_104_strict "${const_sql_0_104}"
    testFoldConst("${const_sql_0_104}")
    def const_sql_0_105 = """select "-infinity", cast("-infinity" as double);"""
    qt_sql_0_105_strict "${const_sql_0_105}"
    testFoldConst("${const_sql_0_105}")
    def const_sql_0_106 = """select "-Infinity", cast("-Infinity" as double);"""
    qt_sql_0_106_strict "${const_sql_0_106}"
    testFoldConst("${const_sql_0_106}")
    def const_sql_0_107 = """select "-INFINITY", cast("-INFINITY" as double);"""
    qt_sql_0_107_strict "${const_sql_0_107}"
    testFoldConst("${const_sql_0_107}")
    def const_sql_0_108 = """select "-1.7976931348623157e+308", cast("-1.7976931348623157e+308" as double);"""
    qt_sql_0_108_strict "${const_sql_0_108}"
    testFoldConst("${const_sql_0_108}")
    def const_sql_0_109 = """select "-2.2250738585072014e-308", cast("-2.2250738585072014e-308" as double);"""
    qt_sql_0_109_strict "${const_sql_0_109}"
    testFoldConst("${const_sql_0_109}")

    sql "set enable_strict_cast=false;"
    qt_sql_0_0_non_strict "${const_sql_0_0}"
    testFoldConst("${const_sql_0_0}")
    qt_sql_0_1_non_strict "${const_sql_0_1}"
    testFoldConst("${const_sql_0_1}")
    qt_sql_0_2_non_strict "${const_sql_0_2}"
    testFoldConst("${const_sql_0_2}")
    qt_sql_0_3_non_strict "${const_sql_0_3}"
    testFoldConst("${const_sql_0_3}")
    qt_sql_0_4_non_strict "${const_sql_0_4}"
    testFoldConst("${const_sql_0_4}")
    qt_sql_0_5_non_strict "${const_sql_0_5}"
    testFoldConst("${const_sql_0_5}")
    qt_sql_0_6_non_strict "${const_sql_0_6}"
    testFoldConst("${const_sql_0_6}")
    qt_sql_0_7_non_strict "${const_sql_0_7}"
    testFoldConst("${const_sql_0_7}")
    qt_sql_0_8_non_strict "${const_sql_0_8}"
    testFoldConst("${const_sql_0_8}")
    qt_sql_0_9_non_strict "${const_sql_0_9}"
    testFoldConst("${const_sql_0_9}")
    qt_sql_0_10_non_strict "${const_sql_0_10}"
    testFoldConst("${const_sql_0_10}")
    qt_sql_0_11_non_strict "${const_sql_0_11}"
    testFoldConst("${const_sql_0_11}")
    qt_sql_0_12_non_strict "${const_sql_0_12}"
    testFoldConst("${const_sql_0_12}")
    qt_sql_0_13_non_strict "${const_sql_0_13}"
    testFoldConst("${const_sql_0_13}")
    qt_sql_0_14_non_strict "${const_sql_0_14}"
    testFoldConst("${const_sql_0_14}")
    qt_sql_0_15_non_strict "${const_sql_0_15}"
    testFoldConst("${const_sql_0_15}")
    qt_sql_0_16_non_strict "${const_sql_0_16}"
    testFoldConst("${const_sql_0_16}")
    qt_sql_0_17_non_strict "${const_sql_0_17}"
    testFoldConst("${const_sql_0_17}")
    qt_sql_0_18_non_strict "${const_sql_0_18}"
    testFoldConst("${const_sql_0_18}")
    qt_sql_0_19_non_strict "${const_sql_0_19}"
    testFoldConst("${const_sql_0_19}")
    qt_sql_0_20_non_strict "${const_sql_0_20}"
    testFoldConst("${const_sql_0_20}")
    qt_sql_0_21_non_strict "${const_sql_0_21}"
    testFoldConst("${const_sql_0_21}")
    qt_sql_0_22_non_strict "${const_sql_0_22}"
    testFoldConst("${const_sql_0_22}")
    qt_sql_0_23_non_strict "${const_sql_0_23}"
    testFoldConst("${const_sql_0_23}")
    qt_sql_0_24_non_strict "${const_sql_0_24}"
    testFoldConst("${const_sql_0_24}")
    qt_sql_0_25_non_strict "${const_sql_0_25}"
    testFoldConst("${const_sql_0_25}")
    qt_sql_0_26_non_strict "${const_sql_0_26}"
    testFoldConst("${const_sql_0_26}")
    qt_sql_0_27_non_strict "${const_sql_0_27}"
    testFoldConst("${const_sql_0_27}")
    qt_sql_0_28_non_strict "${const_sql_0_28}"
    testFoldConst("${const_sql_0_28}")
    qt_sql_0_29_non_strict "${const_sql_0_29}"
    testFoldConst("${const_sql_0_29}")
    qt_sql_0_30_non_strict "${const_sql_0_30}"
    testFoldConst("${const_sql_0_30}")
    qt_sql_0_31_non_strict "${const_sql_0_31}"
    testFoldConst("${const_sql_0_31}")
    qt_sql_0_32_non_strict "${const_sql_0_32}"
    testFoldConst("${const_sql_0_32}")
    qt_sql_0_33_non_strict "${const_sql_0_33}"
    testFoldConst("${const_sql_0_33}")
    qt_sql_0_34_non_strict "${const_sql_0_34}"
    testFoldConst("${const_sql_0_34}")
    qt_sql_0_35_non_strict "${const_sql_0_35}"
    testFoldConst("${const_sql_0_35}")
    qt_sql_0_36_non_strict "${const_sql_0_36}"
    testFoldConst("${const_sql_0_36}")
    qt_sql_0_37_non_strict "${const_sql_0_37}"
    testFoldConst("${const_sql_0_37}")
    qt_sql_0_38_non_strict "${const_sql_0_38}"
    testFoldConst("${const_sql_0_38}")
    qt_sql_0_39_non_strict "${const_sql_0_39}"
    testFoldConst("${const_sql_0_39}")
    qt_sql_0_40_non_strict "${const_sql_0_40}"
    testFoldConst("${const_sql_0_40}")
    qt_sql_0_41_non_strict "${const_sql_0_41}"
    testFoldConst("${const_sql_0_41}")
    qt_sql_0_42_non_strict "${const_sql_0_42}"
    testFoldConst("${const_sql_0_42}")
    qt_sql_0_43_non_strict "${const_sql_0_43}"
    testFoldConst("${const_sql_0_43}")
    qt_sql_0_44_non_strict "${const_sql_0_44}"
    testFoldConst("${const_sql_0_44}")
    qt_sql_0_45_non_strict "${const_sql_0_45}"
    testFoldConst("${const_sql_0_45}")
    qt_sql_0_46_non_strict "${const_sql_0_46}"
    testFoldConst("${const_sql_0_46}")
    qt_sql_0_47_non_strict "${const_sql_0_47}"
    testFoldConst("${const_sql_0_47}")
    qt_sql_0_48_non_strict "${const_sql_0_48}"
    testFoldConst("${const_sql_0_48}")
    qt_sql_0_49_non_strict "${const_sql_0_49}"
    testFoldConst("${const_sql_0_49}")
    qt_sql_0_50_non_strict "${const_sql_0_50}"
    testFoldConst("${const_sql_0_50}")
    qt_sql_0_51_non_strict "${const_sql_0_51}"
    testFoldConst("${const_sql_0_51}")
    qt_sql_0_52_non_strict "${const_sql_0_52}"
    testFoldConst("${const_sql_0_52}")
    qt_sql_0_53_non_strict "${const_sql_0_53}"
    testFoldConst("${const_sql_0_53}")
    qt_sql_0_54_non_strict "${const_sql_0_54}"
    testFoldConst("${const_sql_0_54}")
    qt_sql_0_55_non_strict "${const_sql_0_55}"
    testFoldConst("${const_sql_0_55}")
    qt_sql_0_56_non_strict "${const_sql_0_56}"
    testFoldConst("${const_sql_0_56}")
    qt_sql_0_57_non_strict "${const_sql_0_57}"
    testFoldConst("${const_sql_0_57}")
    qt_sql_0_58_non_strict "${const_sql_0_58}"
    testFoldConst("${const_sql_0_58}")
    qt_sql_0_59_non_strict "${const_sql_0_59}"
    testFoldConst("${const_sql_0_59}")
    qt_sql_0_60_non_strict "${const_sql_0_60}"
    testFoldConst("${const_sql_0_60}")
    qt_sql_0_61_non_strict "${const_sql_0_61}"
    testFoldConst("${const_sql_0_61}")
    qt_sql_0_62_non_strict "${const_sql_0_62}"
    testFoldConst("${const_sql_0_62}")
    qt_sql_0_63_non_strict "${const_sql_0_63}"
    testFoldConst("${const_sql_0_63}")
    qt_sql_0_64_non_strict "${const_sql_0_64}"
    testFoldConst("${const_sql_0_64}")
    qt_sql_0_65_non_strict "${const_sql_0_65}"
    testFoldConst("${const_sql_0_65}")
    qt_sql_0_66_non_strict "${const_sql_0_66}"
    testFoldConst("${const_sql_0_66}")
    qt_sql_0_67_non_strict "${const_sql_0_67}"
    testFoldConst("${const_sql_0_67}")
    qt_sql_0_68_non_strict "${const_sql_0_68}"
    testFoldConst("${const_sql_0_68}")
    qt_sql_0_69_non_strict "${const_sql_0_69}"
    testFoldConst("${const_sql_0_69}")
    qt_sql_0_70_non_strict "${const_sql_0_70}"
    testFoldConst("${const_sql_0_70}")
    qt_sql_0_71_non_strict "${const_sql_0_71}"
    testFoldConst("${const_sql_0_71}")
    qt_sql_0_72_non_strict "${const_sql_0_72}"
    testFoldConst("${const_sql_0_72}")
    qt_sql_0_73_non_strict "${const_sql_0_73}"
    testFoldConst("${const_sql_0_73}")
    qt_sql_0_74_non_strict "${const_sql_0_74}"
    testFoldConst("${const_sql_0_74}")
    qt_sql_0_75_non_strict "${const_sql_0_75}"
    testFoldConst("${const_sql_0_75}")
    qt_sql_0_76_non_strict "${const_sql_0_76}"
    testFoldConst("${const_sql_0_76}")
    qt_sql_0_77_non_strict "${const_sql_0_77}"
    testFoldConst("${const_sql_0_77}")
    qt_sql_0_78_non_strict "${const_sql_0_78}"
    testFoldConst("${const_sql_0_78}")
    qt_sql_0_79_non_strict "${const_sql_0_79}"
    testFoldConst("${const_sql_0_79}")
    qt_sql_0_80_non_strict "${const_sql_0_80}"
    testFoldConst("${const_sql_0_80}")
    qt_sql_0_81_non_strict "${const_sql_0_81}"
    testFoldConst("${const_sql_0_81}")
    qt_sql_0_82_non_strict "${const_sql_0_82}"
    testFoldConst("${const_sql_0_82}")
    qt_sql_0_83_non_strict "${const_sql_0_83}"
    testFoldConst("${const_sql_0_83}")
    qt_sql_0_84_non_strict "${const_sql_0_84}"
    testFoldConst("${const_sql_0_84}")
    qt_sql_0_85_non_strict "${const_sql_0_85}"
    testFoldConst("${const_sql_0_85}")
    qt_sql_0_86_non_strict "${const_sql_0_86}"
    testFoldConst("${const_sql_0_86}")
    qt_sql_0_87_non_strict "${const_sql_0_87}"
    testFoldConst("${const_sql_0_87}")
    qt_sql_0_88_non_strict "${const_sql_0_88}"
    testFoldConst("${const_sql_0_88}")
    qt_sql_0_89_non_strict "${const_sql_0_89}"
    testFoldConst("${const_sql_0_89}")
    qt_sql_0_90_non_strict "${const_sql_0_90}"
    testFoldConst("${const_sql_0_90}")
    qt_sql_0_91_non_strict "${const_sql_0_91}"
    testFoldConst("${const_sql_0_91}")
    qt_sql_0_92_non_strict "${const_sql_0_92}"
    testFoldConst("${const_sql_0_92}")
    qt_sql_0_93_non_strict "${const_sql_0_93}"
    testFoldConst("${const_sql_0_93}")
    qt_sql_0_94_non_strict "${const_sql_0_94}"
    testFoldConst("${const_sql_0_94}")
    qt_sql_0_95_non_strict "${const_sql_0_95}"
    testFoldConst("${const_sql_0_95}")
    qt_sql_0_96_non_strict "${const_sql_0_96}"
    testFoldConst("${const_sql_0_96}")
    qt_sql_0_97_non_strict "${const_sql_0_97}"
    testFoldConst("${const_sql_0_97}")
    qt_sql_0_98_non_strict "${const_sql_0_98}"
    testFoldConst("${const_sql_0_98}")
    qt_sql_0_99_non_strict "${const_sql_0_99}"
    testFoldConst("${const_sql_0_99}")
    qt_sql_0_100_non_strict "${const_sql_0_100}"
    testFoldConst("${const_sql_0_100}")
    qt_sql_0_101_non_strict "${const_sql_0_101}"
    testFoldConst("${const_sql_0_101}")
    qt_sql_0_102_non_strict "${const_sql_0_102}"
    testFoldConst("${const_sql_0_102}")
    qt_sql_0_103_non_strict "${const_sql_0_103}"
    testFoldConst("${const_sql_0_103}")
    qt_sql_0_104_non_strict "${const_sql_0_104}"
    testFoldConst("${const_sql_0_104}")
    qt_sql_0_105_non_strict "${const_sql_0_105}"
    testFoldConst("${const_sql_0_105}")
    qt_sql_0_106_non_strict "${const_sql_0_106}"
    testFoldConst("${const_sql_0_106}")
    qt_sql_0_107_non_strict "${const_sql_0_107}"
    testFoldConst("${const_sql_0_107}")
    qt_sql_0_108_non_strict "${const_sql_0_108}"
    testFoldConst("${const_sql_0_108}")
    qt_sql_0_109_non_strict "${const_sql_0_109}"
    testFoldConst("${const_sql_0_109}")
}