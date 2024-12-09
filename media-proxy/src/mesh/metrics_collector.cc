/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "metrics_collector.h"
#include "proxy_api.h"
#include "logger.h"
#include "manager_local.h"

namespace mesh::telemetry {

Registry registry;

void Registry::register_provider(MetricsProvider *provider)
{
    std::lock_guard<std::mutex> lk(mx);
    providers.push_back(provider);
}

void Registry::unregister_provider(MetricsProvider *provider)
{
    std::lock_guard<std::mutex> lk(mx);
    providers.remove(provider);
}

void Registry::lock() {
    mx.lock();
}

void Registry::unlock() {
    mx.unlock();
}

void MetricsCollector::run(context::Context& ctx)
{
    std::vector<Metric> metrics;

    while (!ctx.cancelled()) {
        auto now = std::chrono::system_clock::now().time_since_epoch();
        int64_t timestamp_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

        // Temporarily block all known metric providers and entities
        // responsible for creation or deleting metric providers
        // from being deleted. The hot path of connections is not affected
        // by this locking.
        connection::local_manager.lock();
        registry.lock();

        uint64_t n = 0;
        for (auto provider : registry.providers) {
            // Don't collect a metric if no id is assigned to the provider.
            if (provider->id.empty())
                continue;
            
            metrics.emplace_back(timestamp_ms);
            auto& metric = metrics.back();
            provider->collect(metric, timestamp_ms);

            // Remove the metric if no fields were added (highly unlikely).
            if (metric.fields.empty()) {
                metrics.pop_back();
            } else {
                metric.provider_id = provider->id;
                n++;
            }
        }
        total += n;

        // Finally remove locks in reverse order.
        registry.unlock();
        connection::local_manager.unlock();

        proxyApiClient->SendMetrics(metrics);

        metrics.clear();

        thread::Sleep(ctx, std::chrono::milliseconds(1000));
    }
}

void MetricsCollector::collect(Metric& metric, const int64_t& timestamp_ms)
{
    metric.addFieldUint64("total", total);
}

} // namespace mesh::telemetry
