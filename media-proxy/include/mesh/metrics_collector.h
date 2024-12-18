/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef METRICS_COLLECTOR_H
#define METRICS_COLLECTOR_H

#include "metrics.h"
#include "concurrency.h"
#include <list>

namespace mesh::telemetry {

class Registry {
public:
    void register_provider(MetricsProvider *provider);
    void unregister_provider(MetricsProvider *provider);
    void lock();
    void unlock();

protected:
    std::list<MetricsProvider *> providers;
    std::mutex mx;

    friend class MetricsCollector;
};

class MetricsCollector : MetricsProvider {
public:
    MetricsCollector() { assign_id("collector"); }

    void run(context::Context& ctx);

private:
    virtual void collect(Metric& metric, const int64_t& timestamp_ms);

    std::atomic<uint64_t> total;
};

extern Registry registry;

} // namespace mesh::telemetry

#endif // METRICS_COLLECTOR_H
