/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "mesh_json_proxy.h"
#include <regex>

namespace mesh {

void from_json(const json& j, MultipointGroup& config) { j.at("urn").get_to(config.urn); }

void from_json(const json& j, ST2110Config& config) {
    j.at("transport").get_to(config.transport);
    j.at("remoteIpAddr").get_to(config.remoteIpAddr);
    j.at("remotePort").get_to(config.remotePort);
    // Optional parameter, default to PACING_ST2110_DEFAULT
    config.pacing = j.value("pacing", ST2110_Pacing::PACING_ST2110_DEFAULT);
    j.at("payloadType").get_to(config.payloadType);
}

void from_json(const json& j, RdmaConfig& config) {
    // Optional parameter, default to CONNECTION_MODE_DEFAULT
    config.connectionMode = j.value("connectionMode", RDMA_ConnectionMode::CONNECTION_MODE_DEFAULT);
    j.at("maxLatencyNs").get_to(config.maxLatencyNs);
}

void from_json(const json& j, Connection& config) {
    if (j.contains("multipoint-group")) {
        if (j.at("multipoint-group").is_array()) {
            config.multipointGroup = j.at("multipoint-group").get<std::vector<MultipointGroup>>();
        } else {
            config.multipointGroup.push_back(j.at("multipoint-group").get<MultipointGroup>());
        }
    }
    if (j.contains("st2110")) {
        if (j.at("st2110").is_array()) {
            config.st2110 = j.at("st2110").get<std::vector<ST2110Config>>();
        } else {
            config.st2110.push_back(j.at("st2110").get<ST2110Config>());
        }
    }
    if (j.contains("rdma")) {
        if (j.at("rdma").is_array()) {
            config.rdma = j.at("rdma").get<std::vector<RdmaConfig>>();
        } else {
            config.rdma.push_back(j.at("rdma").get<RdmaConfig>());
        }
    }
}

void from_json(const json& j, VideoConfig& config) {
    j.at("width").get_to(config.width);
    j.at("height").get_to(config.height);
    j.at("fps").get_to(config.fps);
    j.at("pixelFormat").get_to(config.pixelFormat);
}

void from_json(const json& j, AudioConfig& config) {
    j.at("channels").get_to(config.channels);
    j.at("sampleRate").get_to(config.sampleRate);
    j.at("format").get_to(config.format);
    j.at("packetTime").get_to(config.packetTime);
}

void from_json(const json& j, Payload& config) {
    if (j.contains("video")) {
        if (j.at("video").is_array()) {
            config.video = j.at("video").get<std::vector<VideoConfig>>();
        } else {
            config.video.push_back(j.at("video").get<VideoConfig>());
        }
    }
    if (j.contains("audio")) {
        if (j.at("audio").is_array()) {
            config.audio = j.at("audio").get<std::vector<AudioConfig>>();
        } else {
            config.audio.push_back(j.at("audio").get<AudioConfig>());
        }
    }
}

void from_json(const json& j, ConnectionConfiguration& config) {
    config.bufferQueueCapacity = j.value("bufferQueueCapacity", 16);
    config.maxPayloadSize = j.value("maxPayloadSize", 0);
    config.maxMetadataSize = j.value("maxMetadataSize", 0);
    j.at("connection").get_to(config.connection);
    j.at("payload").get_to(config.payload);
}

} // namespace mesh
