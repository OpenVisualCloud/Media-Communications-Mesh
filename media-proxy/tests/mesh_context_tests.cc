#include <gtest/gtest.h>
#include "concurrency.h"

using namespace mesh;

class test_class {
  public:
    context::Context _ctx;

    void init(context::Context &ctx) { _ctx = context::WithCancel(ctx); }
};

/**
 * Test designed to detect memory leaks in the Context class.
 *
 * Run the test with Valgrind to detect memory leaks.
 * valgrind --leak-check=full ./media_proxy_unit_tests --gtest_filter=*Context*
 */
TEST(MeshContext, constructor) {
    auto ctx = context::WithCancel(context::Background());
    test_class t;
    t.init(ctx);
    {
        auto ctx2 = context::WithCancel(ctx);
        t.init(ctx2);
        mesh::thread::Sleep(ctx2, std::chrono::milliseconds(10));
        ctx2.cancel();
    }
    {
        context::Context c1;
        c1 = context::Context();
    }
    {
        context::Context c1 = context::WithTimeout(ctx, std::chrono::milliseconds(10));
        context::Context c2 = context::WithTimeout(c1, std::chrono::milliseconds(20));
        context::Context c3 = context::WithTimeout(c2, std::chrono::milliseconds(30));
        context::Context c4 = context::WithTimeout(c3, std::chrono::milliseconds(40));
        context::Context c5 = context::WithTimeout(c4, std::chrono::milliseconds(50));
        context::Context c6 = context::WithTimeout(c5, std::chrono::milliseconds(60));
        mesh::thread::Sleep(c6, std::chrono::milliseconds(10));
    }
    {
        context::Context c1 = context::WithTimeout(ctx, std::chrono::milliseconds(60));
        context::Context c2 = context::WithTimeout(c1, std::chrono::milliseconds(50));
        context::Context c3 = context::WithTimeout(c2, std::chrono::milliseconds(40));
        context::Context c4 = context::WithTimeout(c3, std::chrono::milliseconds(30));
        context::Context c5 = context::WithTimeout(c4, std::chrono::milliseconds(20));
        context::Context c6 = context::WithTimeout(c5, std::chrono::milliseconds(10));
        mesh::thread::Sleep(c6, std::chrono::milliseconds(10));
    }
}
