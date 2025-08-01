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

namespace cpp doris
namespace java org.apache.doris.thrift

include "Types.thrift"
include "Exprs.thrift"
include "Partitions.thrift"

enum TPatternType {
  MATCH_NAME = 1,
  MATCH_NAME_GLOB = 2
}

struct TColumn {
    1: required string column_name
    2: required Types.TColumnType column_type
    3: optional Types.TAggregationType aggregation_type
    4: optional bool is_key
    5: optional bool is_allow_null
    6: optional string default_value
    7: optional bool is_bloom_filter_column
    8: optional Exprs.TExpr define_expr
    9: optional bool visible = true
    10: optional list<TColumn> children_column
    11: optional i32 col_unique_id  = -1
    12: optional bool has_bitmap_index = false
    13: optional bool has_ngram_bf_index = false
    14: optional i32 gram_size
    15: optional i32 gram_bf_size
    16: optional string aggregation
    17: optional bool result_is_nullable
    18: optional bool is_auto_increment = false;
    19: optional i32 cluster_key_id = -1
    20: optional i32 be_exec_version = -1
    21: optional TPatternType pattern_type
    22: optional bool variant_enable_typed_paths_to_sparse = false
    23: optional bool is_on_update_current_timestamp = false
}

struct TSlotDescriptor {
  1: required Types.TSlotId id
  2: required Types.TTupleId parent
  3: required Types.TTypeDesc slotType
  4: required i32 columnPos   // in originating table
  5: required i32 byteOffset  // deprecated
  6: required i32 nullIndicatorByte
  7: required i32 nullIndicatorBit
  8: required string colName;
  9: required i32 slotIdx
  10: required bool isMaterialized
  11: optional i32 col_unique_id = -1
  12: optional bool is_key = false
  // If set to false, then such slots will be ignored during
  // materialize them.Used to optmize to read less data and less memory usage
  13: optional bool need_materialize = true
  14: optional bool is_auto_increment = false;
  // subcolumn path info list for semi structure column(variant)
  15: optional list<string> column_paths
  16: optional string col_default_value
  17: optional Types.TPrimitiveType primitive_type = Types.TPrimitiveType.INVALID_TYPE
  18: optional Exprs.TExpr virtual_column_expr
}

struct TTupleDescriptor {
  1: required Types.TTupleId id
  2: required i32 byteSize // deprecated
  3: required i32 numNullBytes // deprecated
  4: optional Types.TTableId tableId
  5: optional i32 numNullSlots // deprecated
}

enum THdfsFileFormat {
  TEXT = 0,
  LZO_TEXT = 1,
  RC_FILE = 2,
  SEQUENCE_FILE =3,
  AVRO = 4,
  PARQUET = 5
}

enum TSchemaTableType {
    SCH_AUTHORS = 0,
    SCH_CHARSETS = 1,
    SCH_COLLATIONS = 2,
    SCH_COLLATION_CHARACTER_SET_APPLICABILITY = 3,
    SCH_COLUMNS = 4,
    SCH_COLUMN_PRIVILEGES = 5,
    SCH_CREATE_TABLE = 6,
    SCH_ENGINES = 7,
    SCH_EVENTS = 8,
    SCH_FILES = 9,
    SCH_GLOBAL_STATUS = 10,
    SCH_GLOBAL_VARIABLES = 11,
    SCH_KEY_COLUMN_USAGE = 12,
    SCH_OPEN_TABLES = 13,
    SCH_PARTITIONS = 14,
    SCH_PLUGINS = 15,
    SCH_PROCESSLIST = 16,
    SCH_PROFILES = 17,
    SCH_REFERENTIAL_CONSTRAINTS = 18,
    SCH_PROCEDURES = 19,
    SCH_SCHEMATA = 20,
    SCH_SCHEMA_PRIVILEGES = 21,
    SCH_SESSION_STATUS = 22,
    SCH_SESSION_VARIABLES = 23,
    SCH_STATISTICS = 24,
    SCH_STATUS = 25,
    SCH_TABLES = 26,
    SCH_TABLE_CONSTRAINTS = 27,
    SCH_TABLE_NAMES = 28,
    SCH_TABLE_PRIVILEGES = 29,
    SCH_TRIGGERS = 30,
    SCH_USER_PRIVILEGES = 31,
    SCH_VARIABLES = 32,
    SCH_VIEWS = 33,
    SCH_INVALID = 34,
    SCH_ROWSETS = 35
    SCH_BACKENDS = 36,
    SCH_COLUMN_STATISTICS = 37,
    SCH_PARAMETERS = 38,
    SCH_METADATA_NAME_IDS = 39,
    SCH_PROFILING = 40,
    SCH_BACKEND_ACTIVE_TASKS = 41,
    SCH_ACTIVE_QUERIES = 42,
    SCH_WORKLOAD_GROUPS = 43,
    SCH_USER = 44,
    SCH_PROCS_PRIV = 45,
    SCH_WORKLOAD_POLICY = 46,
    SCH_TABLE_OPTIONS = 47,
    SCH_WORKLOAD_GROUP_PRIVILEGES = 48,
    SCH_WORKLOAD_GROUP_RESOURCE_USAGE = 49,
    SCH_TABLE_PROPERTIES = 50,
    SCH_FILE_CACHE_STATISTICS = 51,
    SCH_CATALOG_META_CACHE_STATISTICS = 52,
    SCH_BACKEND_KERBEROS_TICKET_CACHE = 53,
    SCH_ROUTINE_LOAD_JOBS = 54,
    SCH_BACKEND_CONFIGURATION=55,
    SCH_BACKEND_TABLETS = 56,
    SCH_VIEW_DEPENDENCY = 57;
}

enum THdfsCompression {
  NONE = 0,
  DEFAULT = 1,
  GZIP = 2,
  DEFLATE = 3,
  BZIP2 = 4,
  SNAPPY = 5,
  SNAPPY_BLOCKED = 6 // Used by sequence and rc files but not stored in the metadata.
}

enum TIndexType {
  BITMAP = 0,
  INVERTED = 1,
  BLOOMFILTER = 2,
  NGRAM_BF = 3
}

enum TPartialUpdateNewRowPolicy {
    APPEND = 0,
    ERROR = 1
}

// Mapping from names defined by Avro to the enum.
// We permit gzip and bzip2 in addition.
const map<string, THdfsCompression> COMPRESSION_MAP = {
  "": THdfsCompression.NONE,
  "none": THdfsCompression.NONE,
  "deflate": THdfsCompression.DEFAULT,
  "gzip": THdfsCompression.GZIP,
  "bzip2": THdfsCompression.BZIP2,
  "snappy": THdfsCompression.SNAPPY
}

struct TOlapTableIndexTablets {
    1: required i64 index_id
    2: required list<i64> tablets
}

// its a closed-open range
struct TOlapTablePartition {
    1: required i64 id
    // deprecated, use 'start_keys' and 'end_keys' instead
    2: optional Exprs.TExprNode start_key
    3: optional Exprs.TExprNode end_key

    // how many tablets in one partition
    4: required i32 num_buckets

    5: required list<TOlapTableIndexTablets> indexes

    6: optional list<Exprs.TExprNode> start_keys
    7: optional list<Exprs.TExprNode> end_keys
    8: optional list<list<Exprs.TExprNode>> in_keys
    9: optional bool is_mutable = true
    // only used in List Partition
    10: optional bool is_default_partition;
    // only used in random distribution scenario to make data distributed even 
    11: optional i64 load_tablet_idx
    12: optional i32 total_replica_num
    13: optional i32 load_required_replica_num
}

struct TOlapTablePartitionParam {
    1: required i64 db_id
    2: required i64 table_id
    3: required i64 version

    // used to split a logical table to multiple paritions
    // deprecated, use 'partition_columns' instead
    4: optional string partition_column

    // used to split a partition to multiple tablets
    5: optional list<string> distributed_columns

    // partitions
    6: required list<TOlapTablePartition> partitions

    7: optional list<string> partition_columns
    8: optional list<Exprs.TExpr> partition_function_exprs
    9: optional bool enable_automatic_partition
    10: optional Partitions.TPartitionType partition_type
    // insert overwrite partition(*)
    11: optional bool enable_auto_detect_overwrite
    12: optional i64 overwrite_group_id
    13: optional bool partitions_is_fake = false
}

struct TOlapTableIndex {
  1: optional string index_name
  2: optional list<string> columns
  3: optional TIndexType index_type
  4: optional string comment
  5: optional i64 index_id
  6: optional map<string, string> properties
  7: optional list<i32> column_unique_ids
}

struct TOlapTableIndexSchema {
    1: required i64 id
    2: required list<string> columns
    3: required i32 schema_hash
    4: optional list<TColumn> columns_desc
    5: optional list<TOlapTableIndex> indexes_desc
    6: optional Exprs.TExpr where_clause
}

struct TOlapTableSchemaParam {
    1: required i64 db_id
    2: required i64 table_id
    3: required i64 version

    // Logical columns, contain all column that in logical table
    4: required list<TSlotDescriptor> slot_descs
    5: required TTupleDescriptor tuple_desc
    6: required list<TOlapTableIndexSchema> indexes
    7: optional bool is_dynamic_schema // deprecated
    8: optional bool is_partial_update // deprecated, use unique_key_update_mode
    9: optional list<string> partial_update_input_columns
    10: optional bool is_strict_mode = false
    11: optional string auto_increment_column
    12: optional i32 auto_increment_column_unique_id = -1
    13: optional Types.TInvertedIndexFileStorageFormat inverted_index_file_storage_format = Types.TInvertedIndexFileStorageFormat.V1
    14: optional Types.TUniqueKeyUpdateMode unique_key_update_mode
    15: optional i32 sequence_map_col_unique_id = -1
    16: optional TPartialUpdateNewRowPolicy partial_update_new_key_policy
}

struct TTabletLocation {
    1: required i64 tablet_id
    2: required list<i64> node_ids
}

struct TOlapTableLocationParam {
    1: required i64 db_id
    2: required i64 table_id
    3: required i64 version
    4: required list<TTabletLocation> tablets
}

struct TNodeInfo {
    1: required i64 id
    2: required i64 option
    3: required string host
    // used to transfer data between nodes
    4: required i32 async_internal_port
}

struct TPaloNodesInfo {
    1: required i64 version
    2: required list<TNodeInfo> nodes
}

struct TOlapTable {
    1: required string tableName
}

struct TMySQLTable {
  1: required string host
  2: required string port
  3: required string user
  4: required string passwd
  5: required string db
  6: required string table
  7: required string charset
}

struct TOdbcTable {
  1: optional string host
  2: optional string port
  3: optional string user
  4: optional string passwd
  5: optional string db
  6: optional string table
  7: optional string driver
  8: optional Types.TOdbcTableType type
}

struct TEsTable {
}

struct TSchemaTable {
  1: required TSchemaTableType tableType
}

struct TBrokerTable {
}

struct THiveTable {
  1: required string db_name
  2: required string table_name
  3: required map<string, string> properties
}

struct TIcebergTable {
  1: required string db_name
  2: required string table_name
  3: required map<string, string> properties
}

struct THudiTable {
  1: optional string dbName
  2: optional string tableName
  3: optional map<string, string> properties
}

struct TJdbcTable {
  1: optional string jdbc_url
  2: optional string jdbc_table_name
  3: optional string jdbc_user
  4: optional string jdbc_password
  5: optional string jdbc_driver_url
  6: optional string jdbc_resource_name
  7: optional string jdbc_driver_class
  8: optional string jdbc_driver_checksum
  9: optional i32 connection_pool_min_size
  10: optional i32 connection_pool_max_size
  11: optional i32 connection_pool_max_wait_time
  12: optional i32 connection_pool_max_life_time
  13: optional bool connection_pool_keep_alive
  14: optional i64 catalog_id
}

struct TMCTable {
  1: optional string region // deprecated
  2: optional string project
  3: optional string table
  4: optional string access_key
  5: optional string secret_key
  6: optional string public_access // deprecated
  7: optional string odps_url   // deprecated
  8: optional string tunnel_url // deprecated 
  9: optional string endpoint
  10: optional string quota
}

struct TTrinoConnectorTable {
  1: optional string db_name
  2: optional string table_name
  3: optional map<string, string> properties
}

struct TLakeSoulTable {
  1: optional string db_name
  2: optional string table_name
  3: optional map<string, string> properties
}

struct TDictionaryTable {
}

// "Union" of all table types.
struct TTableDescriptor {
  1: required Types.TTableId id
  2: required Types.TTableType tableType
  3: required i32 numCols
  4: required i32 numClusteringCols

  // Unqualified name of table
  7: required string tableName;

  // Name of the database that the table belongs to
  8: required string dbName;
  10: optional TMySQLTable mysqlTable
  11: optional TOlapTable olapTable
  12: optional TSchemaTable schemaTable
  14: optional TBrokerTable BrokerTable
  15: optional TEsTable esTable
  16: optional TOdbcTable odbcTable
  17: optional THiveTable hiveTable
  18: optional TIcebergTable icebergTable
  19: optional THudiTable hudiTable
  20: optional TJdbcTable jdbcTable
  21: optional TMCTable mcTable
  22: optional TTrinoConnectorTable trinoConnectorTable
  23: optional TLakeSoulTable lakesoulTable
  24: optional TDictionaryTable dictionaryTable
}

struct TDescriptorTable {
  1: optional list<TSlotDescriptor> slotDescriptors;
  2: required list<TTupleDescriptor> tupleDescriptors;

  // all table descriptors referenced by tupleDescriptors
  3: optional list<TTableDescriptor> tableDescriptors;
}
