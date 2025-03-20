cmake_minimum_required(VERSION 3.10)

# Set the project name and version
project(TestApps VERSION 1.0)

# Find the threading library
find_package(Threads REQUIRED)

# Find the BSD library
find_library(BSD_LIB bsd REQUIRED)

execute_process(
  COMMAND git rev-parse --show-toplevel
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_REPO_ROOT
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "************* root project dir: ${GIT_REPO_ROOT} *************")

set(INC ${GIT_REPO_ROOT}/sdk/include Inc)
set(SRC src/input.c src/mcm.c src/misc.c src/json_context.cc)

include_directories(${INC})

# Add the executable
add_executable(TxVideoApp tx_video_app.c ${SRC})
add_executable(RxVideoApp rx_video_app.c ${SRC})
add_executable(TxAudioApp tx_audio_app.c ${SRC})
add_executable(RxAudioApp rx_audio_app.c ${SRC})


target_link_libraries(TxVideoApp PRIVATE memif ${CMAKE_THREAD_LIBS_INIT} ${BSD_LIB} mcm_dp)
target_link_libraries(RxVideoApp PRIVATE memif ${CMAKE_THREAD_LIBS_INIT} ${BSD_LIB} mcm_dp)
target_link_libraries(TxAudioApp PRIVATE memif ${CMAKE_THREAD_LIBS_INIT} ${BSD_LIB} mcm_dp)
target_link_libraries(RxAudioApp PRIVATE memif ${CMAKE_THREAD_LIBS_INIT} ${BSD_LIB} mcm_dp)
