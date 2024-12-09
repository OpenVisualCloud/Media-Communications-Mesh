/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "metrics.h"
#include "metrics_collector.h"
#include "logger.h"

namespace mesh::telemetry {

MetricsProvider::MetricsProvider()
{
    registry.register_provider(this);
}

MetricsProvider::~MetricsProvider()
{
    registry.unregister_provider(this);
}

void MetricsProvider::assign_id(const std::string& id)
{
    this->id = id;
}

// std::unordered_map<std::string, uint8_t> Provider::metrics_map()
// {
//     return std::unordered_map<std::string, uint8_t>();
// }

} // namespace mesh::telemetry
