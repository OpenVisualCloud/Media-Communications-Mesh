/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ST2110_H
#define ST2110_H

#include <thread>
#include <bsd/string.h>
#include <arpa/inet.h>
#include <mtl/st_pipeline_api.h>
#include <mtl/st30_pipeline_api.h>

#include "conn.h"
#include "mesh_dp.h"
#include "logger.h"

namespace mesh::connection {

#define ST_APP_PAYLOAD_TYPE_ST30 (111)
#define ST_APP_PAYLOAD_TYPE_ST20 (112)
#define ST_APP_PAYLOAD_TYPE_ST22 (114)

int mesh_video_format_to_st_format(int fmt, st_frame_fmt& st_fmt);
int mesh_transport_video_format_to_st20_fmt(int transport_format, st20_fmt& st20_format);
int mesh_audio_format_to_st_format(int fmt, st30_fmt& st_fmt);
int mesh_audio_sampling_to_st_sampling(int sampling, st30_sampling& st_sampling);
int mesh_audio_ptime_to_st_ptime(int ptime, st30_ptime& st_ptime);

void *get_frame_data_ptr(st_frame *src);
void *get_frame_data_ptr(st30_frame *src);

void get_mtl_dev_params(mtl_init_params& st_param, const std::string& dev_port,
                        mtl_log_level log_level, const std::string& ip_addr);
mtl_handle get_mtl_device(const std::string& dev_port, mtl_log_level log_level,
                          const std::string& ip_addr);
int mtl_get_session_id();

/**
 * ST2110
 *
 * Base abstract class of SPMTE ST2110-xx bridge. ST2110Rx/ST2110Tx
 * inherit this class.
 */
template <typename FRAME, typename HANDLE, typename OPS> class ST2110 : public Connection {
  public:
    virtual ~ST2110() {
        shutdown(_ctx);
        if (ops.name)
            free((void *)ops.name);
    };

  protected:
    mtl_handle mtl_device = nullptr;
    HANDLE mtl_session = nullptr;
    OPS ops = {0};
    std::string ip_addr;
    size_t transfer_size = 0;
    std::atomic<bool> frame_available;
    context::Context _ctx = context::WithCancel(context::Background());

    virtual FRAME *get_frame(HANDLE) = 0;
    virtual int put_frame(HANDLE, FRAME *) = 0;
    virtual HANDLE create_session(mtl_handle, OPS *) = 0;
    virtual int close_session(HANDLE) = 0;

    //Wrapper for get_mtl_device, override only for UnitTest purpose
    virtual mtl_handle get_mtl_dev_wrapper(const std::string& dev_port, mtl_log_level log_level,
                                           const std::string& ip_addr) {
        return get_mtl_device(dev_port, log_level, ip_addr);
    };

    static int frame_available_cb(void *ptr) {
        auto _this = static_cast<ST2110 *>(ptr);
        if (!_this) {
            return -1;
        }

        _this->notify_frame_available();

        return 0;
    }

    void notify_frame_available() {
        frame_available.store(true, std::memory_order_release);
        frame_available.notify_one();
    }

    void wait_frame_available() {
        frame_available.wait(false, std::memory_order_acquire);
        frame_available = false;
    }

    virtual int configure_common(context::Context& ctx, const std::string& dev_port,
                                 const MeshConfig_ST2110& cfg_st2110) {
        ip_addr = std::string(cfg_st2110.local_ip_addr);
        strlcpy(ops.port.port[MTL_PORT_P], dev_port.c_str(), MTL_PORT_MAX_LEN);
        ops.port.num_port = 1;

        char session_name[NAME_MAX] = "";
        snprintf(session_name, NAME_MAX, "mcm_mtl_%d", mtl_get_session_id());
        if (ops.name)
            free((void *)ops.name);
        ops.name = strdup(session_name);
        ops.framebuff_cnt = 4;

        ops.priv = this; // app handle register to lib
        ops.notify_frame_available = frame_available_cb;

        log::info("ST2110: configure")
            ("port", ops.port.port[MTL_PORT_P])
            ("num_port", (int)ops.port.num_port)
            ("name", ops.name)
            ("framebuff_cnt", ops.framebuff_cnt);

        return 0;
    }

    Result on_establish(context::Context& ctx) override {
        mtl_device = get_mtl_dev_wrapper(ops.port.port[MTL_PORT_P], MTL_LOG_LEVEL_CRIT, ip_addr);
        if (!mtl_device) {
            log::error("Failed to get MTL device");
            set_state(ctx, State::closed);
            return set_result(Result::error_general_failure);
        }

        _ctx = context::WithCancel(ctx);
        frame_available = false;

        mtl_session = create_session(mtl_device, &ops);
        if (!mtl_session) {
            log::error("Failed to create session");
            set_state(ctx, State::closed);
            return set_result(Result::error_general_failure);
        }

        set_state(ctx, State::active);
        return set_result(Result::success);
    }

    Result on_shutdown(context::Context& ctx) override {
        _ctx.cancel();
        notify_frame_available();

        if (mtl_session) {
            close_session(mtl_session);
            mtl_session = nullptr;
        }
        set_state(ctx, State::closed);
        return set_result(Result::success);
    };
};

} // namespace mesh::connection

#endif // ST2110_H
