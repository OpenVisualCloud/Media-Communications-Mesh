/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CONN_H
#define CONN_H

#include <atomic>
#include <cstddef>
#include "buf.h"
#include "concurrency.h"
#include "metrics.h"
#include "sdk.grpc.pb.h"
#include "sync.h"

namespace mesh::connection {

/**
 * Kind
 * 
 * Definition of connection kinds.
 */
enum class Kind {
    undefined,
    transmitter,
    receiver,
};

/**
 * State
 * 
 * Definition of connection states.
 */
enum class State {
    not_configured, // set in ctor after initialization
    configured,
    establishing,
    active,
    suspended,
    closing,
    closed,
    deleting,       // set in dtor before deinitialization
};

/**
 * Status
 * 
 * Definition of connection statuses.
 */
enum class Status {
    initial,    // reported by the base Connection class
    transition, // reported by the base Connection class
    healthy,    // must be reported by the derived class
    failure,    // must be reported by the derived class
    shutdown,   // reported by the base Connection class
};

/**
 * Result
 * 
 * Definition of connection result options.
 */
enum class Result {
    success,
    error_not_supported,
    error_wrong_state,
    error_no_link_assigned,
    error_bad_argument,
    error_out_of_memory,
    error_general_failure,
    error_context_cancelled,
    error_conn_config_invalid,
    error_buf_config_invalid,
    error_payload_config_invalid,

    error_already_initialized,
    error_initialization_failed,
    error_memory_registration_failed,
    error_thread_creation_failed,
    error_no_buffer,
    error_timeout,
    // TODO: more error codes to be added...
};

/**
 * ConnectionType
 * 
 * Enum defining types of SDK created connections.
 */
enum ConnectionType : int {
// TODO: Remove CONN_TYPE_MEMIF when API update is completed.
//  CONN_TYPE_MEMIF  = 0, ///< Single node direct connection via memif
    CONN_TYPE_GROUP  = 1, ///< Local connection to Multipoint Group
    CONN_TYPE_ST2110 = 2, ///< SMPTE ST2110-xx connection via Media Proxy
    CONN_TYPE_RDMA   = 3, ///< RDMA connection via Media Proxy
};

/**
 * PayloadType
 * 
 * Enum defining types of payload in SDK created connections.
 */
enum PayloadType : int {
    PAYLOAD_TYPE_BLOB  = 0, ///< payload: blob arbitrary data
    PAYLOAD_TYPE_VIDEO = 1, ///< payload: video frames
    PAYLOAD_TYPE_AUDIO = 2, ///< payload: audio packets
};

/**
 * Config
 * 
 * Configuration structure holding basic connection parameters.
 * Initially, the connection configuration is parsed from user provided JSON
 * in SDK, then transmitted to Media Proxy via gRPC, and then transmitted to
 * MCM Agent.
 */
class Config {
public:
    Result assign_from_pb(const sdk::ConnectionConfig& config);
    void assign_to_pb(sdk::ConnectionConfig& config) const;
    void copy_buf_parts_from(const Config& config);

    sdk::ConnectionKind kind;

    uint16_t buf_queue_capacity;
    uint32_t max_payload_size;
    uint32_t max_metadata_size;

    uint32_t calculated_payload_size;

    BufferPartitions buf_parts;

    ConnectionType conn_type;

    struct {
        struct {
            std::string urn; 
        } multipoint_group;

        struct {
            std::string ip_addr;
            uint16_t port;
            std::string mcast_sip_addr;

            sdk::ST2110Transport transport;

            std::string pacing;
            uint32_t payload_type;
        } st2110;

        struct {
            std::string connection_mode;
            uint32_t max_latency_ns;
        } rdma;
    } conn;

    struct {
        std::string engine;

        struct {
            std::string provider;
            uint16_t num_endpoints;
        } rdma;
    } options;

    PayloadType payload_type;

    struct {
        struct {
            int width;
            int height;
            double fps;
            sdk::VideoPixelFormat pixel_format;
        } video;

        struct {
            int channels;
            sdk::AudioSampleRate sample_rate;
            sdk::AudioFormat format;
            sdk::AudioPacketTime packet_time;
        } audio;

    } payload;

    const char * kind2str() const;
    const char * conn_type2str() const;
    const char * st2110_transport2str() const;
    const char * payload_type2str() const;
    const char * video_pixel_format2str() const;
    const char * audio_sample_rate2str() const;
    const char * audio_format2str() const;
    const char * audio_packet_time2str() const;
};

/**
 * Connection
 * 
 * Base abstract class of connection. All connection implementations must
 * inherit this class.
 */
class Connection : public telemetry::MetricsProvider {

public:
    Connection();
    virtual ~Connection();

    Kind kind();
    State state();
    Status status();
    const std::string& name();

    virtual Result set_link(context::Context& ctx, Connection *new_link,
                            Connection *requester = nullptr);
    Connection * link();

    void set_config(const Config& cfg);
    void set_parent(const std::string& parent_id);
    void set_name(const std::string& name);

    void log_dump_config();

    void notify_parent_conn_unlink_requested(context::Context& ctx);

    Result establish(context::Context& ctx);
    Result establish_async(context::Context& ctx);

    Result suspend(context::Context& ctx);
    Result resume(context::Context& ctx);

    Result shutdown(context::Context& ctx);
    Result shutdown_async(context::Context& ctx,
                          std::function<void()> on_shutdown_complete = nullptr);

    Result do_receive(context::Context& ctx, void *ptr, uint32_t sz,
                      uint32_t& sent);

    // TODO: add calls to reset metrics (counters).

    struct {
        // TODO: add timestamp created_at
        // TODO: add timestamp established_at
    } info;

    Config config;
    std::string legacy_sdk_id;

protected:
    void set_state(context::Context& ctx, State new_state);
    void set_status(context::Context& ctx, Status new_status);
    Result set_result(Result res);

    Result transmit(context::Context& ctx, void *ptr, uint32_t sz);

    virtual Result on_establish(context::Context& ctx) = 0;
    virtual Result on_shutdown(context::Context& ctx) = 0;
    virtual Result on_resume(context::Context& ctx);
    virtual Result on_receive(context::Context& ctx, void *ptr, uint32_t sz,
                              uint32_t& sent);
    virtual void on_delete(context::Context& ctx) {}

    Kind _kind = Kind::undefined; // must be properly set in the derived class ctor

    sync::DataplaneAtomicPtr dp_link;

    struct {
        std::atomic<uint64_t> inbound_bytes;
        std::atomic<uint64_t> outbound_bytes;
        std::atomic<uint32_t> transactions_succeeded;
        std::atomic<uint32_t> transactions_failed;
        std::atomic<uint32_t> errors;

        int64_t  prev_timestamp_ms;
        uint64_t prev_inbound_bytes;
        uint64_t prev_outbound_bytes;
        uint32_t prev_errors;
        uint32_t prev_transactions_succeeded;
    } metrics;

private:
    virtual void collect(telemetry::Metric& metric, const int64_t& timestamp_ms);

    std::atomic<State> _state = State::not_configured;
    std::atomic<Status> _status = Status::initial;

    context::Context establish_ctx = context::WithCancel(context::Background());
    std::jthread establish_th;
    std::jthread shutdown_th;
    std::string parent_id;
    std::string _name;
};

const char * kind2str(Kind kind, bool brief = false);
const char * state2str(State state);
const char * status2str(Status status);
const char * result2str(Result res);

} // namespace mesh::connection

#endif // CONN_H
