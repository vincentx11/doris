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

// Most of the cases are copied from https://github.com/trinodb/trino/tree/master
// /testing/trino-product-tests/src/main/resources/sql-tests/testcases
// and modified by Doris.

// syntax error:
// q06 q13 q15
// Test 23 suites, failed 3 suites

// Note: To filter out tables from sql files, use the following one-liner comamnd
// sed -nr 's/.*tables: (.*)$/\1/gp' /path/to/*.sql | sed -nr 's/,/\n/gp' | sort | uniq
suite("load") {
    def tables = ["customer",
                  "lineitem",
                  "nation",
                  "orders",
                  "part",
                  "partsupp",
                  "region", 
                  "supplier"]

    tables.forEach { tableName ->
        sql "DROP TABLE IF EXISTS ${tableName}"
        sql """
                CREATE TABLE IF NOT EXISTS ${tableName} (
                    k bigint,
                    var variant
                    
                )
                UNIQUE KEY(`k`)
                DISTRIBUTED BY HASH(k) BUCKETS 5 
                properties("replication_num" = "1", "disable_auto_compaction" = "false", "enable_unique_key_merge_on_write" = "false");
            """
        streamLoad {
            // a default db 'regression_test' is specified in
            // ${DORIS_HOME}/conf/regression-conf.groovy
            table "${tableName}"

            // set http request header params
            set 'read_json_by_line', 'true' 
            set 'format', 'json' 
            // set 'max_filter_ratio', '0.1'
            time 10000 // limit inflight 10s 

            // relate to ${DORIS_HOME}/regression-test/data/demo/streamload_input.csv.
            // also, you can stream load a http stream, e.g. http://xxx/some.csv
            file """${getS3Url()}/regression/tpch-var/sf0.1/${tableName}.txt.json"""

            // stream load action will check result, include Success status, and NumberTotalRows == NumberLoadedRows

            // if declared a check callback, the default check condition will ignore.
            // So you must check all condition
            check { result, exception, startTime, endTime ->
                if (exception != null) {
                    throw exception
                }
                log.info("Stream load result: ${result}".toString())
                def json = parseJson(result)
                assertEquals("success", json.Status.toLowerCase())
                assertEquals(json.NumberTotalRows, json.NumberLoadedRows)
                assertTrue(json.NumberLoadedRows > 0 && json.LoadBytes > 0)
            }
        }
    }
    // Thread.sleep(70000) // wait for row count report of the tables just loaded
    // tables.forEach { tableName ->
    //     sql """ ANALYZE TABLE $tableName WITH SYNC """
    // }

    // def table = "revenue1"
    // sql new File("""${context.file.parent}/ddl/${table}_delete.sql""").text
    // sql new File("""${context.file.parent}/ddl/${table}.sql""").text

    sql """ sync """
}
