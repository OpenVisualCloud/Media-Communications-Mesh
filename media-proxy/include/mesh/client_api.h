/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CLIENT_API_H
#define CLIENT_API_H

#include "concurrency.h"

namespace mesh {

void RunSDKAPIServer(context::Context& ctx);

} // namespace mesh

#endif // CLIENT_API_H
