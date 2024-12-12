/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MESH_JSON_H
#define MESH_JSON_H

#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace mesh {

struct ClientConfig {
    std::string apiVersion;
    std::string addr;
    std::string port;
    int apiDefaultTimeoutMicroseconds;
    int maxMediaConnections;
};

struct MultipointGroup {
    std::string urn;
};

enum ST2110_Transport {
    TRANSPORT_ST2110_INVALID,
    TRANSPORT_ST2110_20,
    TRANSPORT_ST2110_22,
    TRANSPORT_ST2110_30,
    TRANSPORT_ST2110_MAX
};

enum ST2110_Pacing {
    PACING_ST2110_INVALID,
    PACING_ST2110_DEFAULT,
    PACING_ST2110_20_NARROW,
    PACING_ST2110_20_WIDE,
    PACING_ST2110_20_LINEAR,
    PACING_ST2110_30_RL,
    PACING_ST2110_30_TSC,
    PACING_ST2110_MAX
};

struct ST2110Config {
    ST2110_Transport transport;
    std::string remoteIpAddr;
    uint remotePort;
    ST2110_Pacing pacing;
    uint payloadType;
};

enum RDMA_ConnectionMode {
    CONNECTION_MODE_INVALID,
    CONNECTION_MODE_DEFAULT,
    CONNECTION_MODE_RC,
    CONNECTION_MODE_UC,
    CONNECTION_MODE_UD,
    CONNECTION_MODE_RD,
    CONNECTION_MODE_MAX,
};

struct RdmaConfig {
    RDMA_ConnectionMode connectionMode;
    uint maxLatencyNs;
};

struct Connection {
    std::vector<MultipointGroup> multipointGroup;
    std::vector<ST2110Config> st2110;
    std::vector<RdmaConfig> rdma;
};

enum Video_Format {
    VIDEO_FORMAT_INVALID,
    VIDEO_FORMAT_NV12,        ///< planar YUV 4:2:0, 12bpp
    VIDEO_FORMAT_YUV422P,     ///< planar YUV 4:2:2, 16bpp
    VIDEO_FORMAT_YUV422P10LE, ///< planar YUV 4:2:2, 20bpp
    VIDEO_FORMAT_YUV444P10LE, ///< planar YUV 4:4:4, 30bpp
    VIDEO_FORMAT_RGB8,        ///< packed RGB 3:3:2,  8bpp
    VIDEO_FORMAT_MAX
};

struct VideoConfig {
    uint width;
    uint height;
    double fps;
    Video_Format pixelFormat;
};

enum Audio_SampleRate {
    Audio_SAMPLE_RATE_INVALID,
    AUDIO_SAMPLE_RATE_48000, ///< Audio sample rate 48000 Hz
    AUDIO_SAMPLE_RATE_96000, ///< Audio sample rate 96000 Hz
    AUDIO_SAMPLE_RATE_44100, ///< Audio sample rate 44100 Hz
    AUDIO_SAMPLE_RATE_MAX
};

enum Audio_Format {
    AUDIO_FORMAT_INVALID,
    AUDIO_FORMAT_PCM_S8,    ///< PCM 8 bits per channel
    AUDIO_FORMAT_PCM_S16BE, ///< PCM 16 bits per channel, big endian
    AUDIO_FORMAT_PCM_S24BE, ///< PCM 24 bits per channel, big endian
    AUDIO_FORMAT_MAX
};

enum Audio_PacketTime {
    AUDIO_PACKET_TIME_INVALID,
    /** Constants for 48kHz and 96kHz sample rates */
    AUDIO_PACKET_TIME_1MS,   ///< Audio packet time 1ms
    AUDIO_PACKET_TIME_125US, ///< Audio packet time 125us
    AUDIO_PACKET_TIME_250US, ///< Audio packet time 250us
    AUDIO_PACKET_TIME_333US, ///< Audio packet time 333us
    AUDIO_PACKET_TIME_4MS,   ///< Audio packet time 4ms
    AUDIO_PACKET_TIME_80US,  ///< Audio packet time 80us
    /** Constants for 44.1kHz sample rate */
    AUDIO_PACKET_TIME_1_09MS, ///< Audio packet time 1.09ms
    AUDIO_PACKET_TIME_0_14MS, ///< Audio packet time 0.14ms
    AUDIO_PACKET_TIME_0_09MS, ///< Audio packet time 0.09ms
    AUDIO_PACKET_TIME_MAX
};

struct AudioConfig {
    uint channels;
    Audio_SampleRate sampleRate;
    Audio_Format format;
    Audio_PacketTime packetTime;
};

struct Payload {
    std::vector<VideoConfig> video;
    std::vector<AudioConfig> audio;
    // Add structures for ancillary and blob if needed
};

struct ConnectionConfiguration {
    uint bufferQueueCapacity;
    uint maxPayloadSize;
    uint maxMetadataSize;
    Connection connection;
    Payload payload;
};

void from_json(const json& j, ClientConfig& config);
void from_json(const json& j, MultipointGroup& config);
void from_json(const json& j, ST2110Config& config);
void from_json(const json& j, RdmaConfig& config);
void from_json(const json& j, Connection& config);
void from_json(const json& j, VideoConfig& config);
void from_json(const json& j, AudioConfig& config);
void from_json(const json& j, Payload& config);
void from_json(const json& j, ConnectionConfiguration& config);

NLOHMANN_JSON_SERIALIZE_ENUM(ST2110_Transport,
{
    { TRANSPORT_ST2110_INVALID, ""}, // Undefined JSON values will default to the first pair specified in map
    { TRANSPORT_ST2110_20, "st2110-20" },
    { TRANSPORT_ST2110_22, "st2110-22" },
    { TRANSPORT_ST2110_30, "st2110-30" },
})

NLOHMANN_JSON_SERIALIZE_ENUM(ST2110_Pacing,
{
    { PACING_ST2110_INVALID,   "" }, // Undefined JSON values will default to the first pair specified in map
    { PACING_ST2110_DEFAULT,   "default"},
    { PACING_ST2110_20_NARROW, "narrow" },
    { PACING_ST2110_20_WIDE,   "wide" },
    { PACING_ST2110_20_LINEAR, "linear" },
    { PACING_ST2110_30_RL,     "rl" },
    { PACING_ST2110_30_TSC,    "tsc" }
})

NLOHMANN_JSON_SERIALIZE_ENUM(RDMA_ConnectionMode,
{
    { CONNECTION_MODE_INVALID, "" }, // Undefined JSON values will default to the first pair specified in map
    { CONNECTION_MODE_DEFAULT, "default" },
    { CONNECTION_MODE_RC,      "RC" },
    { CONNECTION_MODE_UC,      "UC" },
    { CONNECTION_MODE_UD,      "UD" },
    { CONNECTION_MODE_RD,      "RD" }
})

NLOHMANN_JSON_SERIALIZE_ENUM(Video_Format,
{
    { VIDEO_FORMAT_INVALID,     "" }, // Undefined JSON values will default to the first pair specified in map
    { VIDEO_FORMAT_NV12,        "nv12" },
    { VIDEO_FORMAT_YUV422P,     "yuv422p" },
    { VIDEO_FORMAT_YUV422P10LE, "yuv422p10le" },
    { VIDEO_FORMAT_YUV444P10LE, "yuv444p10le" },
    { VIDEO_FORMAT_RGB8,        "rgb8" }
})

NLOHMANN_JSON_SERIALIZE_ENUM(Audio_SampleRate,
{
    { Audio_SAMPLE_RATE_INVALID, "" }, // Undefined JSON values will default to the first pair specified in map
    { AUDIO_SAMPLE_RATE_48000, "48000" },
    { AUDIO_SAMPLE_RATE_96000, "96000" },
    { AUDIO_SAMPLE_RATE_44100, "44100" }
})

NLOHMANN_JSON_SERIALIZE_ENUM(Audio_Format,
{
    { AUDIO_FORMAT_INVALID,   "" }, // Undefined JSON values will default to the first pair specified in map
    { AUDIO_FORMAT_PCM_S8,    "pcm_s8" },
    { AUDIO_FORMAT_PCM_S16BE, "pcm_s16be" },
    { AUDIO_FORMAT_PCM_S24BE, "pcm_s24be" }
})

NLOHMANN_JSON_SERIALIZE_ENUM(Audio_PacketTime,
{
    { AUDIO_PACKET_TIME_INVALID, "" }, // undefined JSON values will default to the first pair specified in map
    { AUDIO_PACKET_TIME_1MS,    "1ms" },
    { AUDIO_PACKET_TIME_125US,  "125us" },
    { AUDIO_PACKET_TIME_250US,  "250us" },
    { AUDIO_PACKET_TIME_333US,  "333us" },
    { AUDIO_PACKET_TIME_4MS,    "4ms" },
    { AUDIO_PACKET_TIME_80US,   "80us" },
    { AUDIO_PACKET_TIME_1_09MS, "1.09ms" },
    { AUDIO_PACKET_TIME_0_14MS, "0.14ms" },
    { AUDIO_PACKET_TIME_0_09MS, "0.09ms" }
})

} // namespace mesh

#endif // MESH_JSON_H
