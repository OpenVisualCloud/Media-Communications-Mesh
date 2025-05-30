#include <gtest/gtest.h>
#include "mesh_client.h"
#include "mesh_conn.h"
#include "mesh_dp.h"
#include "mesh_dp_legacy.h"

using namespace mesh;

TEST(mesh_json_sdk, parse_client_cfg) {
    const char *str = R"({
        "apiVersion": "v1",
        "apiConnectionString": "Server=192.168.96.1; Port=8001",
        "apiDefaultTimeoutMicroseconds": 100000,
        "maxMediaConnections": 32
      })";

    ClientConfig config;
    int err = config.parse_from_json(str);

    ASSERT_EQ(err, 0);
    EXPECT_EQ(config.proxy_ip, "192.168.96.1");
    EXPECT_EQ(config.proxy_port, "8001");
}

TEST(mesh_json_sdk, parse_conn_cfg_multipoint_group) {
    const char *str = R"({
        "bufferQueueCapacity": 16,
        "maxPayloadSize": 2097152,
        "maxMetadataSize": 8192,
        "connection": {
          "multipointGroup": {
            "urn": "224.0.0.1:9501"
          }
        }
      })";

    ConnectionConfig config;
    int err = config.parse_from_json(str);

    ASSERT_EQ(err, 0);
    EXPECT_EQ(config.buf_queue_capacity, 16);
    EXPECT_EQ(config.max_payload_size, 2097152);
    EXPECT_EQ(config.max_metadata_size, 8192);
    EXPECT_EQ(config.conn_type, MESH_CONN_TYPE_GROUP);
    EXPECT_EQ(config.conn.multipoint_group.urn, "224.0.0.1:9501");
    EXPECT_EQ(config.payload_type, MESH_PAYLOAD_TYPE_BLOB);
}

TEST(mesh_json_sdk, parse_conn_cfg_st2110) {
    const char *str = R"({
        "connection": {
          "st2110": {
            "transport": "st2110-20",
            "ipAddr": "239.0.0.1",
            "port": 9002,
            "multicastSourceIpAddr": "192.168.95.2",
            "pacing": "narrow",
            "payloadType": 110,
            "transportPixelFormat": "yuv422p10rfc4175"
          }
        },
        "payload": {
          "video": {}
        }
      })";

    ConnectionConfig config;
    int err = config.parse_from_json(str);

    ASSERT_EQ(err, 0);
    EXPECT_EQ(config.conn_type, MESH_CONN_TYPE_ST2110);
    EXPECT_EQ(config.conn.st2110.transport, MESH_CONN_TRANSPORT_ST2110_20);
    EXPECT_EQ(config.conn.st2110.ip_addr, "239.0.0.1");
    EXPECT_EQ(config.conn.st2110.mcast_sip_addr, "192.168.95.2");
    EXPECT_EQ(config.conn.st2110.port, 9002);
    EXPECT_EQ(config.conn.st2110.pacing, "narrow");
    EXPECT_EQ(config.conn.st2110.payload_type, 110);
    EXPECT_EQ(config.conn.st2110.transportPixelFormat, "yuv422p10rfc4175");
    EXPECT_EQ(config.payload_type, MESH_PAYLOAD_TYPE_VIDEO);
}

// Temporarily commented out until there is a decision on RDMA config in JSON
// ----
// TEST(mesh_json_sdk, parse_conn_cfg_rdma) {
//     const char *str = R"({
//         "connection": {
//           "rdma": {
//             "connectionMode": "UC",
//             "maxLatencyNanoseconds": 10000
//           }
//         }
//       })";

//     ConnectionConfig config;
//     int err = config.parse_from_json(str);

//     ASSERT_EQ(err, 0);
//     EXPECT_EQ(config.conn_type, MESH_CONN_TYPE_RDMA);
//     EXPECT_EQ(config.conn.rdma.connection_mode, "UC");
//     EXPECT_EQ(config.conn.rdma.max_latency_ns, 10000);
//     EXPECT_EQ(config.payload_type, MESH_PAYLOAD_TYPE_BLOB);
// }

TEST(mesh_json_sdk, parse_conn_cfg_blob) {
  const char *str = R"({
      "maxPayloadSize": 921600,
      "connection": {
        "multipointGroup": {}
      },
      "payload": {
        "blob": {}
      }
    })";

  ConnectionConfig config;
  int err = config.parse_from_json(str);

  ASSERT_EQ(err, 0);
  EXPECT_EQ(config.payload_type, MESH_PAYLOAD_TYPE_BLOB);
  EXPECT_EQ(config.max_payload_size, 921600);
}

TEST(mesh_json_sdk, parse_conn_cfg_blob_calc) {
  const char *str = R"({
      "maxPayloadSize": 921600,
      "connection": {
        "multipointGroup": {}
      },
      "payload": {
        "blob": {}
      }
    })";

  ConnectionConfig config;
  config.parse_from_json(str);
  int err = config.calc_payload_size();

  ASSERT_EQ(err, 0);
  EXPECT_EQ(config.payload_type, MESH_PAYLOAD_TYPE_BLOB);
  EXPECT_EQ(config.calculated_payload_size, 921600);
}

TEST(mesh_json_sdk, parse_conn_cfg_video) {
    const char *str = R"({
        "connection": {
          "multipointGroup": {}
        },
        "payload": {
          "video": {
            "width": 1920,
            "height": 1080,
            "fps": 59.9,
            "pixelFormat": "yuv422p10le"
          }
        }
      })";

    ConnectionConfig config;
    int err = config.parse_from_json(str);

    ASSERT_EQ(err, 0);
    EXPECT_EQ(config.payload_type, MESH_PAYLOAD_TYPE_VIDEO);
    EXPECT_EQ(config.payload.video.width, 1920);
    EXPECT_EQ(config.payload.video.height, 1080);
    EXPECT_EQ(config.payload.video.fps, 59.9);
    EXPECT_EQ(config.payload.video.pixel_format, MESH_VIDEO_PIXEL_FORMAT_YUV422PLANAR10LE);
}

TEST(mesh_json_sdk, parse_conn_cfg_video_calc) {
    const char *str = R"({
        "connection": {
          "multipointGroup": {}
        },
        "payload": {
          "video": {
            "width": 1920,
            "height": 1080,
            "pixelFormat": "yuv422p10le"
          }
        }
      })";

    ConnectionConfig config;
    config.parse_from_json(str);
    int err = config.calc_payload_size();

    ASSERT_EQ(err, 0);
    EXPECT_EQ(config.calculated_payload_size, 1920 * 1080 * 4);
}

TEST(mesh_json_sdk, parse_conn_cfg_audio) {
    const char *str = R"({
        "connection": {
          "multipointGroup": {}
        },
        "payload": {
          "audio": {
            "channels": 4,
            "sampleRate": 96000,
            "format": "pcm_s16be",
            "packetTime": "125us"
          }
        }
      })";

    ConnectionConfig config;
    int err = config.parse_from_json(str);

    ASSERT_EQ(err, 0);
    EXPECT_EQ(config.payload_type, MESH_PAYLOAD_TYPE_AUDIO);
    EXPECT_EQ(config.payload.audio.channels, 4);
    EXPECT_EQ(config.payload.audio.sample_rate, MESH_AUDIO_SAMPLE_RATE_96000);
    EXPECT_EQ(config.payload.audio.format, MESH_AUDIO_FORMAT_PCM_S16BE);
    EXPECT_EQ(config.payload.audio.packet_time, MESH_AUDIO_PACKET_TIME_125US);
}

TEST(mesh_json_sdk, parse_conn_cfg_audio_calc) {
    const char *str = R"({
        "connection": {
          "multipointGroup": {}
        },
        "payload": {
          "audio": {
            "channels": 4,
            "sampleRate": 96000,
            "format": "pcm_s16be",
            "packetTime": "125us"
          }
        }
      })";

    ConnectionConfig config;
    config.parse_from_json(str);
    int err = config.calc_payload_size();

    ASSERT_EQ(err, 0);
    EXPECT_EQ(config.calculated_payload_size, 96);
}
