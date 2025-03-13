#include "json_context.h"
#include "mesh_conn.h"

namespace mesh {
  extern "C" {
    MeshConfig_Video get_video_params(MeshConnection *conn){
      ConnectionContext *conn_ctx = (ConnectionContext *)conn;
      return (MeshConfig_Video){

        .width = conn_ctx->cfg.payload.video.width,
        .height = conn_ctx->cfg.payload.video.height,
        .fps = conn_ctx->cfg.payload.video.fps,
        .pixel_format = conn_ctx->cfg.payload.video.pixel_format
      };
    }
  }
}
