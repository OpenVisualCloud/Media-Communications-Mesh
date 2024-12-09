#include <stdio.h>
#include "Inc/input.h"
#include "Inc/mcm.h"
#include <unistd.h>

const char* client_cfg;
const char* conn_cfg;

int main(int argc, char* argv[]){
  printf("launching RX App \n");
  printf("reading client configuration... \n");  
  client_cfg = parse_json_to_string("client.json");
  printf("reading connection configuration... \n");
  conn_cfg = parse_json_to_string("connection.json");
  mcm_ts mcm;
  mcm_init_client(&mcm, client_cfg);

  printf("waiting for frames... \n");
  while(1){
    mcm_create_rx_connection(&mcm, conn_cfg);
    mcm_receive_video_frames(&mcm);
  }
  return 0;
}