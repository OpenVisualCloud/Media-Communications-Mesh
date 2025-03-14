#include "json_context.h"
#include "mesh_conn.h"

namespace mesh {
  extern "C" {
    MeshConfig_Video get_video_params(MeshConnection *conn){
      ConnectionContext *conn_ctx = (ConnectionContext *)conn;
      return (MeshConfig_Video){
        .width = (int)conn_ctx->cfg_json.payload.video.width,
        .height = (int)conn_ctx->cfg_json.payload.video.height,
        .fps = conn_ctx->cfg_json.payload.video.fps,
        .pixel_format = conn_ctx->cfg_json.payload.video.pixel_format
      };
    }
  }
}
