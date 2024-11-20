/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "uuid.h"
#include <random>
#include <iomanip>
#include <mutex>

namespace mesh {

std::string generate_uuid_v4()
{
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;
    static std::mutex mx;

    std::array<uint8_t, 16> uuid;

    {
        std::lock_guard<std::mutex> lk(mx);
        for (auto& byte : uuid) {
            byte = static_cast<uint8_t>(dis(gen));
        }
    }

    // Set the version to 4 (UUIDv4)
    uuid[6] = (uuid[6] & 0x0F) | 0x40;
    // Set the variant to 2 (RFC 4122)
    uuid[8] = (uuid[8] & 0x3F) | 0x80;

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < uuid.size(); ++i) {
        oss << std::setw(2) << static_cast<int>(uuid[i]);
        if (i == 3 || i == 5 || i == 7 || i == 9) {
            oss << '-';
        }
    }

    return oss.str();
}

} // namespace mesh
