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

package org.apache.doris.alter;

import org.apache.doris.catalog.Column;
import org.apache.doris.catalog.Database;
import org.apache.doris.catalog.Env;
import org.apache.doris.catalog.Index;
import org.apache.doris.catalog.KeysType;
import org.apache.doris.catalog.MaterializedIndex;
import org.apache.doris.catalog.OlapTable;
import org.apache.doris.catalog.OlapTable.OlapTableState;
import org.apache.doris.catalog.Partition;
import org.apache.doris.catalog.TableIf.TableType;
import org.apache.doris.catalog.Tablet;
import org.apache.doris.cloud.datasource.CloudInternalCatalog;
import org.apache.doris.cloud.proto.Cloud;
import org.apache.doris.cloud.qe.ComputeGroupException;
import org.apache.doris.cloud.system.CloudSystemInfoService;
import org.apache.doris.common.Config;
import org.apache.doris.common.MetaNotFoundException;
import org.apache.doris.proto.OlapFile;
import org.apache.doris.qe.ConnectContext;
import org.apache.doris.task.AgentTask;
import org.apache.doris.task.AgentTaskQueue;
import org.apache.doris.thrift.TTaskType;

import com.google.common.base.Preconditions;
import com.google.common.base.Strings;
import com.google.common.collect.Table;
import com.google.common.collect.Table.Cell;
import org.apache.logging.log4j.LogManager;
import org.apache.logging.log4j.Logger;

import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.stream.Collectors;

public class CloudSchemaChangeJobV2 extends SchemaChangeJobV2 {
    private static final Logger LOG = LogManager.getLogger(SchemaChangeJobV2.class);

    public CloudSchemaChangeJobV2(String rawSql, long jobId, long dbId, long tableId,
            String tableName, long timeoutMs) {
        super(rawSql, jobId, dbId, tableId, tableName, timeoutMs);
        ConnectContext context = ConnectContext.get();
        if (context != null) {
            String clusterName = "";
            try {
                clusterName = context.getCloudCluster();
            } catch (ComputeGroupException e) {
                LOG.warn("failed to get compute group name", e);
            }
            LOG.debug("rollup job add cloud cluster, context not null, cluster: {}", clusterName);
            if (!Strings.isNullOrEmpty(clusterName)) {
                setCloudClusterName(clusterName);
            }
        }
        LOG.debug("schema change job add cloud cluster, context {}", context);
    }

    private CloudSchemaChangeJobV2() {}

    @Override
    protected void commitShadowIndex() throws AlterCancelException {
        List<Long> shadowIdxList =
                indexIdMap.keySet().stream().collect(Collectors.toList());
        try {
            ((CloudInternalCatalog) Env.getCurrentInternalCatalog())
                    .commitMaterializedIndex(dbId, tableId, shadowIdxList, false);
        } catch (Exception e) {
            LOG.warn("commitMaterializedIndex exception:", e);
            throw new AlterCancelException(e.getMessage());
        }
        LOG.info("commitShadowIndex finished, dbId:{}, tableId:{}, jobId:{}, shadowIdxList:{}",
                dbId, tableId, jobId, shadowIdxList);
    }

    @Override
    protected void onCancel() {
        if (Config.enable_check_compatibility_mode) {
            LOG.info("skip drop shadown indexes in checking compatibility mode");
            return;
        }

        List<Long> shadowIdxList = indexIdMap.keySet().stream().collect(Collectors.toList());
        dropIndex(shadowIdxList);

        long tryTimes = 1;
        while (true) {
            try {
                Set<Table.Cell<Long, Long, Map<Long, Long>>> tableSet = partitionIndexTabletMap.cellSet();
                Iterator<Cell<Long, Long, Map<Long, Long>>> it = tableSet.iterator();
                while (it.hasNext()) {
                    Table.Cell<Long, Long, Map<Long, Long>> data = it.next();
                    Long partitionId = data.getRowKey();
                    Long shadowIndexId = data.getColumnKey();
                    Long originIndexId = indexIdMap.get(shadowIndexId);
                    Map<Long, Long> shadowTabletIdToOriginTabletId = data.getValue();
                    for (Map.Entry<Long, Long> entry : shadowTabletIdToOriginTabletId.entrySet()) {
                        Long shadowTabletId = entry.getKey();
                        Long originTabletId = entry.getValue();
                        ((CloudInternalCatalog) Env.getCurrentInternalCatalog())
                                .removeSchemaChangeJob(jobId, dbId, tableId, originIndexId, shadowIndexId,
                                        partitionId, originTabletId, shadowTabletId);
                    }
                    LOG.info("Cancel SchemaChange. Remove SchemaChangeJob in ms."
                            + "dbId:{}, tableId:{}, originIndexId:{}, partitionId:{}. tabletSize:{}",
                            dbId, tableId, originIndexId, partitionId, shadowTabletIdToOriginTabletId.size());
                }
                break;
            } catch (Exception e) {
                LOG.warn("tryTimes:{}, onCancel exception:", tryTimes, e);
            }
            sleepSeveralSeconds();
            tryTimes++;
        }
    }

    @Override
    protected void postProcessOriginIndex() {
        if (Config.enable_check_compatibility_mode) {
            LOG.info("skip drop origin indexes in checking compatibility mode");
            return;
        }

        List<Long> originIdxList = indexIdMap.values().stream().collect(Collectors.toList());
        dropIndex(originIdxList);
    }

    private void dropIndex(List<Long> idxList) {
        int tryTimes = 1;
        while (true) {
            try {
                ((CloudInternalCatalog) Env.getCurrentInternalCatalog())
                    .dropMaterializedIndex(tableId, idxList, false);
                break;
            } catch (Exception e) {
                LOG.warn("drop index failed, retry times {}, dbId: {}, tableId: {}, jobId: {}, idxList: {}:",
                        tryTimes, dbId, tableId, jobId, idxList, e);
            }
            sleepSeveralSeconds();
            tryTimes++;
        }

        LOG.info("dropIndex finished, dbId:{}, tableId:{}, jobId:{}, IdxList:{}",
                dbId, tableId, jobId, idxList);
    }

    @Override
    protected void createShadowIndexReplica() throws AlterCancelException {
        Database db = Env.getCurrentInternalCatalog()
                .getDbOrException(dbId, s -> new AlterCancelException("Database " + s + " does not exist"));

        // 1. create replicas
        OlapTable tbl;
        try {
            tbl = (OlapTable) db.getTableOrMetaException(tableId, TableType.OLAP);
        } catch (MetaNotFoundException e) {
            throw new AlterCancelException(e.getMessage());
        }

        Long expiration = (createTimeMs + timeoutMs) / 1000;
        tbl.readLock();
        try {
            Preconditions.checkState(tbl.getState() == OlapTableState.SCHEMA_CHANGE);
            try {
                List<Long> shadowIdxList = indexIdMap.keySet().stream().collect(Collectors.toList());
                ((CloudInternalCatalog) Env.getCurrentInternalCatalog())
                        .prepareMaterializedIndex(tableId, shadowIdxList,
                        expiration);
                createShadowIndexReplicaForPartition(tbl);
            } catch (Exception e) {
                LOG.warn("createCloudShadowIndexReplica Exception:", e);
                throw new AlterCancelException(e.getMessage());
            }

        } finally {
            tbl.readUnlock();
        }

        // create all replicas success.
        // add all shadow indexes to catalog
        tbl.writeLockOrAlterCancelException();
        try {
            Preconditions.checkState(tbl.getState() == OlapTableState.SCHEMA_CHANGE);
            addShadowIndexToCatalog(tbl);
        } finally {
            tbl.writeUnlock();
        }
    }

    private void createShadowIndexReplicaForPartition(OlapTable tbl) throws Exception {
        for (long partitionId : partitionIndexMap.rowKeySet()) {
            Partition partition = tbl.getPartition(partitionId);
            if (partition == null) {
                continue;
            }
            Map<Long, MaterializedIndex> shadowIndexMap = partitionIndexMap.row(partitionId);
            for (Map.Entry<Long, MaterializedIndex> entry : shadowIndexMap.entrySet()) {
                long shadowIdxId = entry.getKey();
                MaterializedIndex shadowIdx = entry.getValue();

                short shadowShortKeyColumnCount = indexShortKeyMap.get(shadowIdxId);
                List<Column> shadowSchema = indexSchemaMap.get(shadowIdxId);
                List<Integer> clusterKeyUids = null;
                if (shadowIdxId == tbl.getBaseIndexId() || isShadowIndexOfBase(shadowIdxId, tbl)) {
                    clusterKeyUids = OlapTable.getClusterKeyUids(shadowSchema);
                }
                int shadowSchemaHash = indexSchemaVersionAndHashMap.get(shadowIdxId).schemaHash;
                int shadowSchemaVersion = indexSchemaVersionAndHashMap.get(shadowIdxId).schemaVersion;
                long originIndexId = indexIdMap.get(shadowIdxId);
                KeysType originKeysType = tbl.getKeysTypeByIndexId(originIndexId);
                List<Index> tabletIndexes = originIndexId == tbl.getBaseIndexId() ? indexes : null;

                Cloud.CreateTabletsRequest.Builder requestBuilder =
                        Cloud.CreateTabletsRequest.newBuilder();
                for (Tablet shadowTablet : shadowIdx.getTablets()) {
                    OlapFile.TabletMetaCloudPB.Builder builder =
                            ((CloudInternalCatalog) Env.getCurrentInternalCatalog())
                                    .createTabletMetaBuilder(tableId, shadowIdxId,
                                            partitionId, shadowTablet,
                                            tbl.getPartitionInfo().getTabletType(partitionId),
                                            shadowSchemaHash, originKeysType, shadowShortKeyColumnCount, bfColumns,
                                            bfFpp, tabletIndexes, shadowSchema, tbl.getDataSortInfo(),
                                            tbl.getCompressionType(),
                                            tbl.getStoragePolicy(), tbl.isInMemory(), true,
                                            tbl.getName(), tbl.getTTLSeconds(),
                                            tbl.getEnableUniqueKeyMergeOnWrite(), tbl.storeRowColumn(),
                                            shadowSchemaVersion, tbl.getCompactionPolicy(),
                                            tbl.getTimeSeriesCompactionGoalSizeMbytes(),
                                            tbl.getTimeSeriesCompactionFileCountThreshold(),
                                            tbl.getTimeSeriesCompactionTimeThresholdSeconds(),
                                            tbl.getTimeSeriesCompactionEmptyRowsetsThreshold(),
                                            tbl.getTimeSeriesCompactionLevelThreshold(),
                                            tbl.disableAutoCompaction(),
                                            tbl.getRowStoreColumnsUniqueIds(rowStoreColumns),
                                            tbl.getInvertedIndexFileStorageFormat(),
                                            tbl.rowStorePageSize(),
                                            tbl.variantEnableFlattenNested(), clusterKeyUids,
                                            tbl.storagePageSize(),
                                            tbl.storageDictPageSize(), true);
                    requestBuilder.addTabletMetas(builder);
                } // end for rollupTablets
                requestBuilder.setDbId(dbId);
                ((CloudInternalCatalog) Env.getCurrentInternalCatalog())
                        .sendCreateTabletsRpc(requestBuilder);
            }
        }
    }

    @Override
    protected void ensureCloudClusterExist(List<AgentTask> tasks) throws AlterCancelException {
        if (((CloudSystemInfoService) Env.getCurrentSystemInfo())
                .getCloudClusterIdByName(cloudClusterName) == null) {
            for (AgentTask task : tasks) {
                task.setFinished(true);
                AgentTaskQueue.removeTask(task.getBackendId(), TTaskType.ALTER, task.getSignature());
            }
            StringBuilder sb = new StringBuilder("cloud cluster(");
            sb.append(cloudClusterName);
            sb.append(") has been removed, jobId=");
            sb.append(jobId);
            String msg = sb.toString();
            LOG.warn(msg);
            throw new AlterCancelException(msg);
        }
    }

    @Override
    protected boolean checkTableStable(Database db) throws AlterCancelException {
        return true;
    }
}
