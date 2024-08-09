# Find source files for tests
file(GLOB TEST_SOURCES "*.cc")

set(MP_LIB media_proxy_lib)

# Add an executable for tests
add_executable(MCM_unit_tests ${TEST_SOURCES})

# Link the executable with gtest and gtest_main
target_link_libraries(MCM_unit_tests PRIVATE gtest gtest_main ${MP_LIB})
target_include_directories(MCM_unit_tests PUBLIC
    ${CMAKE_SOURCE_DIR}/media-proxy/include
    # Include generated *.pb.h files
    ${CMAKE_BINARY_DIR}/media-proxy/
)

# Add tests to CTest
add_test(NAME MCM_unit_tests COMMAND MCM_unit_tests)
