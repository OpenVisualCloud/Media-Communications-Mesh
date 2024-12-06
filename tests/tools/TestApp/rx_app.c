#include <stdio.h>
#include "Inc/input.h"
#include "Inc/mcm.h"
#include <unistd.h>

const char* client_cfg;
const char* conn_cfg;

int main(int argc, char* argv[]){
  printf("launching RX App \n");
  client_cfg = parse_json_to_string("client.json");
  conn_cfg = parse_json_to_string("connection.json");
  mcm_ts mcm;
  mcm_init_client(&mcm, client_cfg);

  while(1){
    mcm_create_rx_connection(&mcm, conn_cfg);
    mcm_receive_video_frames(&mcm);
  }
  return 0;
}