/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef MESH_CONN_ZC_H
#define MESH_CONN_ZC_H

#include "mesh_conn.h"
#include "gateway_zc.h"
#include "uuid.h"
#include "gateway_zc.h"

namespace mesh {

using namespace zerocopy::gateway;

class ZeroCopyRxEvent {
public:
    void *ptr;
    uint32_t sz;
    int err;
};

class ZeroCopyConnectionContext : public ConnectionContext {
public:
    explicit ZeroCopyConnectionContext(ClientContext *parent) : ConnectionContext(parent) {}
    ~ZeroCopyConnectionContext() override {}

    int establish() override;
    int shutdown() override;
    int get_buffer(MeshBuffer **buf, int timeout_ms) override;
    int put_buffer(MeshBuffer *buf, int timeout_ms) override;

    std::string temporary_id = generate_uuid_v4();

    thread::Channel<zerocopy::Config> zero_copy_config_ch;
    zerocopy::Config zc_config;

    GatewayTx gw;

    thread::Channel<ZeroCopyRxEvent> zero_copy_rx_ch;

    GatewayRx gw_rx;
};

} // namespace mesh

#endif // MESH_CONN_ZC_H
