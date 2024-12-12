#include <gtest/gtest.h>
#include "mesh_json_proxy.h"
#include "mesh_dp.h"
using namespace mesh;

TEST(mesh_json_proxy, parse) {
    std::string json_str = R"({
        "bufferQueueCapacity": 16,
        "maxPayloadSize": 2097152,
        "maxMetadataSize": 8192,
        "connection": {
            "multipoint-group": [
                {
                    "urn": "ipv4:224.0.0.1:9003"
                },
                {
                    "urn": "ipv4:224.0.0.1:9006"
                }
            ],
            "st2110": {
                "transport": "st2110-20",
                "remoteIpAddr": "192.168.95.2",
                "remotePort": 9002,
                "pacing": "narrow",
                "payloadType": 112
            },
            "rdma": [
                {
                    "connectionMode": "ARC",
                    "maxLatencyNs": 10000
                },
                {
                    "connectionMode": "RC",
                    "maxLatencyNs": 20000
                },
                {
                    "connectionMode": "UD",
                    "maxLatencyNs": 30000
                }
            ]
        },
        "payload": {
            "video": {
                "width": 1920,
                "height": 1080,
                "fps": 60.0,
                "pixelFormat": "yuv422p10le"
            },
            "audio": [
                {
                    "channels": 2,
                    "sampleRate": 48000,
                    "format": "pcm_s24be",
                    "packetTime": "1ms"
                },
                {
                    "channels": 2,
                    "sampleRate": 96000,
                    "format": "pcm_s24be",
                    "packetTime": "2ms"
                }
            ],
            "ancillary": {},
            "blob": {}
        }
    })";

    ConnectionConfiguration connection_config;

    try {
        json j = json::parse(json_str);
        from_json(j, connection_config);
    } catch (const json::exception& e) {
        std::cout << e.what() << '\n';
        ASSERT_EQ(1, 0);
    } catch (const std::exception& e) {
        std::cout << e.what() << '\n';
        ASSERT_EQ(1, 0);
    }
}
