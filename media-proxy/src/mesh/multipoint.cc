#include "multipoint.h"
#include "logger.h"

namespace mesh::multipoint {

using namespace mesh::connection;

Group::Group(const std::string& group_id) : Connection()
{
    _kind = Kind::transmitter;
    id = group_id;
}

Group::~Group()
{
}

void Group::configure(context::Context& ctx)
{
    set_state(ctx, State::configured);
}

Result Group::set_link(context::Context& ctx, Connection *new_link,
                       Connection *requester)
{
    if (!new_link && requester) {
        // Remove the requester as the group input
        if (requester == _link) {
            log::info("[GROUP] Remove input")("group_id", id)("id", requester->id);
            return Connection::set_link(ctx, nullptr);
        }

        // Remove the requester from the group outputs list
        for (auto it = outputs.begin(); it != outputs.end(); ++it) {
            if (*it != requester)
                continue;

            log::info("[GROUP] Delete output")("group_id", id)("id", requester->id);

            setting_link.store(true, std::memory_order_release);
            transmitting.wait(true, std::memory_order_acquire);

            outputs.erase(it);
                
            setting_link.store(false, std::memory_order_release);
            setting_link.notify_one();
            break;
        }
        return Result::success;
    }

    // log::info("[GROUP] Set link")("group_id", id)("new_link", new_link)
    //                             ("requester", requester);
    return Connection::set_link(ctx, new_link);
}

Result Group::assign_input(context::Context& ctx, Connection *input) {
    if (input->kind() != Kind::receiver)
        return Result::error_bad_argument;

    log::info("[GROUP] Assign input")("group_id", id)("id", input->id);

    return set_link(ctx, input);
}

Result Group::add_output(context::Context& ctx, Connection *output) {
    if (output->kind() != Kind::transmitter)
        return Result::error_bad_argument;

    log::info("[GROUP] Add output")("group_id", id)("id", output->id);

    setting_link.store(true, std::memory_order_release);
    transmitting.wait(true, std::memory_order_acquire);

    outputs.emplace_back(output);
        
    setting_link.store(false, std::memory_order_release);
    setting_link.notify_one();

    return Result::success;
}

Result Group::on_establish(context::Context& ctx)
{
    set_state(ctx, State::active);
    set_status(ctx, Status::healthy);

    return Result::success;
}

Result Group::on_receive(context::Context& ctx, void *ptr,
                         uint32_t sz, uint32_t& sent)
{
    if (state() != State::active)
        return set_result(Result::error_wrong_state);

    if (!_link)
        return set_result(Result::error_no_link_assigned);

    metrics.inbound_bytes += sz;

    auto res = Result::success;
    uint32_t total_sent = 0;
    uint32_t errors = 0;    

    if (outputs.empty())
        return Result::error_no_link_assigned;

    transmitting.store(true, std::memory_order_release);
    setting_link.wait(true, std::memory_order_acquire);

    for (Connection *output : outputs) {
        if (!output) {
            errors++;
            continue;
        }

        uint32_t out_sent = 0;
        res = output->do_receive(ctx, ptr, sz, out_sent);

        total_sent += out_sent;

        if (res != Result::success)
            errors++;
    }

    transmitting.store(false, std::memory_order_release);
    transmitting.notify_one();

    sent = sz;
    metrics.outbound_bytes += total_sent;
    metrics.errors += errors;

    if (!errors)
        metrics.transactions_succeeded++;
    else
        metrics.transactions_failed++;

    return Result::success;
}

Result Group::on_shutdown(context::Context& ctx)
{
    set_link(ctx, nullptr);

    outputs.clear();

    set_state(ctx, State::closed);
    set_status(ctx, Status::shutdown);

    return Result::success;
}

void Group::on_delete(context::Context& ctx)
{
}

} // namespace mesh::multipoint
