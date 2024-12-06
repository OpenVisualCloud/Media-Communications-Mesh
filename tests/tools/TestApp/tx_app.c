#include <stdio.h>
#include "Inc/input.h"
#include "Inc/mcm.h"
#include <unistd.h>
#include <string.h>


const char* client_cfg;
const char* conn_cfg;

int main(int argc, char** argv){
  printf("launching TX app \n");
  client_cfg = parse_json_to_string("client.json");
  conn_cfg = parse_json_to_string("connection.json");
  mcm_ts mcm;
  mcm_init_client(&mcm, client_cfg);
  mcm_create_tx_connection(&mcm, conn_cfg);
  mcm_send_video_frame(&mcm, "test_frame_1", strlen("test_frame_1"));
  return 0;
}