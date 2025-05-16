/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CLIENT_H
#define CLIENT_H

#include <string>

namespace mesh::client {

/**
 * Client
 * 
 * SDK client implementation.
 */
class Client {

public:
    Client() {}

    std::string id;
};

} // namespace mesh::client

#endif // CLIENT_H
