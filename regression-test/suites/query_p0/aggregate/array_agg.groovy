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

suite("array_agg") {
    sql "DROP TABLE IF EXISTS `test_array_agg`;"
    sql "DROP TABLE IF EXISTS `test_array_agg1`;"
    sql "DROP TABLE IF EXISTS `test_array_agg_int`;"
    sql "DROP TABLE IF EXISTS `test_array_agg_decimal`;"
    sql "DROP TABLE IF EXISTS `test_array_agg_complex`;"

    sql """
       CREATE TABLE `test_array_agg` (
        `id` int(11) NOT NULL,
        `label_name` varchar(32) default null,
        `value_field` string default null
    ) ENGINE=OLAP
    DUPLICATE KEY(`id`)
    COMMENT 'OLAP'
    DISTRIBUTED BY HASH(`id`) BUCKETS 1
    PROPERTIES (
    "replication_allocation" = "tag.location.default: 1",
    "storage_format" = "V2",
    "light_schema_change" = "true",
    "disable_auto_compaction" = "false",
    "enable_single_replica_compaction" = "false"
    );
    """

    sql """
    CREATE TABLE `test_array_agg_int` (
        `id` int(11) NOT NULL,
        `label_name` varchar(32) default null,
        `value_field` string default null,
        `age` int(11) default null
    ) ENGINE=OLAP
    DUPLICATE KEY(`id`)
    COMMENT 'OLAP'
    DISTRIBUTED BY HASH(`id`) BUCKETS 1
    PROPERTIES (
    "replication_allocation" = "tag.location.default: 1",
    "storage_format" = "V2",
    "light_schema_change" = "true",
    "disable_auto_compaction" = "false",
    "enable_single_replica_compaction" = "false"
    );
    """

    sql """
       CREATE TABLE `test_array_agg_decimal` (
        `id` int(11) NOT NULL,
        `label_name` varchar(32) default null,
        `value_field` string default null,
        `age` int(11) default null,
         `o_totalprice` DECIMAL(15, 2) default NULL,
         `label_name_not_null` varchar(32) not null
    )ENGINE=OLAP
    DUPLICATE KEY(`id`)
    COMMENT 'OLAP'
    DISTRIBUTED BY HASH(`id`) BUCKETS 1
    PROPERTIES (
    "replication_allocation" = "tag.location.default: 1",
    "storage_format" = "V2",
    "light_schema_change" = "true",
    "disable_auto_compaction" = "false",
    "enable_single_replica_compaction" = "false"
    );
    """

    sql """
    insert into `test_array_agg` values
    (1, "alex",NULL),
    (1, "LB", "V1_2"),
    (1, "LC", "V1_3"),
    (2, "LA", "V2_1"),
    (2, "LB", "V2_2"),
    (2, "LC", "V2_3"),
    (3, "LA", "V3_1"),
    (3, NULL, NULL),
    (3, "LC", "V3_3"),
    (4, "LA", "V4_1"),
    (4, "LB", "V4_2"),
    (4, "LC", "V4_3"),
    (5, "LA", "V5_1"),
    (5, "LB", "V5_2"),
    (5, "LC", "V5_3"),
    (5, NULL, "V5_3"),
    (6, "LC", "V6_3"),
    (6, "LC", NULL),
    (6, "LC", "V6_3"),
    (6, "LC", NULL),
    (6, NULL, "V6_3"),
    (7, "LC", "V7_3"),
    (7, "LC", NULL),
    (7, "LC", "V7_3"),
    (7, "LC", NULL),
    (7, NULL, "V7_3");
    """

    sql """
    insert into `test_array_agg_int` values
    (1, "alex",NULL,NULL),
    (1, "LB", "V1_2",1),
    (1, "LC", "V1_3",2),
    (2, "LA", "V2_1",4),
    (2, "LB", "V2_2",5),
    (2, "LC", "V2_3",5),
    (3, "LA", "V3_1",6),
    (3, NULL, NULL,6),
    (3, "LC", "V3_3",NULL),
    (4, "LA", "V4_1",5),
    (4, "LB", "V4_2",6),
    (4, "LC", "V4_3",6),
    (5, "LA", "V5_1",6),
    (5, "LB", "V5_2",5),
    (5, "LC", "V5_3",NULL),
    (6, "LC", "V6_3",NULL),
    (6, "LC", NULL,NULL),
    (6, "LC", "V6_3",NULL),
    (6, "LC", NULL,NULL),
    (6, NULL, "V6_3",NULL),
    (7, "LC", "V7_3",NULL),
    (7, "LC", NULL,NULL),
    (7, "LC", "V7_3",NULL),
    (7, "LC", NULL,NULL),
    (7, NULL, "V7_3",NULL);
    """

    sql """
    insert into `test_array_agg_decimal` values
    (1, "alex",NULL,NULL,NULL,"alex"),
    (1, "LB", "V1_2",1,NULL,"alexxing"),
    (1, "LC", "V1_3",2,1.11,"alexcoco"),
    (2, "LA", "V2_1",4,1.23,"alex662"),
    (2, "LB", "",5,NULL,""),
    (2, "LC", "",5,1.21,"alexcoco1"),
    (3, "LA", "V3_1",6,1.21,"alexcoco2"),
    (3, NULL, NULL,6,1.23,"alexcoco3"),
    (3, "LC", "V3_3",NULL,1.24,"alexcoco662"),
    (4, "LA", "",5,1.22,"alexcoco662"),
    (4, "LB", "V4_2",6,NULL,"alexcoco662"),
    (4, "LC", "V4_3",6,1.22,"alexcoco662"),
    (5, "LA", "V5_1",6,NULL,"alexcoco662"),
    (5, "LB", "V5_2",5,NULL,"alexcoco662"),
    (5, "LC", "V5_3",NULL,NULL,"alexcoco662"),
    (7, "", NULL,NULL,NULL,"alexcoco1"),
    (8, "", NULL,0,NULL,"alexcoco2");
    """

    order_qt_sql1 """
    SELECT count(id), array_agg(`label_name`) FROM `test_array_agg` GROUP BY `id` order by id;
    """
    order_qt_sql2 """
    SELECT count(value_field), array_agg(label_name) FROM `test_array_agg` GROUP BY value_field order by value_field;
    """
    order_qt_sql3 """
    SELECT array_agg(`label_name`) FROM `test_array_agg`;
    """
    order_qt_sql4 """
    SELECT array_agg(`value_field`) FROM `test_array_agg`;
    """
    order_qt_sql5 """
    SELECT id, array_agg(age) FROM test_array_agg_int GROUP BY id order by id;
    """

    order_qt_sql6 """
    select array_agg(label_name) from test_array_agg_decimal where id=7;
    """

    order_qt_sql6_1 """
    select sum(o_totalprice), array_agg(label_name) from test_array_agg_decimal where id=7;
    """

    order_qt_sql7 """
    select array_agg(label_name) from test_array_agg_decimal group by id order by id;
    """

    order_qt_sql8 """
    select array_agg(age) from test_array_agg_decimal where id=7;
    """

    order_qt_sql9 """
    select id,array_agg(o_totalprice) from test_array_agg_decimal group by id order by id;
    """


    // test for bucket 10
    sql """ CREATE TABLE `test_array_agg1` (
        `id` int(11) NOT NULL,
        `label_name` varchar(32) default null,
        `value_field` string default null
    ) ENGINE=OLAP
    DUPLICATE KEY(`id`)
    COMMENT 'OLAP'
    DISTRIBUTED BY HASH(`id`) BUCKETS 10
    PROPERTIES (
    "replication_allocation" = "tag.location.default: 1",
    "storage_format" = "V2",
    "light_schema_change" = "true",
    "disable_auto_compaction" = "false",
    "enable_single_replica_compaction" = "false"
    ); """

    sql """
    insert into `test_array_agg1` values
    (1, "alex",NULL),
    (1, "LB", "V1_2"),
    (1, "LC", "V1_3"),
    (2, "LA", "V2_1"),
    (2, "LB", "V2_2"),
    (2, "LC", "V2_3"),
    (3, "LA", "V3_1"),
    (3, NULL, NULL),
    (3, "LC", "V3_3"),
    (4, "LA", "V4_1"),
    (4, "LB", "V4_2"),
    (4, "LC", "V4_3"),
    (5, "LA", "V5_1"),
    (5, "LB", "V5_2"),
    (5, "LC", "V5_3"),
    (5, NULL, "V5_3"),
    (6, "LC", "V6_3"),
    (6, "LC", NULL),
    (6, "LC", "V6_3"),
    (6, "LC", NULL),
    (6, NULL, "V6_3"),
    (7, "LC", "V7_3"),
    (7, "LC", NULL),
    (7, "LC", "V7_3"),
    (7, "LC", NULL),
    (7, NULL, "V7_3");
    """

    order_qt_sql11 """
    SELECT count(id), size(array_agg(`label_name`)) FROM `test_array_agg` GROUP BY `id` order by id;
    """
    order_qt_sql21 """
    SELECT count(value_field), size(array_agg(label_name)) FROM `test_array_agg` GROUP BY value_field order by value_field;
    """

    // only support nereids
    sql "SET enable_nereids_planner=true;"
    sql "SET enable_fallback_to_original_planner=false;"
	sql """ CREATE TABLE IF NOT EXISTS test_array_agg_complex (id int, kastr array<string>, km map<string, int>, ks STRUCT<id: int>) engine=olap
                                                                                         DISTRIBUTED BY HASH(`id`) BUCKETS 4
                                                                                         properties("replication_num" = "1") """
    streamLoad {
        table "test_array_agg_complex"
        file "test_array_agg_complex.csv"
        time 60000

        check { result, exception, startTime, endTime ->
            if (exception != null) {
                throw exception
            }
            log.info("Stream load result: ${result}".toString())
            def json = parseJson(result)
            assertEquals(112, json.NumberTotalRows)
            assertEquals(112, json.NumberLoadedRows)
        }
    }

    order_qt_sql_array_agg_array """ SELECT id, array_agg(kastr) FROM test_array_agg_complex GROUP BY id ORDER BY id """
    order_qt_sql_array_agg_map """ SELECT id, array_agg(km) FROM test_array_agg_complex GROUP BY id ORDER BY id """
    order_qt_sql_array_agg_struct """ SELECT id, array_agg(ks) FROM test_array_agg_complex GROUP BY id ORDER BY id """
    order_qt_sql_collect_list_array """ SELECT id, collect_list(kastr) FROM test_array_agg_complex GROUP BY id ORDER BY id """
    order_qt_sql_collect_list_map """ SELECT id, collect_list(km) FROM test_array_agg_complex GROUP BY id ORDER BY id """
    order_qt_sql_collect_list_struct """ SELECT id, collect_list(ks) FROM test_array_agg_complex GROUP BY id ORDER BY id """
    order_qt_sql_group_array_array """ SELECT group_array(kastr) FROM test_array_agg_complex GROUP BY id ORDER BY id """
    order_qt_sql_group_array_map """ SELECT group_array(km) FROM test_array_agg_complex GROUP BY id ORDER BY id """
    order_qt_sql_group_array_struct """ SELECT group_array(ks) FROM test_array_agg_complex GROUP BY id ORDER BY id """
    // add limit for param
    order_qt_sql_array_agg_array_limit """ SELECT id, array_agg(kastr) FROM test_array_agg_complex GROUP BY id ORDER BY id """
    order_qt_sql_array_agg_map_limit """ SELECT id, array_agg(km) FROM test_array_agg_complex GROUP BY id ORDER BY id """
    order_qt_sql_array_agg_struct_limit """ SELECT id, array_agg(ks) FROM test_array_agg_complex GROUP BY id ORDER BY id"""
    order_qt_sql_collect_list_array_limit """ SELECT id, collect_list(kastr, 2) FROM test_array_agg_complex GROUP BY id ORDER BY id"""
    order_qt_sql_collect_list_map_limit """ SELECT id, collect_list(km, 2) FROM test_array_agg_complex GROUP BY id ORDER BY id"""
    order_qt_sql_collect_list_struct_limit """ SELECT id, collect_list(ks, 3) FROM test_array_agg_complex GROUP BY id ORDER BY id"""
    order_qt_sql_group_array_array_limit """ SELECT group_array(kastr, 3) FROM test_array_agg_complex GROUP BY id ORDER BY id"""
    order_qt_sql_group_array_map_limit """ SELECT group_array(km, 7) FROM test_array_agg_complex GROUP BY id ORDER BY id"""
    order_qt_sql_group_array_struct_limit """ SELECT group_array(ks, 7) FROM test_array_agg_complex GROUP BY id ORDER BY id"""

    // for session variable "set ENABLE_LOCAL_EXCHANGE = 0
    order_qt_sql_collect_list_array1 """ SELECT /*+ SET ENABLE_LOCAL_EXCHANGE = 0 */ id, collect_list(kastr) FROM test_array_agg_complex GROUP BY id ORDER BY id """
    order_qt_sql_collect_list_map1 """ SELECT /*+ SET ENABLE_LOCAL_EXCHANGE = 0 */ id, collect_list(km) FROM test_array_agg_complex GROUP BY id ORDER BY id """
    order_qt_sql_collect_list_struct1 """ SELECT /*+ SET ENABLE_LOCAL_EXCHANGE = 0 */ id, collect_list(ks) FROM test_array_agg_complex GROUP BY id ORDER BY id """
    order_qt_sql_group_array_array1 """ SELECT /*+ SET ENABLE_LOCAL_EXCHANGE = 0 */ group_array(kastr) FROM test_array_agg_complex GROUP BY id ORDER BY id """
    order_qt_sql_group_array_map1 """ SELECT /*+ SET ENABLE_LOCAL_EXCHANGE = 0 */ group_array(km) FROM test_array_agg_complex GROUP BY id ORDER BY id """
    order_qt_sql_group_array_struct1 """ SELECT /*+ SET ENABLE_LOCAL_EXCHANGE = 0 */ group_array(ks) FROM test_array_agg_complex GROUP BY id ORDER BY id """
    // add limit for param
    order_qt_sql_collect_list_array_limit1 """ SELECT /*+ SET ENABLE_LOCAL_EXCHANGE = 0 */ id, collect_list(kastr, 2) FROM test_array_agg_complex GROUP BY id ORDER BY id """
    order_qt_sql_collect_list_map_limit1 """ SELECT /*+ SET ENABLE_LOCAL_EXCHANGE = 0 */ id, collect_list(km, 2) FROM test_array_agg_complex GROUP BY id ORDER BY id """
    order_qt_sql_collect_list_struct_limit1 """ SELECT /*+ SET ENABLE_LOCAL_EXCHANGE = 0 */ id, collect_list(ks, 3) FROM test_array_agg_complex GROUP BY id ORDER BY id """
    order_qt_sql_group_array_array_limit1 """ SELECT /*+ SET ENABLE_LOCAL_EXCHANGE = 0 */ group_array(kastr, 3) FROM test_array_agg_complex GROUP BY id ORDER BY id """
    order_qt_sql_group_array_map_limit1 """ SELECT /*+ SET ENABLE_LOCAL_EXCHANGE = 0 */ group_array(km, 7) FROM test_array_agg_complex GROUP BY id ORDER BY id """
    order_qt_sql_group_array_struct_limit1 """ SELECT /*+ SET ENABLE_LOCAL_EXCHANGE = 0 */ group_array(ks, 7) FROM test_array_agg_complex GROUP BY id ORDER BY id """

 sql """ DROP TABLE IF EXISTS test_array_agg_ip;"""
    sql """
        CREATE TABLE test_array_agg_ip(
            k1 BIGINT ,
            k4 ipv4 ,
            k6 ipv6 ,
            s string
        ) DISTRIBUTED BY HASH(k1) BUCKETS 1 PROPERTIES("replication_num" = "1");
    """
    sql """ insert into test_array_agg_ip values(1,'0.0.0.123','::855d',"0.0.0.123") , (2,'0.0.12.42','::0.4.221.183',"0.0.0.123") , (3,'0.119.130.67','::a:7429:d0d6:6e08:9f5f',"2001:0DB8:AC10:FE01:FEED:BABE:CAFE:F00D"),(4,null,null,"2001:0DB8:AC10:FE01:FEED:BABE:CAFE:F00D"); """
        

     qt_select """select array_sort(array_agg(k4)),array_sort(array_agg(k6)) from test_array_agg_ip """

    sql "DROP TABLE `test_array_agg`"
    sql "DROP TABLE `test_array_agg1`"
    sql "DROP TABLE `test_array_agg_int`"
    sql "DROP TABLE `test_array_agg_decimal`"
    sql "DROP TABLE `test_array_agg_ip`"


    sql """ drop table if exists test_user_tags;"""

    sql """
    CREATE TABLE test_user_tags (
        k1 varchar(150) NULL,
        k2 varchar(150) NULL,
        k3 varchar(150) NULL,
        k4 array<varchar(150)> NULL,
        k5 array<varchar(150)> NULL,
        k6 datetime NULL
    ) ENGINE=OLAP
    UNIQUE KEY(k1, k2, k3)
    DISTRIBUTED BY HASH(k2) BUCKETS 3
    PROPERTIES ("replication_allocation" = "tag.location.default: 1");
    """

    sql """
    INSERT INTO test_user_tags VALUES
          ('corp001', 'wx001', 'vip', ['id1', 'id2'], ['tag1', 'tag2'], '2023-01-01 10:00:00'),
          ('corp001', 'wx001', 'level', ['id3'], ['tag3'], '2023-01-01 10:00:00'),
          ('corp002', 'wx002', 'vip', ['id4', 'id5'], ['tag4', 'tag5'], '2023-01-02 10:00:00');
    """
    sql "SET spill_streaming_agg_mem_limit = 1024;"
    sql "SET enable_spill = true;"

    qt_select """ SELECT k1,array_agg(k5) FROM test_user_tags group by k1 order by k1; """

    sql "UNSET VARIABLE ALL;"
}
