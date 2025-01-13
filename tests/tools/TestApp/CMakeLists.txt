cmake_minimum_required(VERSION 3.10)

# Set the project name and version
project(TestApps VERSION 1.0)



# Check if myENV is defined
if(DEFINED DEMO)
message(STATUS "************* configuring demo app *************")
  set(INC "Inc")
  set(SRC src/input.c src/mcm.c src/mcm_mock.c)

else()
  message(STATUS "************* configuring full app *************")
  
  execute_process(
    COMMAND git rev-parse --show-toplevel
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_REPO_ROOT
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  message(STATUS "************* root project dir: ${GIT_REPO_ROOT} *************")

  set(INC ${GIT_REPO_ROOT}/sdk/include
          ${GIT_REPO_ROOT}/sdk/3rdparty/nlohmann
          ${GIT_REPO_ROOT}/sdk/3rdparty/libmemif/src)
  file(GLOB SRC "${GIT_REPO_ROOT}/sdk/src/*.c")

endif()

include_directories(${INC})

# Add the executable
add_executable(TxApp tx_app.c ${SRC})
add_executable(RxApp rx_app.c ${SRC})
