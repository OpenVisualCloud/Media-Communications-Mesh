#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "Inc/input.h"
#include "Inc/mcm.h"




const char* client_cfg;
const char* conn_cfg;

int main(int argc, char** argv){
  if (argc != 2) {
      fprintf(stderr, "Usage: %s <file>\n", argv[0]);
      exit(EXIT_FAILURE);
  }

  char* file_name = argv[1];
  printf("launching TX app \n");
  client_cfg = parse_json_to_string("client.json");
  conn_cfg = parse_json_to_string("connection.json");
  mcm_ts mcm;
  mcm_init_client(&mcm, client_cfg);
  mcm_create_tx_connection(&mcm, conn_cfg);
  mcm_send_video_frame(&mcm, file_name, DUMMY_LEN);
  return 0;
}