#ifndef MULTIPOINT_H
#define MULTIPOINT_H

#include "conn.h"
#include <list>

namespace mesh::multipoint {

using namespace mesh::connection;

class Group : public Connection {

public:
    Group(const std::string& group_id);
    ~Group() override;

    void configure(context::Context& ctx);

    Result set_link(context::Context& ctx, Connection *new_link,
                    Connection *requester = nullptr) override;

    Result assign_input(context::Context& ctx, Connection *input);
    Result add_output(context::Context& ctx, Connection *output);

    void delete_all_outputs() {
        outputs.clear();
    }

    int outputs_num() {
        return outputs.size();
    }

private:
    Result on_establish(context::Context& ctx) override;
    Result on_receive(context::Context& ctx, void *ptr, uint32_t sz,
                      uint32_t& sent) override;
    Result on_shutdown(context::Context& ctx) override;
    void on_delete(context::Context& ctx) override;

private:
    std::list<Connection *> outputs;
    std::mutex outputs_mx;

    sync::DataplaneAtomicUint64 outputs_ptr;

    std::list<Connection *> * get_hotpath_outputs();
    void set_hotpath_outputs(const std::list<Connection *> *new_outputs);
};

} // namespace mesh::multipoint

#endif // MULTIPOINT_H
