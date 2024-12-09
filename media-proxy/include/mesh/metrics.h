/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef METRICS_H
#define METRICS_H

#include <vector>
#include <string>
#include <variant>

namespace mesh::telemetry {

class MetricField {
public:
    MetricField(const std::string& name, const std::string& str_value)
        : name(name), value(str_value) {}

    MetricField(const std::string& name, uint64_t& uint_value)
        : name(name), value(uint_value) {}

    MetricField(const std::string& name, double& double_value)
        : name(name), value(double_value) {}

    MetricField(const std::string& name, bool bool_value)
        : name(name), value(bool_value) {}

    std::string name;
    std::variant<std::string, uint64_t, double, bool> value;
};

class Metric {
public:
    Metric(int64_t timestamp_ms)
        : timestamp_ms(timestamp_ms) {}

    void addFieldString(const std::string& name, const std::string& str_value) {
        fields.emplace_back(name, str_value);
    }

    void addFieldUint64(const std::string& name, uint64_t uint_value) {
        fields.emplace_back(name, uint_value);
    }

    void addFieldDouble(const std::string& name, double double_value) {
        fields.emplace_back(name, double_value);
    }

    void addFieldBool(const std::string& name, bool bool_value) {
        fields.emplace_back(name, bool_value);
    }

    int64_t timestamp_ms;
    std::string provider_id;
    std::vector<MetricField> fields;
};

class MetricsProvider {
public:
    void assign_id(const std::string& id);

    std::string id;

protected:
    MetricsProvider();
    virtual ~MetricsProvider();

    virtual void collect(Metric& metric, const int64_t& timestamp_ms) {}
    // virtual std::unordered_map<std::string, uint8_t> metrics_map();
    // virtual void reset(uint64_t mask) {}

    friend class MetricsCollector;
};

} // namespace mesh::telemetry

#endif // METRICS_H
