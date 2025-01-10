/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PROXY_CONFIG_H
#define PROXY_CONFIG_H

#include <string>

namespace mesh::config {

class Proxy {
public:
    struct {
        std::string dev_port_bdf;
        std::string dataplane_ip_addr;
    } st2110 = {
        .dev_port_bdf = "0000:31:00.0",
        .dataplane_ip_addr = "192.168.96.1",
    };

    struct {
        std::string dataplane_ip_addr;
        std::string dataplane_local_ports;
    } rdma = {
        .dataplane_ip_addr = "192.168.96.2",
        .dataplane_local_ports = "9100-9999",
    };

    uint16_t sdk_api_port = 8002;
    std::string agent_addr = "localhost:50051";
};

extern Proxy proxy;

} // namespace mesh::config

#endif // PROXY_CONFIG_H
