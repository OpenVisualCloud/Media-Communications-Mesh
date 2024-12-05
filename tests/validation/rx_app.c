#include "Inc/input.h"
#include <stdio.h>

const char* client_cfg;
const char* conn_cfg;

int main(int argc, char* argv[]){
  client_cfg = parse_json_to_string("client.json");
  conn_cfg = parse_json_to_string("connection.json");
  if(is_mock_enabled(argc, argv)){
    printf("mock enabled\n");
  }else{
    printf("mock disabled\n");
  }

  return 0;
}