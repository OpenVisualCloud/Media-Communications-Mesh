#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "Inc/input.h"
#include "Inc/mcm.h"




const char* client_cfg;
const char* conn_cfg;

int main(int argc, char** argv){
  if (argc != 4) {
      fprintf(stderr, "Usage: %s <client_cfg.json> <connection_cfg.json> <abs path>/file|frame>\n", argv[0]);
      exit(EXIT_FAILURE);
  }

  char* client_cfg_file = argv[1];
  char* conn_cfg_file = argv[2];
  char* frame_file = argv[3];
  FILE *frame = fopen(frame_file, "rb");
  printf("launching TX app \n");
  printf("reading client configuration... \n");  
  client_cfg = parse_json_to_string(client_cfg_file);
  printf("reading connection configuration... \n");
  conn_cfg = parse_json_to_string(conn_cfg_file);
  mcm_ts mcm;
  mcm_init_client(&mcm, client_cfg);
  mcm_create_tx_connection(&mcm, conn_cfg);
  mcm_send_video_frame(&mcm, frame);
  return 0;
}