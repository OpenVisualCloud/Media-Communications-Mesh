/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "manager_bridges.h"
#include "conn_registry.h"
#include "logger.h"
#include "mocked_bridge.h"
#include "st2110tx.h"
#include "st2110rx.h"
#include "proxy_config.h"
#include "conn_rdma_tx.h"
#include "conn_rdma_rx.h"

namespace mesh::connection {

BridgesManager bridges_manager;

std::string_view getGroupURN(std::string_view group_id) {
    size_t pos = group_id.find('/');
    if (pos != std::string_view::npos) {
        return group_id.substr(0, pos);
    }
    return group_id;
}

int BridgesManager::create_bridge(context::Context& ctx, Connection*& bridge,
                                  const std::string& id, const BridgeConfig& cfg)
{
    // DEBUG
    // auto mocked_bridge = new(std::nothrow) MockedBridge;
    // if (!mocked_bridge)
    //     return -ENOMEM;

    // mocked_bridge->configure(ctx, kind);

    // bridge = mocked_bridge;
    // DEBUG

    MeshConfig_Video cfg_video;
    cfg_video.fps = 25;
    cfg_video.width = 640;
    cfg_video.height = 360;
    cfg_video.pixel_format = MESH_VIDEO_PIXEL_FORMAT_YUV422P10LE;

    // log::debug("CREATE BRIDGE")("type", cfg.type)("kind", kind2str(cfg.kind));

    if (!cfg.type.compare("st2110")) {
        MeshConfig_ST2110 cfg_st2110;

        strlcpy(cfg_st2110.local_ip_addr,
                config::proxy.st2110.dataplane_ip_addr.c_str(),
                sizeof(cfg_st2110.local_ip_addr));
        strlcpy(cfg_st2110.remote_ip_addr, cfg.st2110.remote_ip.c_str(),
                sizeof(cfg_st2110.remote_ip_addr));

        cfg_st2110.transport = MESH_CONN_TRANSPORT_ST2110_20;

        // Create Egress ST2110 Bridge
        if (cfg.kind == Kind::transmitter) {
            auto egress_bridge = new(std::nothrow) ST2110_20Tx;
            if (!egress_bridge)
                return -ENOMEM;

            cfg_st2110.remote_port = cfg.st2110.port;

            auto res = egress_bridge->configure(ctx,
                                                config::proxy.st2110.dev_port_bdf,
                                                cfg_st2110, cfg_video);
            if (res != Result::success) {
                log::error("Error configuring ST2110 Egress bridge: %s",
                           result2str(res));
                delete egress_bridge;
                return -1;
            }
            bridge = egress_bridge;

        // Create Ingress ST2110 Bridge
        } else if (cfg.kind == Kind::receiver) {
            auto ingress_bridge = new(std::nothrow) ST2110_20Rx;
            if (!ingress_bridge)
                return -ENOMEM;

            cfg_st2110.local_port = cfg.st2110.port;

            auto res = ingress_bridge->configure(ctx,
                                                 config::proxy.st2110.dev_port_bdf,
                                                 cfg_st2110, cfg_video);
            if (res != Result::success) {
                log::error("Error configuring ST2110 Ingress bridge: %s",
                           result2str(res));
                delete ingress_bridge;
                return -1;
            }
            bridge = ingress_bridge;
        } else {
            return -1;
        }
    } else if (!cfg.type.compare("rdma")) {
        libfabric_ctx *dev_handle = NULL;

        mcm_conn_param req = {};

        strlcpy(req.local_addr.ip, config::proxy.rdma.dataplane_ip_addr.c_str(),
                sizeof(req.local_addr.ip));
        strlcpy(req.remote_addr.ip, cfg.rdma.remote_ip.c_str(),
                sizeof(req.remote_addr.ip));

        req.payload_args.rdma_args.transfer_size = 921600;
        req.payload_args.rdma_args.queue_size = 16;

        // Create Egress RDMA Bridge
        if (cfg.kind == Kind::transmitter) {
            auto egress_bridge = new(std::nothrow) RdmaTx;
            if (!egress_bridge)
                return -ENOMEM;

            req.type = is_tx;

            snprintf(req.remote_addr.port, sizeof(req.remote_addr.port),
                     "%u", cfg.rdma.port);

            auto res = egress_bridge->configure(ctx, req, dev_handle);
            if (res != Result::success) {
                log::error("Error configuring RDMA Egress bridge: %s",
                           result2str(res));
                delete egress_bridge;
                return -1;
            }
            bridge = egress_bridge;

        // Create Ingress RDMA Bridge
        } else if (cfg.kind == Kind::receiver) {
            auto ingress_bridge = new(std::nothrow) RdmaRx;
            if (!ingress_bridge)
                return -ENOMEM;

            req.type = is_rx;
            
            snprintf(req.local_addr.port, sizeof(req.local_addr.port),
                     "%u", cfg.rdma.port);

            auto res = ingress_bridge->configure(ctx, req, dev_handle);
            if (res != Result::success) {
                log::error("Error configuring RDMA Ingress bridge: %s",
                           result2str(res));
                delete ingress_bridge;
                return -1;
            }
            bridge = ingress_bridge;

        } else {
            return -1;
        }
    }

    // log::debug("BEFORE ESTABLISH");

    auto res = bridge->establish_async(ctx);
    if (res != Result::success) {
        delete bridge;
        return -1;
    }

    // log::debug("ESTABLISH COMPLETED");

    lock();
    thread::Defer d([this]{ unlock(); });

    // Assign id accessed by metrics collector.
    bridge->assign_id(id);

    registry.add(id, bridge);

    return 0;
}

int BridgesManager::delete_bridge(context::Context& ctx, const std::string& id)
{
    auto bridge = registry.get(id);
    if (!bridge)
        return -1;

    {
        lock();
        thread::Defer d([this]{ unlock(); });

        if (bridge->link()) {
            // log::debug("Shutdown the link");
            bridge->link()->set_link(ctx, nullptr, bridge);
            bridge->set_link(ctx, nullptr);
        }

        registry.remove(id);
    }

    auto res = bridge->shutdown_async(ctx);
    // delete bridge; // The instance is deleted in a thread in shutdown_async()

    return 0;
}

Connection * BridgesManager::get_bridge(context::Context& ctx,
                                        const std::string& id)
{
    return registry.get(id);
}

void BridgesManager::shutdown(context::Context& ctx)
{
    auto ids = registry.get_all_ids();

    for (const std::string& id : ids) {
        auto err = delete_bridge(ctx, id);
        if (err)
            log::error("Error deleting bridge (%d)", err)
                      ("bridge_id", id);
    }
}

void BridgesManager::lock()
{
    // log::debug("Bridges Manager mx lock");
    mx.lock();
}

void BridgesManager::unlock()
{
    // log::debug("Bridges Manager mx unlock");
    mx.unlock();
}

} // namespace mesh::connection
