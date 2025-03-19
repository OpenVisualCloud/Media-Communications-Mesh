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
      return (MeshConfig_Audio){
        .channels = conn_ctx->cfg_json.payload.audio.channels,
        .sample_rate = conn_ctx->cfg_json.payload.audio.sample_rate,
        .format  = conn_ctx->cfg_json.payload.audio.format,
        .packet_time = conn_ctx->cfg_json.payload.audio.packet_time
      };
    }
  }
}
