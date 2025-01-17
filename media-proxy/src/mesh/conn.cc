/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "conn.h"
#include <cstring>
#include "logger.h"

namespace mesh::connection {

Connection::Connection()
{
    std::memset(&metrics, 0, sizeof(metrics));
}

Connection::~Connection()
{
    auto ctx = context::WithTimeout(context::Background(),
                                    std::chrono::milliseconds(5000));
    set_state(ctx, State::deleting);
    on_delete(ctx);
}

Kind Connection::kind()
{
    return _kind;
}

void Connection::set_state(context::Context& ctx, State new_state)
{
    State old_state = _state.exchange(new_state, std::memory_order_release);
    if (old_state != new_state) {
        // TODO: generate an Event. Use context to cancel sending the Event.
    }
}

State Connection::state()
{
    return _state.load(std::memory_order_acquire);
}

void Connection::set_status(context::Context& ctx, Status new_status)
{
    Status old_status = _status.exchange(new_status, std::memory_order_release);
    if (old_status != new_status) {
        // TODO: generate an Events. Use context to cancel sending the Event.
    }
}

Status Connection::status()
{
    // TODO: Rethink the logic here.

    switch (state()) {
    case State::not_configured:
    case State::configured:
        return Status::initial;
    case State::establishing:
    case State::closing:
        return Status::transition;
    case State::closed:
    case State::deleting:
        return Status::shutdown;
    default:
        return _status.load(std::memory_order_acquire);
    }
}

void Connection::set_config(const Config& cfg)
{
    config = cfg;

    log::debug("[SDK] Conn config")
              ("kind", config.kind2str())
              ("conn_type", config.conn_type2str())
              ("payload_type", config.payload_type2str())
              ("buf_queue_cap", config.buf_queue_capacity)
              ("max_payload_size", config.max_payload_size)
              ("max_metadata_size", config.max_metadata_size)
              ("calc_payload_size", config.calculated_payload_size);

    switch (config.conn_type) {
    case CONN_TYPE_GROUP:
        log::debug("[SDK] Multipoint group config")
                  ("urn", config.conn.multipoint_group.urn);
        break;
    case CONN_TYPE_ST2110:
        log::debug("[SDK] ST2110 config")
                  ("remote_ip_addr", config.conn.st2110.remote_ip_addr)
                  ("remote_port", config.conn.st2110.remote_port)
                  ("transport", config.st2110_transport2str())
                  ("pacing", config.conn.st2110.pacing)
                  ("payload_type", config.conn.st2110.payload_type);
        break;
    case CONN_TYPE_RDMA:
        log::debug("[SDK] RDMA config")
                  ("connection_mode", config.conn.rdma.connection_mode)
                  ("max_latency_ns", config.conn.rdma.max_latency_ns);
        break;
    }
    switch (config.payload_type) {
    case PAYLOAD_TYPE_VIDEO:
        log::debug("[SDK] Video config")
                  ("width", config.payload.video.width)
                  ("height", config.payload.video.height)
                  ("fps", config.payload.video.fps)
                  ("pixel_format", config.video_pixel_format2str());
        break;
    case PAYLOAD_TYPE_AUDIO:
        log::debug("[SDK] Audio config")
                  ("channels", config.payload.audio.channels)
                  ("sample_rate", config.audio_sample_rate2str())
                  ("format", config.audio_format2str())
                  ("packet_time", config.audio_packet_time2str());
        break;
    }
}

Result Connection::establish(context::Context& ctx)
{
    switch (state()) {
    case State::configured:
    case State::closed:
        set_state(ctx, State::establishing);
        return set_result(on_establish(ctx));
    default:
        return set_result(Result::error_wrong_state);
    }
}

Result Connection::establish_async(context::Context& ctx)
{
    switch (state()) {
    case State::configured:
    case State::closed:
        set_state(ctx, State::establishing);

        establish_ctx = context::WithCancel(ctx);

        try {
            establish_th = std::jthread([this]() {
                // log::debug("ON ESTABLISH THREAD START");
                auto res = on_establish(establish_ctx);
                if (res != Result::success)
                    log::error("Threaded on_establish() err: %s",
                               result2str(res));
                // log::debug("ON ESTABLISH THREAD EXIT");
            });
        }
        catch (const std::system_error& e) {
            log::error("Thread creation for on_establish() failed");
            return set_result(Result::error_out_of_memory);
        }
        return set_result(Result::success);

    default:
        return set_result(Result::error_wrong_state);
    }
}

Result Connection::suspend(context::Context& ctx)
{
    if (state() != State::active)
        return set_result(Result::error_wrong_state);

    set_state(ctx, State::suspended);
    return set_result(Result::success);
}

Result Connection::resume(context::Context& ctx)
{
    if (state() != State::suspended)
        return set_result(Result::error_wrong_state);

    set_state(ctx, State::active);
    return set_result(Result::success);
}

Result Connection::shutdown(context::Context& ctx)
{
    switch (state()) {
    case State::closed:
        return set_result(Result::success);
    case State::deleting:
        return set_result(Result::error_wrong_state);
    default:
        set_state(ctx, State::closing);
        return set_result(on_shutdown(ctx));
    }
}

Result Connection::shutdown_async(context::Context& ctx)
{
    switch (state()) {
    case State::closed:
        return set_result(Result::success);
    case State::deleting:
        return set_result(Result::error_wrong_state);
    default:
        set_state(ctx, State::closing);

        try {
            shutdown_th = std::jthread([this, &ctx]() {
                // log::debug("======= ON SHUTDOWN THREAD START");

                establish_ctx.cancel();
                if (establish_th.joinable()) {
                    // log::debug("ON SHUTDOWN THREAD - waiting for establish thread to exit");
                    establish_th.join();
                    // log::debug("ON SHUTDOWN THREAD - establish thread exited");
                }

                auto res = on_shutdown(ctx);
                if (res != Result::success)
                    log::error("Threaded on_shutdown() err: %s",
                               result2str(res));
                // log::debug("======= ON SHUTDOWN THREAD EXIT");
                shutdown_th.detach();
                delete this; // TODO: consider on making this safer
            });
        }
        catch (const std::system_error& e) {
            log::error("Thread creation for on_shutdown() failed");
            return set_result(Result::error_out_of_memory);
        }
        return set_result(Result::success);
    }
}

Result Connection::transmit(context::Context& ctx, void *ptr, uint32_t sz)
{
    // WARNING: This is the hot path of Data Plane.
    // Avoid any unnecessary operations that can increase latency.

    if (state() != State::active)
        return set_result(Result::error_wrong_state);

    if (!_link)
        return set_result(Result::error_no_link_assigned);

    metrics.inbound_bytes += sz;

    uint32_t sent = 0;
    Result res;

    // transmitting.store(true, std::memory_order_release);
    // setting_link.wait(true, std::memory_order_acquire);
    {
        const std::lock_guard<std::mutex> lk(link_mx);

        res = _link->do_receive(ctx, ptr, sz, sent);
    }
    // transmitting.store(false, std::memory_order_release);
    // transmitting.notify_all();

    // thread::Sleep(ctx, std::chrono::milliseconds(1));

    metrics.outbound_bytes += sent;

    if (res == Result::success)
        metrics.transactions_succeeded++;
    else
        metrics.transactions_failed++;

    return res;
}

Result Connection::do_receive(context::Context& ctx, void *ptr, uint32_t sz,
                              uint32_t& sent)
{
    // WARNING: This is the hot path of Data Plane.
    // Avoid any unnecessary operations that can increase latency.

    metrics.inbound_bytes += sz;

    if (state() != State::active)
        return Result::error_wrong_state;

    Result res = on_receive(ctx, ptr, sz, sent);

    metrics.outbound_bytes += sent;
    if (res == Result::success)
        metrics.transactions_succeeded++;
    else
        metrics.transactions_failed++;

    return res;
}

Result Connection::on_receive(context::Context& ctx, void *ptr, uint32_t sz,
                              uint32_t& sent)
{
    // This is the default implementation.
    // Derived Tx connection classes must override this method.
    //
    // WARNING: This is the hot path of Data Plane.
    // The method must return as soon as possible.
    // Avoid any unnecessary operations that can increase latency.

    return Result::error_not_supported;
}

Result Connection::set_link(context::Context& ctx, Connection *new_link,
                            Connection *requester)
{
    (void)requester;

    // WARNING: Changing a link will affect the hot path of Data Plane.
    // Avoid any unnecessary operations that can increase latency.

    if (_link == new_link)
        return set_result(Result::success);

    // TODO: generate an Event (conn_changing_link).
    // Use context to cancel sending the Event.

    // setting_link.store(true, std::memory_order_release);
    // transmitting.wait(true, std::memory_order_acquire);
    {
        const std::lock_guard<std::mutex> lk(link_mx);

        _link = new_link;
    }
    // setting_link.store(false, std::memory_order_release);
    // setting_link.notify_one();

    // TODO: generate a post Event (conn_link_changed).
    // Use context to cancel sending the Event.

    return set_result(Result::success); // TODO: return error if needed
}

Connection * Connection::link()
{
    return _link;
}

Result Connection::set_result(Result res)
{
    if (res != Result::success)
        metrics.errors++;

    return res;
}

void Connection::collect(telemetry::Metric& metric, const int64_t& timestamp_ms)
{
    uint64_t in = metrics.inbound_bytes;
    uint64_t out = metrics.outbound_bytes;
    uint32_t strn = metrics.transactions_succeeded;

    metric.addFieldString("state", state2str(state()));
    metric.addFieldBool("link", link() != nullptr);
    metric.addFieldUint64("in", in);
    metric.addFieldUint64("out", out);
    metric.addFieldUint64("strn", strn);
    metric.addFieldUint64("ftrn", metrics.transactions_failed);
    metric.addFieldUint64("err", metrics.errors);

    auto dt = timestamp_ms - metrics.prev_timestamp_ms;

    if (metrics.prev_inbound_bytes) {
        uint64_t bw = (in - metrics.prev_inbound_bytes);
        bw = (bw * 8 * 1000) / dt;
        metric.addFieldDouble("inbw", (bw/1000)/1000.0);
    }
    metrics.prev_inbound_bytes = in;

    if (metrics.prev_outbound_bytes) {
        uint64_t bw = (out - metrics.prev_outbound_bytes);
        bw = (bw * 8 * 1000) / dt;
        metric.addFieldDouble("outbw", (bw/1000)/1000.0);
    }
    metrics.prev_outbound_bytes = out;

    if (metrics.prev_transactions_succeeded) {
        uint64_t tps = (strn - metrics.prev_transactions_succeeded);
        tps = (tps * 10 * 1000) / dt;
        metric.addFieldDouble("tps", tps/10.0);
    }
    metrics.prev_transactions_succeeded = strn;

    metrics.prev_timestamp_ms = timestamp_ms;

    auto errors_delta = metrics.errors - metrics.prev_errors;
    metrics.prev_errors = metrics.errors;
    metric.addFieldUint64("errd", errors_delta);
}

static const char str_unknown[] = "?unknown?";

const char * kind2str(Kind kind, bool brief)
{
    switch (kind) {
    case Kind::undefined:   return brief ? "Undef" : "undefined";
    case Kind::transmitter: return brief ? "Tx" : "transmitter";
    case Kind::receiver:    return brief ? "Rx" : "receiver";
    default:                return str_unknown;
    }
}

const char * state2str(State state)
{
    switch (state) {
    case State::not_configured: return "not configured";
    case State::configured:     return "configured";
    case State::establishing:   return "establishing";
    case State::active:         return "active";
    case State::suspended:      return "suspended";
    case State::closing:        return "closing";
    case State::closed:         return "closed";
    case State::deleting:       return "deleting";
    default:                    return str_unknown;
    }
}

const char * status2str(Status status)
{
    switch (status) {
    case Status::initial:    return "initial";
    case Status::transition: return "transition";
    case Status::healthy:    return "healthy";
    case Status::failure:    return "failure";
    case Status::shutdown:   return "shutdown";
    default:                 return str_unknown;
    }
}

const char * result2str(Result res)
{
    switch (res) {
    case Result::success:                          return "success";
    case Result::error_not_supported:              return "not supported";
    case Result::error_wrong_state:                return "wrong state";
    case Result::error_no_link_assigned:           return "no link assigned";
    case Result::error_bad_argument:               return "bad argument";
    case Result::error_out_of_memory:              return "out of memory";
    case Result::error_general_failure:            return "general failure";
    case Result::error_context_cancelled:          return "context cancelled";
    case Result::error_conn_config_invalid:        return "invalid conn config";
    case Result::error_payload_config_invalid:     return "invalid payload config";

    case Result::error_already_initialized:        return "already initialized";
    case Result::error_initialization_failed:      return "initialization failed";
    case Result::error_memory_registration_failed: return "memory registration failed";
    case Result::error_thread_creation_failed:     return "thread creation failed";
    case Result::error_no_buffer:                  return "no buffer";
    case Result::error_timeout:                    return "timeout";

    default:                                       return str_unknown;
    }
}

const char * Config::kind2str() const
{
    switch (kind) {
    case sdk::CONN_KIND_TRANSMITTER: return "tx";
    case sdk::CONN_KIND_RECEIVER:    return "rx";
    default:                         return str_unknown;
    }
}

const char * Config::conn_type2str() const
{
    switch (conn_type) {
    case CONN_TYPE_GROUP:  return "multipoint-group";
    case CONN_TYPE_ST2110: return "st2110";
    case CONN_TYPE_RDMA:   return "rdma";
    default:               return str_unknown;
    }
}

const char * Config::st2110_transport2str() const
{
    switch (conn.st2110.transport) {
    case sdk::CONN_TRANSPORT_ST2110_20: return "st2110-20";
    case sdk::CONN_TRANSPORT_ST2110_22: return "st2110-22";
    case sdk::CONN_TRANSPORT_ST2110_30: return "st2110-30";
    default:                            return str_unknown;
    }
}

const char * Config::payload_type2str() const
{
    switch (payload_type) {
    case PAYLOAD_TYPE_BLOB:  return "blob";
    case PAYLOAD_TYPE_VIDEO: return "video";
    case PAYLOAD_TYPE_AUDIO: return "audio";
    default:                 return str_unknown;
    }
}

const char * Config::video_pixel_format2str() const
{
    switch (payload.video.pixel_format) {
    case sdk::VIDEO_PIXEL_FORMAT_YUV422PLANAR10LE:  return "yuv422p10le";
    case sdk::VIDEO_PIXEL_FORMAT_V210:              return "v210";
    case sdk::VIDEO_PIXEL_FORMAT_YUV422RFC4175BE10: return "yuv422p10rfc4175";
    default:                                        return str_unknown;
    }
}

const char * Config::audio_sample_rate2str() const
{
    switch (payload.audio.sample_rate) {
    case sdk::AUDIO_SAMPLE_RATE_48000: return "48K";
    case sdk::AUDIO_SAMPLE_RATE_96000: return "96K";
    case sdk::AUDIO_SAMPLE_RATE_44100: return "44.1K";
    default:                           return str_unknown;
    }
}

const char * Config::audio_format2str() const
{
    switch (payload.audio.format) {
    case sdk::AUDIO_FORMAT_PCM_S8:    return "pcm_s8";
    case sdk::AUDIO_FORMAT_PCM_S16BE: return "pcm_s16be";
    case sdk::AUDIO_FORMAT_PCM_S24BE: return "pcm_s24be";
    default:                          return str_unknown;
    }
}

const char * Config::audio_packet_time2str() const
{
    switch (payload.audio.packet_time) {
    case sdk::AUDIO_PACKET_TIME_1MS:    return "1ms";
    case sdk::AUDIO_PACKET_TIME_125US:  return "125us";
    case sdk::AUDIO_PACKET_TIME_250US:  return "250us";
    case sdk::AUDIO_PACKET_TIME_333US:  return "333us";
    case sdk::AUDIO_PACKET_TIME_4MS:    return "4ms";
    case sdk::AUDIO_PACKET_TIME_80US:   return "80us";
    case sdk::AUDIO_PACKET_TIME_1_09MS: return "1.09ms";
    case sdk::AUDIO_PACKET_TIME_0_14MS: return "0.14ms";
    case sdk::AUDIO_PACKET_TIME_0_09MS: return "0.09ms";
    default:                            return str_unknown;
    }
}

Result Config::assign_from_pb(const sdk::ConnectionConfig& config)
{
    kind = config.kind();
    if (kind != sdk::ConnectionKind::CONN_KIND_TRANSMITTER &&
        kind != sdk::ConnectionKind::CONN_KIND_RECEIVER)
        return Result::error_conn_config_invalid;

    buf_queue_capacity      = config.buf_queue_capacity();
    max_payload_size        = config.max_payload_size();
    max_metadata_size       = config.max_metadata_size();
    calculated_payload_size = config.calculated_payload_size();

    if (config.has_multipoint_group()) {
        conn_type = ConnectionType::CONN_TYPE_GROUP;
        const sdk::ConfigMultipointGroup& group = config.multipoint_group();
        conn.multipoint_group.urn = group.urn();
    } else if (config.has_st2110()) {
        conn_type = ConnectionType::CONN_TYPE_ST2110;
        const sdk::ConfigST2110& st2110 = config.st2110();
        conn.st2110.remote_ip_addr = st2110.remote_ip_addr();
        conn.st2110.remote_port    = st2110.remote_port();
        conn.st2110.transport      = st2110.transport();
        conn.st2110.pacing         = st2110.pacing();
        conn.st2110.payload_type   = st2110.payload_type();
    } else if (config.has_rdma()) {
        conn_type = ConnectionType::CONN_TYPE_RDMA;
        const sdk::ConfigRDMA& rdma = config.rdma();
        conn.rdma.connection_mode = rdma.connection_mode();
        conn.rdma.max_latency_ns  = rdma.max_latency_ns();
    } else {
        return Result::error_conn_config_invalid;
    }

    if (config.has_video()) {
        payload_type = PayloadType::PAYLOAD_TYPE_VIDEO;
        const sdk::ConfigVideo& video = config.video();
        payload.video.width        = video.width();
        payload.video.height       = video.height();
        payload.video.fps          = video.fps();
        payload.video.pixel_format = video.pixel_format();
    } else if (config.has_audio()) {
        payload_type = PayloadType::PAYLOAD_TYPE_AUDIO;
        const sdk::ConfigAudio& audio = config.audio();
        payload.audio.channels    = audio.channels();
        payload.audio.sample_rate = audio.sample_rate();
        payload.audio.format      = audio.format();
        payload.audio.packet_time = audio.packet_time();
    } else {
        return Result::error_payload_config_invalid;
    }

    return Result::success;
}

void Config::assign_to_pb(sdk::ConnectionConfig& config) const
{
    config.set_kind((sdk::ConnectionKind)kind);

    config.set_buf_queue_capacity(buf_queue_capacity);
    config.set_max_payload_size(max_payload_size);
    config.set_max_metadata_size(max_metadata_size);
    config.set_calculated_payload_size(calculated_payload_size);

    if (conn_type == ConnectionType::CONN_TYPE_GROUP) {
        auto group = new sdk::ConfigMultipointGroup();
        group->set_urn(conn.multipoint_group.urn);
        config.set_allocated_multipoint_group(group);
    } else if (conn_type == ConnectionType::CONN_TYPE_ST2110) {
        auto st2110 = new sdk::ConfigST2110();
        st2110->set_remote_ip_addr(conn.st2110.remote_ip_addr);
        st2110->set_remote_port(conn.st2110.remote_port);
        st2110->set_transport(conn.st2110.transport);
        st2110->set_pacing(conn.st2110.pacing);
        st2110->set_payload_type(conn.st2110.payload_type);
        config.set_allocated_st2110(st2110);
    } else if (conn_type == ConnectionType::CONN_TYPE_RDMA) {
        auto rdma = new sdk::ConfigRDMA();
        rdma->set_connection_mode(conn.rdma.connection_mode);
        rdma->set_max_latency_ns(conn.rdma.max_latency_ns);
        config.set_allocated_rdma(rdma);
    }

    if (payload_type == PayloadType::PAYLOAD_TYPE_VIDEO) {
        auto video = new sdk::ConfigVideo();
        video->set_width(payload.video.width);
        video->set_height(payload.video.height);
        video->set_fps(payload.video.fps);
        video->set_pixel_format(payload.video.pixel_format);
        config.set_allocated_video(video);
    } else if (payload_type == PayloadType::PAYLOAD_TYPE_AUDIO) {
        auto audio = new sdk::ConfigAudio();
        audio->set_channels(payload.audio.channels);
        audio->set_sample_rate(payload.audio.sample_rate);
        audio->set_format(payload.audio.format);
        audio->set_packet_time(payload.audio.packet_time);
        config.set_allocated_audio(audio);
    }
}

} // namespace mesh::connection
