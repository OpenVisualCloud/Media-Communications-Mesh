#include "app.h"

int main(int argc, char** argv){
  app_config_ts config = parse_cli_input(argc, argv);
  return 0;
}