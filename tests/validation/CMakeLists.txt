cmake_minimum_required(VERSION 3.10)

# Set the project name and version
project(MyCApp VERSION 1.0)
# Include directories
include_directories(Inc
                    ../../sdk/include
                    ../../sdk/3rdparty/libmemif/src)

set(SRC src/input.c src/mcm.c)
# Add the executable
add_executable(TxApp tx_app.c ${SRC})
add_executable(RxApp rx_app.c ${SRC})