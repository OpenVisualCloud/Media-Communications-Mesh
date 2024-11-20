#include <gtest/gtest.h>
#include "proxy_context.h"

// TODO: Create real tests
TEST(ProxyContextTests, ProxyContextConstructor) {
    ProxyContext ctx;
    EXPECT_EQ(ctx.getTCPListenPort(), 8002);
    EXPECT_EQ(ctx.getRPCListenAddress(), "0.0.0.0:8001");
    EXPECT_EQ(ctx.getTCPListenAddress(), "0.0.0.0:8002");
}
