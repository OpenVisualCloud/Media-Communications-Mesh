/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "multipoint_zc.h"
#include <sys/shm.h>
#include "logger.h"

namespace mesh::multipoint {

key_t generate_sysv_key(const std::string& input) {
    // Use std::hash to get a size_t hash of the string
    std::size_t hash_value = std::hash<std::string>{}(input);

    // Mix the 64-bit hash down to 32 bits using a good mixing function
    // This is a simplified variant of Thomas Wang's 64-to-32 bit hash
    uint32_t lower = static_cast<uint32_t>(hash_value);
    uint32_t upper = static_cast<uint32_t>(hash_value >> 32);
    uint32_t mixed = lower ^ upper;

    // Further mix bits to reduce clustering
    mixed ^= (mixed >> 16);
    mixed *= 0x85ebca6b;
    mixed ^= (mixed >> 13);
    mixed *= 0xc2b2ae35;
    mixed ^= (mixed >> 16);

    return mixed;
}

Result ZeroCopyGroup::on_establish(context::Context& ctx)
{
    // TODO: shmget create

    cfg.sysv_key = generate_sysv_key(id);
    cfg.mem_region_sz = config.buf_parts.total_size() + 4;

    log::debug("SHM on_establish %u", cfg.mem_region_sz);

    shmid = shmget(cfg.sysv_key, cfg.mem_region_sz, IPC_CREAT | IPC_EXCL | 0666);
    if (shmid < 0) {
        log::error("shmid negative - establish %d", errno);
        set_state(ctx, State::closed);
        return Result::error_general_failure;
    }
    log::debug("SHMID %d", shmid);






    set_state(ctx, State::active);
    set_status(ctx, Status::healthy);

    return Result::success;
}

Result ZeroCopyGroup::on_shutdown(context::Context& ctx)
{
    // TODO: shmctl RMID

    auto res = shmctl(shmid, IPC_RMID, nullptr);
    if (res < 0) {
        log::error("shmctl error %d", errno);
    }

    return Group::on_shutdown(ctx);
}

zerocopy::Config * ZeroCopyGroup::get_config()
{
    if (state() == State::active)
        return &cfg;
    else
        return nullptr;
}

Result zc_init_gateway_from_group(context::Context& ctx,
                                  zerocopy::gateway::Gateway *gw,
                                  Connection *group)
{
    auto zc_group = dynamic_cast<multipoint::ZeroCopyGroup *>(group);
    if (!zc_group || !gw)
        return Result::error_bad_argument;

    auto ret = gw->init(ctx, zc_group->get_config());
    if (ret != zerocopy::gateway::Result::success)
        return Result::error_general_failure;

    return Result::success;
}

} // namespace mesh::multipoint
