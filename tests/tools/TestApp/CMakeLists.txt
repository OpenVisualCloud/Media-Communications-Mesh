cmake_minimum_required(VERSION 3.10)

# Set the project name and version
project(TestApps VERSION 1.0)
# Include directories
include_directories(Inc)

set(SRC src/input.c src/mcm.c src/mcm_mock.c)



# Add the executable
add_executable(TxApp tx_app.c ${SRC})
add_executable(RxApp rx_app.c ${SRC})
