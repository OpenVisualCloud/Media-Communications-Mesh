/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CONN_LOCAL_TX_H
#define CONN_LOCAL_TX_H

#include "conn_local.h"

namespace mesh::connection {

class LocalTx : public Local {

public:
    LocalTx();

private:
    void default_memif_ops(memif_ops_t *ops) override;
    int on_memif_receive(void *ptr, uint32_t sz) override;

    Result on_receive(context::Context& ctx, void *ptr, uint32_t sz,
                      uint32_t& sent) override;
};

} // namespace mesh::connection

#endif // CONN_LOCAL_TX_H
