cmake_minimum_required(VERSION 3.10)

# Set the project name and version
project(MyCApp VERSION 1.0)
# Include directories
include_directories(Inc
                    ../../sdk/include
                    ../../sdk/3rdparty/libmemif/src)

# Add the executable
add_executable(TestApp app.c src/app_config.c)