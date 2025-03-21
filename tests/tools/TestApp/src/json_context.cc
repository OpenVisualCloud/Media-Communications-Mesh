#include "json_context.h"
#include "mesh_conn.h"

namespace mesh {
  extern "C" {
    /*TODO: temporary solution, replace with custom json parsing.
      those are internal structures that can be changed any time
      so to avoid breaking changes, we need to provide a way to get
      those values from the josn directly  
    */
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
      return (MeshConfig_Audio){
        .channels = conn_ctx->cfg_json.payload.audio.channels,
        .sample_rate = conn_ctx->cfg_json.payload.audio.sample_rate,
        .format  = conn_ctx->cfg_json.payload.audio.format,
        .packet_time = conn_ctx->cfg_json.payload.audio.packet_time
      };
    }
  }
}
