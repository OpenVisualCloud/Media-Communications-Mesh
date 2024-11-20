/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MANAGER_LOCAL_H
#define MANAGER_LOCAL_H

#include "mcm_dp.h"
#include <string>
#include "concurrency.h"
#include "conn_registry.h"

namespace mesh::connection {

class LocalManager {
public:
    int create_connection(context::Context& ctx, std::string& id,
                          mcm_conn_param *param, memif_conn_param *memif_param);

    int delete_connection(context::Context& ctx, const std::string& id);

    void shutdown(context::Context& ctx);

private:
    Registry registry;
    std::shared_mutex mx;
};

extern LocalManager local_manager;

} // namespace mesh::connection

#endif // MANAGER_LOCAL_H
