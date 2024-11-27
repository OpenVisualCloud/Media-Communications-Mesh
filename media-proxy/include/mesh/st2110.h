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

/**
 * ST2110
 *
 * Base abstract class of SPMTE ST2110-xx bridge. ST2110Rx/ST2110Tx
 * inherit this class.
 */
class ST2110 : public Connection {
  public:
    static int mesh_video_format_to_st_format(int fmt, st_frame_fmt& st_fmt);
    static int mesh_audio_format_to_st_format(int fmt, st30_fmt& st_fmt);
    static int mesh_audio_sampling_to_st_sampling(int sampling, st30_sampling& st_sampling);
    static int mesh_audio_ptime_to_st_ptime(int ptime, st30_ptime& st_ptime);

    static void *get_frame_data_ptr(st_frame *src);
    static void *get_frame_data_ptr(st30_frame *src);

    static void get_mtl_dev_params(mtl_init_params& st_param, const std::string& dev_port,
                                   mtl_log_level log_level,
                                   const char local_ip_addr[MESH_IP_ADDRESS_SIZE]);
    static mtl_handle get_mtl_device(const std::string& dev_port, mtl_log_level log_level,
                                     const char local_ip_addr[MESH_IP_ADDRESS_SIZE],
                                     int& session_id);

    virtual ~ST2110() {};

  protected:
    static int frame_available_cb(void *ptr);

    mtl_handle mtl_device = nullptr;
    std::atomic<bool> stop;
};

} // namespace mesh::connection

#endif // ST2110_H
