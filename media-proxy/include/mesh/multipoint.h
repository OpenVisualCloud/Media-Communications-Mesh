/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MULTIPOINT_H
#define MULTIPOINT_H

#include <list>
#include "metrics.h"
#include "concurrency.h"
#include "conn.h"

using namespace mesh::connection;

namespace mesh::multipoint {

class Group : public Connection {
public:
    Group(const std::string& id);
    ~Group() {}

    void configure(context::Context& ctx);

    Result assign_input(context::Context& ctx, Connection *input);
    Result add_output(context::Context& ctx, Connection *output);

    bool input_assigned();
    int outputs_num();

protected:
    std::list<Connection *> outputs;
    std::mutex outputs_mx;

    Result on_shutdown(context::Context& ctx) override;

    virtual void on_outputs_updated() {}

private:
    Result set_link(context::Context& ctx, Connection *new_link,
                    Connection *requester = nullptr) override;
};

} // namespace mesh::multipoint

#endif // MULTIPOINT_H
