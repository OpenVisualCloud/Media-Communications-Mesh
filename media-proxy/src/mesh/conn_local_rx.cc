/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "conn_local_rx.h"
#include <string.h>
#include "logger.h"

namespace mesh::connection {

LocalRx::LocalRx() : Local()
{
    _kind = Kind::receiver;
    no_link_reported = false;
}

void LocalRx::default_memif_ops(memif_ops_t *ops)
{
    strncpy(ops->app_name, "mcm_tx", sizeof(ops->app_name));
    strncpy(ops->interface_name, "mcm_tx", sizeof(ops->interface_name));
    strncpy(ops->socket_path, "/run/mcm/mcm_tx_memif.sock", sizeof(ops->socket_path));
}

int LocalRx::on_memif_receive(void *ptr, uint32_t sz)
{
    if (!link() && !no_link_reported) {
        no_link_reported = true;
        log::warn("Local %s conn: no link", kind2str(_kind, true));
    }

    auto res = transmit(context::Background(), ptr, sz);
    switch (res) {
    case Result::error_no_link_assigned:
    case Result::success:
        return 0;
    default:
        log::error("Local Rx conn transmit err: %s", result2str(res))("sz", sz);
        return -1;
    }
}

} // namespace mesh::connection
