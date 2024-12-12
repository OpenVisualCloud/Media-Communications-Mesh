/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "mesh_json_sdk.h"
#include <regex>

namespace mesh {

std::vector<std::string> split_string_by_delimiter(const std::string& str,
                                                   const std::string& delimiter) {
    std::vector<std::string> result;
    std::regex re(delimiter);
    std::sregex_token_iterator it(str.begin(), str.end(), re, -1);
    std::sregex_token_iterator reg_end;

    for (; it != reg_end; ++it) {
        result.push_back(it->str());
    }

    return result;
}

void from_json(const json& j, ClientConfig& config) {
    std::string apiConnectionString;
    j.at("apiConnectionString").get_to(apiConnectionString);
    j.at("apiVersion").get_to(config.apiVersion);
    config.apiDefaultTimeoutMicroseconds = j.value("apiDefaultTimeoutMicroseconds", 0);
    config.maxMediaConnections = j.value("maxMediaConnections", 0);

    std::vector<std::string> parts = split_string_by_delimiter(apiConnectionString, "; ");
    for (const auto& part : parts) {
        std::vector<std::string> key_value = split_string_by_delimiter(part, "=");
        if (key_value.size() == 2) {
            if (key_value[0] == "Server") {
                config.addr = key_value[1];
            } else if (key_value[0] == "Port") {
                config.port = key_value[1];
            } else {
                throw std::invalid_argument("Invalid key in apiConnectionString " + part);
            }
        } else {
            throw std::invalid_argument("Missing value in key-value pair " + part);
        }
    }
}

} // namespace mesh
