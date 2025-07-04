/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MULTIPOINT_ZC_H
#define MULTIPOINT_ZC_H

#include "multipoint.h"
#include "gateway_zc.h"

namespace mesh::multipoint {

/**
 * Zero-copy multipoint group implementation.
 */
class ZeroCopyGroup : public Group {
public:
    ZeroCopyGroup(const std::string& id) : Group(id) {}

    Result on_establish(context::Context& ctx) override;
    Result on_shutdown(context::Context& ctx) override;

    zerocopy::Config * get_config();

private:
    zerocopy::Config cfg;
    int shmid = -1;
};

Result zc_init_gateway_from_group(context::Context& ctx,
                                  zerocopy::gateway::Gateway *gw,
                                  Connection *group);

} // namespace mesh::multipoint

#endif // MULTIPOINT_ZC_H
