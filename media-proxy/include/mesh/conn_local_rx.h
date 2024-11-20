/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CONN_LOCAL_RX_H
#define CONN_LOCAL_RX_H

#include "conn_local.h"

namespace mesh::connection {

class LocalRx : public Local {
public:
    LocalRx();

private:
    void default_memif_ops(memif_ops_t *ops) override;
    int on_memif_receive(void *ptr, uint32_t sz) override;

    bool no_link_reported;
};

} // namespace mesh::connection

#endif // CONN_LOCAL_RX_H
