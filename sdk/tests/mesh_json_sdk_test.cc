#include <gtest/gtest.h>
#include "mesh_json_sdk.h"
#include "mesh_dp.h"
using namespace mesh;

TEST(mesh_json_sdk, parse) {
    std::string json_str = R"({
        "apiVersion": "v1",
        "apiConnectionString": "Server=192.168.96.1; Port=8001",
        "apiDefaultTimeoutMicroseconds": 100000,
        "maxMediaConnections": 32
      })";

    ClientConfig client_config;

    try {
        json j = json::parse(json_str);
        from_json(j, client_config);
    } catch (const json::exception& e) {
        std::cout << e.what() << '\n';
        ASSERT_EQ(1, 0);
    } catch (const std::exception& e) {
        std::cout << e.what() << '\n';
        ASSERT_EQ(1, 0);
    }
}
