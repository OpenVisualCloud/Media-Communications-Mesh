/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MULTIPOINT_COPY_H
#define MULTIPOINT_COPY_H

#include "multipoint.h"
#include <list>

namespace mesh::multipoint {

/**
 * Copy-based multipoint group implementation.
 */
class CopyGroup : public Group {
public:
    CopyGroup(const std::string& id) : Group(id) {}
    ~CopyGroup() override {}

private:
    sync::DataplaneAtomicPtr outputs_ptr;

    Result on_establish(context::Context& ctx) override;
    Result on_receive(context::Context& ctx, void *ptr, uint32_t sz,
                      uint32_t& sent) override;

    void on_outputs_updated() override;

    std::list<Connection *> * get_hotpath_outputs_lock();
    void hotpath_outputs_unlock();
    void set_hotpath_outputs(std::list<Connection *> *new_outputs);
};

} // namespace mesh::multipoint

#endif // MULTIPOINT_COPY_H
