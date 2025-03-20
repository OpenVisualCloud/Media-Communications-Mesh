#include "json_context.h"
#include "mesh_conn.h"

namespace mesh {
  extern "C" {

    int get_payload_type(MeshConnection *conn){
      ConnectionContext *conn_ctx = (ConnectionContext *)conn;
      return conn_ctx->cfg_json.payload_type;
    }

    MeshConfig_Video get_video_params(MeshConnection *conn){
      ConnectionContext *conn_ctx = (ConnectionContext *)conn;
      return (MeshConfig_Video){
        .width = (int)conn_ctx->cfg_json.payload.video.width,
        .height = (int)conn_ctx->cfg_json.payload.video.height,
        .fps = conn_ctx->cfg_json.payload.video.fps,
        .pixel_format = conn_ctx->cfg_json.payload.video.pixel_format
      };
    }
    MeshConfig_Audio get_audio_params(MeshConnection *conn){
      ConnectionContext *conn_ctx = (ConnectionContext *)conn;
          /* 
        packet_time, format, sample_rate match tables,
        order as in Media-Communications-Mesh/sdk/include/mesh_dp.hL231
    */
    int packet_time_convert_table_us[] = {1000, 125, 250, 333, 4000, 80, 1009, 140, 90};
    char* format_convert_table_str[] = {"pcms8", "pcms16be", "pcms24be"};
    int sample_rate_convert_table_hz[] = {48000, 96000, 44100};
      return (MeshConfig_Audio){
        .channels = conn_ctx->cfg_json.payload.audio.channels,
        .sample_rate = conn_ctx->cfg_json.payload.audio.sample_rate,
        .format  = conn_ctx->cfg_json.payload.audio.format,
        .packet_time = packet_time_convert_table_us[conn_ctx->cfg_json.payload.audio.packet_time]
      };
    }
  }
}
