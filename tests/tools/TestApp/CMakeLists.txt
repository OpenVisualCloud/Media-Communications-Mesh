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
set(SRC src/input.c src/mcm.c src/misc.c)

include_directories(${INC})

# Add the executable
add_executable(TxVideoApp tx_video_app.c ${SRC})
add_executable(RxVideoApp rx_video_app.c ${SRC})
add_executable(TxAudioApp tx_audio_app.c ${SRC})
add_executable(RxAudioApp rx_audio_app.c ${SRC})
add_executable(TxBlobApp tx_blob_app.c ${SRC})
add_executable(RxBlobApp rx_blob_app.c ${SRC})
add_executable(TxST2110App tx_st2110_app.c ${SRC})
add_executable(RxST2110App rx_st2110_app.c ${SRC})
add_executable(TxRDMATestApp tx_rdma_test_app.c ${SRC})
add_executable(RxRDMATestApp rx_rdma_test_app.c ${SRC})
add_executable(TxMultipointApp tx_multipoint_app.c ${SRC})
add_executable(RxMultipointApp rx_multipoint_app.c ${SRC})
add_executable(CallbackTestApp callback_test_app.c ${SRC})
add_executable(ErrorInjectionApp error_injection_app.c ${SRC})
add_executable(MemoryTestApp memory_test_app.c ${SRC})
add_executable(PerformanceProfileApp performance_profile_app.c ${SRC})
add_executable(ProtocolTestApp protocol_test_app.c ${SRC})
add_executable(FormatValidationApp format_validation_app.c ${SRC})
add_executable(CodecTestApp codec_test_app.c ${SRC})
add_executable(ContainerValidationApp container_validation_app.c ${SRC})

# link the libraries
target_link_libraries(TxVideoApp PRIVATE memif ${CMAKE_THREAD_LIBS_INIT} ${BSD_LIB} mcm_dp jansson)
target_link_libraries(RxVideoApp PRIVATE memif ${CMAKE_THREAD_LIBS_INIT} ${BSD_LIB} mcm_dp jansson)
target_link_libraries(TxAudioApp PRIVATE memif ${CMAKE_THREAD_LIBS_INIT} ${BSD_LIB} mcm_dp jansson)
target_link_libraries(RxAudioApp PRIVATE memif ${CMAKE_THREAD_LIBS_INIT} ${BSD_LIB} mcm_dp jansson)
target_link_libraries(TxBlobApp PRIVATE memif ${CMAKE_THREAD_LIBS_INIT} ${BSD_LIB} mcm_dp jansson)
target_link_libraries(RxBlobApp PRIVATE memif ${CMAKE_THREAD_LIBS_INIT} ${BSD_LIB} mcm_dp jansson)
target_link_libraries(TxST2110App PRIVATE memif ${CMAKE_THREAD_LIBS_INIT} ${BSD_LIB} mcm_dp jansson)
target_link_libraries(RxST2110App PRIVATE memif ${CMAKE_THREAD_LIBS_INIT} ${BSD_LIB} mcm_dp jansson)
target_link_libraries(TxRDMATestApp PRIVATE memif ${CMAKE_THREAD_LIBS_INIT} ${BSD_LIB} mcm_dp jansson)
target_link_libraries(RxRDMATestApp PRIVATE memif ${CMAKE_THREAD_LIBS_INIT} ${BSD_LIB} mcm_dp jansson)
target_link_libraries(TxMultipointApp PRIVATE memif ${CMAKE_THREAD_LIBS_INIT} ${BSD_LIB} mcm_dp jansson)
target_link_libraries(RxMultipointApp PRIVATE memif ${CMAKE_THREAD_LIBS_INIT} ${BSD_LIB} mcm_dp jansson m)
target_link_libraries(CallbackTestApp PRIVATE memif ${CMAKE_THREAD_LIBS_INIT} ${BSD_LIB} mcm_dp jansson)
target_link_libraries(ErrorInjectionApp PRIVATE memif ${CMAKE_THREAD_LIBS_INIT} ${BSD_LIB} mcm_dp jansson)
target_link_libraries(MemoryTestApp PRIVATE memif ${CMAKE_THREAD_LIBS_INIT} ${BSD_LIB} mcm_dp jansson)
target_link_libraries(PerformanceProfileApp PRIVATE memif ${CMAKE_THREAD_LIBS_INIT} ${BSD_LIB} mcm_dp jansson)
target_link_libraries(ProtocolTestApp PRIVATE memif ${CMAKE_THREAD_LIBS_INIT} ${BSD_LIB} mcm_dp jansson m)
target_link_libraries(FormatValidationApp PRIVATE memif ${CMAKE_THREAD_LIBS_INIT} ${BSD_LIB} mcm_dp jansson)
target_link_libraries(CodecTestApp PRIVATE memif ${CMAKE_THREAD_LIBS_INIT} ${BSD_LIB} mcm_dp jansson m)
target_link_libraries(ContainerValidationApp PRIVATE memif ${CMAKE_THREAD_LIBS_INIT} ${BSD_LIB} mcm_dp jansson)
