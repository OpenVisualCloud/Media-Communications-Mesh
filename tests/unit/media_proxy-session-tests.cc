#include <gtest/gtest.h>
#include <arpa/inet.h>

#include "session-mtl.h"
#include "session-rdma.h"

#define PCI_ADDR "0000:4b:01.0"
#define NET_ADDR "192.168.96.1"

template <class CLASS_NAME, typename DEV_HANDLE, typename OPTS> session *test_null()
{
    session *s = nullptr;

    int dummy = 1000;
    DEV_HANDLE *d = (DEV_HANDLE *)&dummy;

    OPTS o{0};
    memif_ops_t m{0};

    try {
        s = new CLASS_NAME(d, &o, nullptr);
    } catch (const exception &e) {
    }
    EXPECT_TRUE(s == nullptr);

    try {
        s = new CLASS_NAME(nullptr, &o, &m);
    } catch (const exception &e) {
    }
    EXPECT_TRUE(s == nullptr);

    try {
        s = new CLASS_NAME(d, nullptr, &m);
    } catch (const exception &e) {
    }
    EXPECT_TRUE(s == nullptr);

    return s;
}

TEST(media_proxy, session_rx_st20p_constructor_null)
{
    session *s = test_null<rx_st20_mtl_session, mtl_main_impl, st20p_rx_ops>();
    EXPECT_TRUE(s == nullptr);
}

TEST(media_proxy, session_tx_st20p_constructor_null)
{
    session *s = test_null<tx_st20_mtl_session, mtl_main_impl, st20p_tx_ops>();
    EXPECT_TRUE(s == nullptr);
}

TEST(media_proxy, session_rx_st22p_constructor_null)
{
    session *s = test_null<rx_st22_mtl_session, mtl_main_impl, st22p_rx_ops>();
    EXPECT_TRUE(s == nullptr);
}

TEST(media_proxy, session_tx_st22p_constructor_null)
{
    session *s = test_null<tx_st22_mtl_session, mtl_main_impl, st22p_tx_ops>();
    EXPECT_TRUE(s == nullptr);
}

TEST(media_proxy, session_rx_st30_constructor_null)
{
    session *s = test_null<rx_st30_mtl_session, mtl_main_impl, st30_rx_ops>();
    EXPECT_TRUE(s == nullptr);
}

TEST(media_proxy, session_tx_st30_constructor_null)
{
    session *s = test_null<tx_st30_mtl_session, mtl_main_impl, st30_tx_ops>();
    EXPECT_TRUE(s == nullptr);
}

TEST(media_proxy, session_tx_rdma_constructor_null) {
    session *s = test_null<tx_rdma_session, libfabric_ctx, rdma_s_ops_t>();
    EXPECT_TRUE(s == nullptr);
}

TEST(media_proxy, session_rx_rdma_constructor_null) {
    session *s = test_null<rx_rdma_session, libfabric_ctx, rdma_s_ops_t>();
    EXPECT_TRUE(s == nullptr);
}
