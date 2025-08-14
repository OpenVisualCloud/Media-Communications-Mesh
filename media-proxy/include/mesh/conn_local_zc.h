/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CONN_LOCAL_ZC_H
#define CONN_LOCAL_ZC_H

#include "conn_local_tx.h"
#include "gateway_zc.h"

namespace mesh::connection {

using namespace zerocopy::gateway;

class ZeroCopyLocal : public Connection {
public:
    ZeroCopyLocal() : Connection() {}

    Result configure(context::Context& ctx);

    Result set_link(context::Context& ctx, Connection *new_link,
                    Connection *requester = nullptr) override;

    std::string sdk_temporary_id;

private:
    Result on_establish(context::Context& ctx) override;
    Result on_shutdown(context::Context& ctx) override;
};

} // namespace mesh::connection

#endif // CONN_LOCAL_ZC_H
