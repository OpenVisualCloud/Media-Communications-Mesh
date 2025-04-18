/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "mesh_client.h"
#include <string.h>
#include "mesh_conn.h"
#include "mesh_logger.h"
#include "json.hpp"
#include "mesh_dp_legacy.h"

namespace mesh {

class KeyValueString {
public:
    KeyValueString() = default;

    void parse(const std::string& str) {
        std::istringstream stream(str);
        std::string token;

        while (std::getline(stream, token, ';')) {
            std::istringstream tkStream(token);
            std::string key, value;

            if (std::getline(tkStream, key, '=') && std::getline(tkStream, value)) {
                // Remove leading and trailing whitespace from key and value
                key.erase(0, key.find_first_not_of(" \t\n\r\f\v"));
                key.erase(key.find_last_not_of(" \t\n\r\f\v") + 1);
                value.erase(0, value.find_first_not_of(" \t\n\r\f\v"));
                value.erase(value.find_last_not_of(" \t\n\r\f\v") + 1);

                storage[key] = value;
            }
        }
    }

    std::optional<std::string> value(const char *key) {
        auto it = storage.find(key);
        if (it != storage.end())
            return it->second;
        else
            return std::nullopt;
    }

private:
    std::unordered_map<std::string, std::string> storage;
};

int ClientConfig::parse_from_json(const char *str)
{
    try {
        auto j = nlohmann::json::parse(str);
        const char *field;

        api_version = j.value("apiVersion", "v1");
        default_timeout_us = j.value("apiDefaultTimeoutMicroseconds", 1000000);
        max_conn_num = j.value("maxMediaConnections", 32);
        std::string conn_str = j.value("apiConnectionString", "");

        KeyValueString params;
        params.parse(conn_str);

        auto ip = params.value("Server");
        if (ip) {
            proxy_ip = *ip;
        } else {
            auto env = getenv("MCM_MEDIA_PROXY_IP");
            if (env)
                proxy_ip = env;
            else
                proxy_ip = "127.0.0.1";
        }

        auto port = params.value("Port");
        if (port) {
            proxy_port = *port;
        } else {
            auto env = getenv("MCM_MEDIA_PROXY_PORT");
            if (env)
                proxy_port = env;
            else
                proxy_port = "8002";
        }

        return 0;
    } catch (const nlohmann::json::exception& e) {
        log::error("client cfg json parse err: %s", e.what());
    } catch (const std::exception& e) {
        log::error("client cfg json parse std err: %s", e.what());
    }
    return -MESH_ERR_CLIENT_CONFIG_INVAL;
}

ClientContext::ClientContext()
{
}

int ClientContext::shutdown()
{
    std::lock_guard<std::mutex> lk(mx);

    if (!conns.empty())
        return -MESH_ERR_FOUND_ALLOCATED;

    /**
     * TODO: Shutdown and deallocate connections here
     */

    mesh_internal_ops.grpc_destroy_client(grpc_client);
    grpc_client = NULL;

    return 0;
}

int ClientContext::init(const char *config)
{
    log::debug("JSON client config: %s", config);

    int err = cfg.parse_from_json(config);
    if (err)
        return err;

    std::string endpoint = cfg.proxy_ip + ":" + cfg.proxy_port;
    grpc_client = mesh_internal_ops.grpc_create_client_json(endpoint);

    return 0;
}

int ClientContext::create_conn(MeshConnection **conn, int kind)
{
    if (!conn)
        return -MESH_ERR_BAD_CONN_PTR;

    if (kind != MESH_CONN_KIND_SENDER && kind != MESH_CONN_KIND_RECEIVER)
        return -MESH_ERR_CONN_CONFIG_INVAL;

    std::lock_guard<std::mutex> lk(mx);

    if ((long int)conns.size() >= cfg.max_conn_num)
        return -MESH_ERR_MAX_CONN;

    ConnectionContext *conn_ctx = new(std::nothrow) ConnectionContext(this);
    if (!conn_ctx)
        return -ENOMEM;

    conn_ctx->cfg.kind = kind;

    try {
        conns.push_back(conn_ctx);
    }
    catch (...) {
        delete conn_ctx;
        return -ENOMEM;
    }

    *conn = (MeshConnection *)conn_ctx;

    return 0;
}

} // namespace mesh
