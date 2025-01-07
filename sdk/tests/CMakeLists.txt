# Find source files for tests
file(GLOB TEST_SOURCES "*.cc")

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -lz -fPIC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -lz -fPIC")

# Add an executable for tests
add_executable(sdk_unit_tests ${TEST_SOURCES})

# Link the executable with gtest and gtest_main
target_link_libraries(sdk_unit_tests PRIVATE mcm_dp gtest gtest_main)
target_include_directories(sdk_unit_tests PUBLIC
    ${CMAKE_SOURCE_DIR}/sdk/include
    ${CMAKE_SOURCE_DIR}/sdk/include/mesh
)

# Add tests to CTest
add_test(NAME sdk_unit_tests COMMAND sdk_unit_tests)
