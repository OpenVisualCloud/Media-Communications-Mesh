/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MESH_JSON_SDK_H
#define MESH_JSON_SDK_H

#include "json.hpp"
using json = nlohmann::json;

namespace mesh {

struct ClientConfig {
    std::string apiVersion;
    std::string addr;
    std::string port;
    int apiDefaultTimeoutMicroseconds;
    int maxMediaConnections;
};

void from_json(const json& j, ClientConfig& config);

} // namespace mesh

#endif // MESH_JSON_SDK_H
