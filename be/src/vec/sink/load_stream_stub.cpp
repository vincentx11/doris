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

#include "vec/sink/load_stream_stub.h"

#include <sstream>

#include "common/cast_set.h"
#include "olap/rowset/rowset_writer.h"
#include "runtime/query_context.h"
#include "util/brpc_client_cache.h"
#include "util/debug_points.h"
#include "util/network_util.h"
#include "util/thrift_util.h"
#include "util/uid_util.h"

namespace doris {
#include "common/compile_check_begin.h"

int LoadStreamReplyHandler::on_received_messages(brpc::StreamId id, butil::IOBuf* const messages[],
                                                 size_t size) {
    auto stub = _stub.lock();
    if (!stub) {
        LOG(WARNING) << "stub is not exist when on_received_messages, " << *this
                     << ", stream_id=" << id;
        return 0;
    }
    for (size_t i = 0; i < size; i++) {
        butil::IOBufAsZeroCopyInputStream wrapper(*messages[i]);
        PLoadStreamResponse response;
        response.ParseFromZeroCopyStream(&wrapper);

        if (response.eos()) {
            stub->_is_eos.store(true);
        }

        Status st = Status::create<false>(response.status());

        std::stringstream ss;
        ss << "on_received_messages, " << *this << ", stream_id=" << id;
        if (response.success_tablet_ids_size() > 0) {
            ss << ", success tablet ids:";
            for (auto tablet_id : response.success_tablet_ids()) {
                ss << " " << tablet_id;
            }
            std::lock_guard<bthread::Mutex> lock(stub->_success_tablets_mutex);
            for (auto tablet_id : response.success_tablet_ids()) {
                stub->_success_tablets.push_back(tablet_id);
            }
        }
        if (response.failed_tablets_size() > 0) {
            ss << ", failed tablet ids:";
            for (auto pb : response.failed_tablets()) {
                ss << " " << pb.id() << ":" << Status::create(pb.status());
            }
            std::lock_guard<bthread::Mutex> lock(stub->_failed_tablets_mutex);
            for (auto pb : response.failed_tablets()) {
                stub->_failed_tablets.emplace(pb.id(), Status::create(pb.status()));
            }
        }
        if (response.tablet_schemas_size() > 0) {
            ss << ", tablet schema num: " << response.tablet_schemas_size();
            std::lock_guard<bthread::Mutex> lock(stub->_schema_mutex);
            for (const auto& schema : response.tablet_schemas()) {
                auto tablet_schema = std::make_unique<TabletSchema>();
                tablet_schema->init_from_pb(schema.tablet_schema());
                stub->_tablet_schema_for_index->emplace(schema.index_id(),
                                                        std::move(tablet_schema));
                stub->_enable_unique_mow_for_index->emplace(
                        schema.index_id(), schema.enable_unique_key_merge_on_write());
            }
            stub->_schema_cv.notify_all();
        }
        ss << ", status: " << st;
        LOG(INFO) << ss.str();

        if (response.has_load_stream_profile()) {
            TRuntimeProfileTree tprofile;
            const uint8_t* buf =
                    reinterpret_cast<const uint8_t*>(response.load_stream_profile().data());
            uint32_t len = cast_set<uint32_t>(response.load_stream_profile().size());
            auto status = deserialize_thrift_msg(buf, &len, false, &tprofile);
            if (status.ok()) {
                // TODO
                //_sink->_state->load_channel_profile()->update(tprofile);
            } else {
                LOG(WARNING) << "load stream TRuntimeProfileTree deserialize failed, errmsg="
                             << status;
            }
        }
    }
    return 0;
}

void LoadStreamReplyHandler::on_closed(brpc::StreamId id) {
    Defer defer {[this]() { delete this; }};
    LOG(INFO) << "on_closed, " << *this << ", stream_id=" << id;
    auto stub = _stub.lock();
    if (!stub) {
        LOG(WARNING) << "stub is not exist when on_closed, " << *this;
        return;
    }
    stub->_is_closed.store(true);
}

inline std::ostream& operator<<(std::ostream& ostr, const LoadStreamReplyHandler& handler) {
    ostr << "LoadStreamReplyHandler load_id=" << UniqueId(handler._load_id)
         << ", dst_id=" << handler._dst_id;
    return ostr;
}

LoadStreamStub::LoadStreamStub(PUniqueId load_id, int64_t src_id,
                               std::shared_ptr<IndexToTabletSchema> schema_map,
                               std::shared_ptr<IndexToEnableMoW> mow_map, bool incremental)
        : _load_id(load_id),
          _src_id(src_id),
          _tablet_schema_for_index(schema_map),
          _enable_unique_mow_for_index(mow_map),
          _is_incremental(incremental) {};

LoadStreamStub::~LoadStreamStub() {
    if (_is_open.load() && !_is_closed.load()) {
        auto ret = brpc::StreamClose(_stream_id);
        LOG(INFO) << *this << " is deconstructed, close " << (ret == 0 ? "success" : "failed");
    }
}

// open_load_stream
Status LoadStreamStub::open(BrpcClientCache<PBackendService_Stub>* client_cache,
                            const NodeInfo& node_info, int64_t txn_id,
                            const OlapTableSchemaParam& schema,
                            const std::vector<PTabletID>& tablets_for_schema, int total_streams,
                            int64_t idle_timeout_ms, bool enable_profile) {
    std::unique_lock<bthread::Mutex> lock(_open_mutex);
    if (_is_init.load()) {
        return _status;
    }
    _is_init.store(true);
    _dst_id = node_info.id;
    brpc::StreamOptions opt;
    opt.max_buf_size = cast_set<int>(config::load_stream_max_buf_size);
    opt.idle_timeout_ms = idle_timeout_ms;
    opt.messages_in_batch = config::load_stream_messages_in_batch;
    opt.handler = new LoadStreamReplyHandler(_load_id, _dst_id, shared_from_this());
    brpc::Controller cntl;
    if (int ret = brpc::StreamCreate(&_stream_id, cntl, &opt)) {
        delete opt.handler;
        _status = Status::Error<true>(ret, "Failed to create stream");
        return _status;
    }
    cntl.set_timeout_ms(config::open_load_stream_timeout_ms);
    POpenLoadStreamRequest request;
    *request.mutable_load_id() = _load_id;
    request.set_src_id(_src_id);
    request.set_txn_id(txn_id);
    request.set_enable_profile(enable_profile);
    if (_is_incremental) {
        request.set_total_streams(0);
    } else if (total_streams > 0) {
        request.set_total_streams(total_streams);
    } else {
        _status = Status::InternalError("total_streams should be greator than 0");
        return _status;
    }
    request.set_idle_timeout_ms(idle_timeout_ms);
    schema.to_protobuf(request.mutable_schema());
    for (auto& tablet : tablets_for_schema) {
        *request.add_tablets() = tablet;
    }
    POpenLoadStreamResponse response;
    // set connection_group "streaming" to distinguish with non-streaming connections
    const auto& stub = client_cache->get_client(node_info.host, node_info.brpc_port);
    if (stub == nullptr) {
        return Status::InternalError("failed to init brpc client to {}:{}", node_info.host,
                                     node_info.brpc_port);
    }
    stub->open_load_stream(&cntl, &request, &response, nullptr);
    for (const auto& resp : response.tablet_schemas()) {
        auto tablet_schema = std::make_unique<TabletSchema>();
        tablet_schema->init_from_pb(resp.tablet_schema());
        _tablet_schema_for_index->emplace(resp.index_id(), std::move(tablet_schema));
        _enable_unique_mow_for_index->emplace(resp.index_id(),
                                              resp.enable_unique_key_merge_on_write());
    }
    if (cntl.Failed()) {
        brpc::StreamClose(_stream_id);
        _status = Status::InternalError("Failed to connect to backend {}: {}", _dst_id,
                                        cntl.ErrorText());
        return _status;
    }
    LOG(INFO) << "open load stream to host=" << node_info.host << ", port=" << node_info.brpc_port
              << ", " << *this;
    _is_open.store(true);
    _status = Status::OK();
    return _status;
}

// APPEND_DATA
Status LoadStreamStub::append_data(int64_t partition_id, int64_t index_id, int64_t tablet_id,
                                   int32_t segment_id, uint64_t offset, std::span<const Slice> data,
                                   bool segment_eos, FileType file_type) {
    if (!_is_open.load()) {
        add_failed_tablet(tablet_id, _status);
        return _status;
    }
    DBUG_EXECUTE_IF("LoadStreamStub.skip_send_segment", { return Status::OK(); });
    PStreamHeader header;
    header.set_src_id(_src_id);
    *header.mutable_load_id() = _load_id;
    header.set_partition_id(partition_id);
    header.set_index_id(index_id);
    header.set_tablet_id(tablet_id);
    header.set_segment_id(segment_id);
    header.set_segment_eos(segment_eos);
    header.set_offset(offset);
    header.set_opcode(doris::PStreamHeader::APPEND_DATA);
    header.set_file_type(file_type);
    return _encode_and_send(header, data);
}

// ADD_SEGMENT
Status LoadStreamStub::add_segment(int64_t partition_id, int64_t index_id, int64_t tablet_id,
                                   int32_t segment_id, const SegmentStatistics& segment_stat,
                                   TabletSchemaSPtr flush_schema) {
    if (!_is_open.load()) {
        add_failed_tablet(tablet_id, _status);
        return _status;
    }
    DBUG_EXECUTE_IF("LoadStreamStub.skip_send_segment", { return Status::OK(); });
    PStreamHeader header;
    header.set_src_id(_src_id);
    *header.mutable_load_id() = _load_id;
    header.set_partition_id(partition_id);
    header.set_index_id(index_id);
    header.set_tablet_id(tablet_id);
    header.set_segment_id(segment_id);
    header.set_opcode(doris::PStreamHeader::ADD_SEGMENT);
    segment_stat.to_pb(header.mutable_segment_statistics());
    if (flush_schema != nullptr) {
        flush_schema->to_schema_pb(header.mutable_flush_schema());
    }
    return _encode_and_send(header);
}

// CLOSE_LOAD
Status LoadStreamStub::close_load(const std::vector<PTabletID>& tablets_to_commit) {
    if (!_is_open.load()) {
        return _status;
    }
    PStreamHeader header;
    *header.mutable_load_id() = _load_id;
    header.set_src_id(_src_id);
    header.set_opcode(doris::PStreamHeader::CLOSE_LOAD);
    for (const auto& tablet : tablets_to_commit) {
        *header.add_tablets() = tablet;
    }
    _status = _encode_and_send(header);
    if (!_status.ok()) {
        LOG(WARNING) << "stream " << _stream_id << " close failed: " << _status;
        return _status;
    }
    _is_closing.store(true);
    return Status::OK();
}

// GET_SCHEMA
Status LoadStreamStub::get_schema(const std::vector<PTabletID>& tablets) {
    if (!_is_open.load()) {
        return _status;
    }
    PStreamHeader header;
    *header.mutable_load_id() = _load_id;
    header.set_src_id(_src_id);
    header.set_opcode(doris::PStreamHeader::GET_SCHEMA);
    std::ostringstream oss;
    oss << "fetching tablet schema from stream " << _stream_id
        << ", load id: " << print_id(_load_id) << ", tablet id:";
    for (const auto& tablet : tablets) {
        *header.add_tablets() = tablet;
        oss << " " << tablet.tablet_id();
    }
    if (tablets.size() == 0) {
        oss << " none";
    }
    LOG(INFO) << oss.str();
    return _encode_and_send(header);
}

Status LoadStreamStub::wait_for_schema(int64_t partition_id, int64_t index_id, int64_t tablet_id,
                                       int64_t timeout_ms) {
    if (!_is_open.load()) {
        return _status;
    }
    if (_tablet_schema_for_index->contains(index_id)) {
        return Status::OK();
    }
    PTabletID tablet;
    tablet.set_partition_id(partition_id);
    tablet.set_index_id(index_id);
    tablet.set_tablet_id(tablet_id);
    RETURN_IF_ERROR(get_schema({tablet}));

    MonotonicStopWatch watch;
    watch.start();
    while (!_tablet_schema_for_index->contains(index_id) &&
           watch.elapsed_time() / 1000 / 1000 < timeout_ms) {
        RETURN_IF_ERROR(check_cancel());
        static_cast<void>(wait_for_new_schema(100));
    }

    if (!_tablet_schema_for_index->contains(index_id)) {
        return Status::TimedOut("timeout to get tablet schema for index {}", index_id);
    }
    return Status::OK();
}

Status LoadStreamStub::close_finish_check(RuntimeState* state, bool* is_closed) {
    DBUG_EXECUTE_IF("LoadStreamStub::close_wait.long_wait", DBUG_BLOCK);
    DBUG_EXECUTE_IF("LoadStreamStub::close_finish_check.close_failed",
                    { return Status::InternalError("close failed"); });
    *is_closed = true;
    if (!_is_open.load()) {
        // we don't need to close wait on non-open streams
        return Status::OK();
    }
    if (state->get_query_ctx()->is_cancelled()) {
        return state->get_query_ctx()->exec_status();
    }
    if (!_is_closing.load()) {
        *is_closed = false;
        return _status;
    }
    if (_is_closed.load()) {
        RETURN_IF_ERROR(check_cancel());
        if (!_is_eos.load()) {
            return Status::InternalError("Stream closed without EOS, {}", to_string());
        }
        return Status::OK();
    }
    *is_closed = false;
    return Status::OK();
}

void LoadStreamStub::cancel(Status reason) {
    LOG(WARNING) << *this << " is cancelled because of " << reason;
    if (_is_open.load()) {
        brpc::StreamClose(_stream_id);
    }
    {
        std::lock_guard<bthread::Mutex> lock(_cancel_mutex);
        _cancel_st = reason;
        _is_cancelled.store(true);
    }
    _is_closed.store(true);
}

Status LoadStreamStub::_encode_and_send(PStreamHeader& header, std::span<const Slice> data) {
    butil::IOBuf buf;
    size_t header_len = header.ByteSizeLong();
    buf.append(reinterpret_cast<uint8_t*>(&header_len), sizeof(header_len));
    buf.append(header.SerializeAsString());
    size_t data_len = std::transform_reduce(data.begin(), data.end(), 0, std::plus(),
                                            [](const Slice& s) { return s.get_size(); });
    buf.append(reinterpret_cast<uint8_t*>(&data_len), sizeof(data_len));
    for (const auto& slice : data) {
        buf.append(slice.get_data(), slice.get_size());
    }
    bool eos = header.opcode() == doris::PStreamHeader::CLOSE_LOAD;
    bool get_schema = header.opcode() == doris::PStreamHeader::GET_SCHEMA;
    add_bytes_written(buf.size());
    return _send_with_buffer(buf, eos || get_schema);
}

Status LoadStreamStub::_send_with_buffer(butil::IOBuf& buf, bool sync) {
    butil::IOBuf output;
    std::unique_lock<decltype(_buffer_mutex)> buffer_lock(_buffer_mutex);
    _buffer.append(buf);
    if (!sync && _buffer.size() < config::brpc_streaming_client_batch_bytes) {
        return Status::OK();
    }
    output.swap(_buffer);
    // acquire send lock while holding buffer lock, to ensure the message order
    std::lock_guard<decltype(_send_mutex)> send_lock(_send_mutex);
    buffer_lock.unlock();
    VLOG_DEBUG << "send buf size : " << output.size() << ", sync: " << sync;
    auto st = _send_with_retry(output);
    if (!st.ok()) {
        _handle_failure(output, st);
    }
    return st;
}

void LoadStreamStub::_handle_failure(butil::IOBuf& buf, Status st) {
    while (buf.size() > 0) {
        // step 1: parse header
        size_t hdr_len = 0;
        buf.cutn((void*)&hdr_len, sizeof(size_t));
        butil::IOBuf hdr_buf;
        PStreamHeader hdr;
        buf.cutn(&hdr_buf, hdr_len);
        butil::IOBufAsZeroCopyInputStream wrapper(hdr_buf);
        hdr.ParseFromZeroCopyStream(&wrapper);

        // step 2: cut data
        size_t data_len = 0;
        buf.cutn((void*)&data_len, sizeof(size_t));
        butil::IOBuf data_buf;
        buf.cutn(&data_buf, data_len);

        // step 3: handle failure
        switch (hdr.opcode()) {
        case PStreamHeader::ADD_SEGMENT:
        case PStreamHeader::APPEND_DATA: {
            DBUG_EXECUTE_IF("LoadStreamStub._handle_failure.append_data_failed", {
                add_failed_tablet(hdr.tablet_id(), st);
                return;
            });
            DBUG_EXECUTE_IF("LoadStreamStub._handle_failure.add_segment_failed", {
                add_failed_tablet(hdr.tablet_id(), st);
                return;
            });
            add_failed_tablet(hdr.tablet_id(), st);
        } break;
        case PStreamHeader::CLOSE_LOAD: {
            DBUG_EXECUTE_IF("LoadStreamStub._handle_failure.close_load_failed", {
                brpc::StreamClose(_stream_id);
                return;
            });
            brpc::StreamClose(_stream_id);
        } break;
        case PStreamHeader::GET_SCHEMA: {
            DBUG_EXECUTE_IF("LoadStreamStub._handle_failure.get_schema_failed", {
                // Just log and let wait_for_schema timeout
                std::ostringstream oss;
                for (const auto& tablet : hdr.tablets()) {
                    oss << " " << tablet.tablet_id();
                }
                LOG(WARNING) << "failed to send GET_SCHEMA request, tablet_id:" << oss.str() << ", "
                             << *this;
                return;
            });
            // Just log and let wait_for_schema timeout
            std::ostringstream oss;
            for (const auto& tablet : hdr.tablets()) {
                oss << " " << tablet.tablet_id();
            }
            LOG(WARNING) << "failed to send GET_SCHEMA request, tablet_id:" << oss.str() << ", "
                         << *this;
        } break;
        default:
            LOG(WARNING) << "unexpected stream message " << hdr.opcode() << ", " << *this;
            DCHECK(false);
        }
    }
}

Status LoadStreamStub::_send_with_retry(butil::IOBuf& buf) {
    for (;;) {
        RETURN_IF_ERROR(check_cancel());
        int ret;
        {
            DBUG_EXECUTE_IF("LoadStreamStub._send_with_retry.delay_before_send", {
                int64_t delay_ms = dp->param<int64_t>("delay_ms", 1000);
                bthread_usleep(delay_ms * 1000);
            });
            brpc::StreamWriteOptions options;
            options.write_in_background = config::enable_brpc_stream_write_background;
            ret = brpc::StreamWrite(_stream_id, buf, &options);
        }
        DBUG_EXECUTE_IF("LoadStreamStub._send_with_retry.stream_write_failed", { ret = EPIPE; });
        switch (ret) {
        case 0:
            return Status::OK();
        case EAGAIN: {
            const timespec time = butil::seconds_from_now(config::load_stream_eagain_wait_seconds);
            int wait_ret = brpc::StreamWait(_stream_id, &time);
            if (wait_ret != 0) {
                return Status::InternalError("StreamWait failed, err={}, {}", wait_ret,
                                             to_string());
            }
            break;
        }
        default:
            return Status::InternalError("StreamWrite failed, err={}, {}", ret, to_string());
        }
    }
}

std::string LoadStreamStub::to_string() {
    std::ostringstream ss;
    ss << *this;
    return ss.str();
}

inline std::ostream& operator<<(std::ostream& ostr, const LoadStreamStub& stub) {
    ostr << "LoadStreamStub load_id=" << print_id(stub._load_id) << ", src_id=" << stub._src_id
         << ", dst_id=" << stub._dst_id << ", stream_id=" << stub._stream_id;
    return ostr;
}

Status LoadStreamStubs::open(BrpcClientCache<PBackendService_Stub>* client_cache,
                             const NodeInfo& node_info, int64_t txn_id,
                             const OlapTableSchemaParam& schema,
                             const std::vector<PTabletID>& tablets_for_schema, int total_streams,
                             int64_t idle_timeout_ms, bool enable_profile) {
    bool get_schema = true;
    auto status = Status::OK();
    for (auto& stream : _streams) {
        Status st;
        if (get_schema) {
            st = stream->open(client_cache, node_info, txn_id, schema, tablets_for_schema,
                              total_streams, idle_timeout_ms, enable_profile);
        } else {
            st = stream->open(client_cache, node_info, txn_id, schema, {}, total_streams,
                              idle_timeout_ms, enable_profile);
        }
        if (st.ok()) {
            get_schema = false;
        } else {
            LOG(WARNING) << "open stream failed: " << st << "; stream: " << *stream;
            status = st;
            // no break here to try get schema from the rest streams
        }
    }
    // only mark open when all streams open success
    _open_success.store(status.ok());
    // cancel all streams if open failed
    if (!status.ok()) {
        cancel(status);
    }
    return status;
}

Status LoadStreamStubs::close_load(const std::vector<PTabletID>& tablets_to_commit) {
    if (!_open_success.load()) {
        return Status::InternalError("streams not open");
    }
    bool first = true;
    auto status = Status::OK();
    for (auto& stream : _streams) {
        Status st;
        if (first) {
            st = stream->close_load(tablets_to_commit);
            first = false;
        } else {
            st = stream->close_load({});
        }
        if (!st.ok()) {
            LOG(WARNING) << "close_load failed: " << st << "; stream: " << *stream;
        }
    }
    return status;
}

} // namespace doris
