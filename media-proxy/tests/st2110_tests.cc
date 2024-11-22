#include <gtest/gtest.h>
#include "mesh/st2110rx.h"
#include "mesh/st2110tx.h"
#include "mesh/conn.h"
using namespace mesh;

#define DUMMY_DATA1 "DUMMY_DATA1"
#define DUMMY_DATA2 "DUMMY_DATA2"

struct wrapper {
    static uint32_t received_packets_dummy1;
    static uint32_t received_packets_dummy2;

    static st_frame *get_frame_dummy(int *h)
    {
        st_frame *f = new st_frame;
        f->addr[0] = calloc(1000, 1);
        memcpy(f->addr[0], DUMMY_DATA1, sizeof(DUMMY_DATA1));
        return f;
    }

    static int put_frame_dummy(int *d, st_frame *f)
    {
        if (memcmp(f->addr[0], DUMMY_DATA1, sizeof(DUMMY_DATA1)) == 0) {
            received_packets_dummy1++;
        } else if (memcmp(f->addr[0], DUMMY_DATA2, sizeof(DUMMY_DATA2)) == 0) {
            received_packets_dummy2++;
        }

        free(f->addr[0]);
        delete f;
        return 0;
    }

    static int *create_dummy(mtl_handle, int *o) { return (int *)malloc(1); }

    static int close_dummy(int *h)
    {
        free(h);
        return 0;
    }
};
uint32_t wrapper::received_packets_dummy1;
uint32_t wrapper::received_packets_dummy2;

class EmulatedTransmitter : public connection::Connection {
  public:
    EmulatedTransmitter(context::Context &ctx)
    {
        _kind = connection::Kind::transmitter;
        set_state(ctx, connection::State::configured);
    }

    connection::Result on_establish(context::Context &ctx)
    {
        set_state(ctx, connection::State::active);
        return connection::Result::success;
    }

    connection::Result on_shutdown(context::Context &ctx) { return connection::Result::success; }

    connection::Result transmit_wrapper(context::Context &ctx, void *ptr, uint32_t sz)
    {
        return transmit(ctx, ptr, sz);
    }
};

class EmulatedReceiver : public connection::Connection {
  public:
    uint32_t received_packets_lossless;
    uint32_t received_packets_lossy;
    EmulatedReceiver(context::Context &ctx)
    {
        _kind = connection::Kind::receiver;
        received_packets_lossless = 0;
        received_packets_lossy = 0;
        set_state(ctx, connection::State::configured);
    }

    connection::Result on_establish(context::Context &ctx)
    {
        set_state(ctx, connection::State::active);
        return connection::Result::success;
    }

    connection::Result on_shutdown(context::Context &ctx) { return connection::Result::success; }

    connection::Result on_receive(context::Context &ctx, void *ptr, uint32_t sz, uint32_t &sent)
    {
        if (memcmp(ptr, DUMMY_DATA1, sizeof(DUMMY_DATA1)) == 0) {
            received_packets_lossless++;
        } else {
            received_packets_lossy++;
        }
        return connection::Result::success;
    }
};

class EmulatedST2110_Tx : public connection::ST2110Tx<st_frame, int *, int> {
  public:
    EmulatedST2110_Tx()
    {
        _get_frame_fn = wrapper::get_frame_dummy;
        _put_frame_fn = wrapper::put_frame_dummy;
        _create_session_fn = wrapper::create_dummy;
        _close_session_fn = wrapper::close_dummy;
        _transfer_size = 10000;
    };
    ~EmulatedST2110_Tx(){};

    connection::Result configure(context::Context &ctx)
    {
        set_state(ctx, connection::State::configured);
        return connection::Result::success;
    }
};

class EmulatedST2110_Rx : public connection::ST2110Rx<st_frame, int *, int> {
  public:
    EmulatedST2110_Rx()
    {
        _get_frame_fn = wrapper::get_frame_dummy;
        _put_frame_fn = wrapper::put_frame_dummy;
        _create_session_fn = wrapper::create_dummy;
        _close_session_fn = wrapper::close_dummy;
        _transfer_size = 10000;
    };
    ~EmulatedST2110_Rx(){};

    connection::Result configure(context::Context &ctx)
    {
        set_state(ctx, connection::State::configured);
        return connection::Result::success;
    }
};

static void validate_state_change(context::Context &ctx, connection::Connection *c)
{
    connection::Result res;

    // Change state Configured -> Active
    res = c->establish(ctx);
    ASSERT_EQ(res, connection::Result::success) << mesh::connection::result2str(res);
    ASSERT_EQ(c->state(), connection::State::active);

    // Change state Active -> Suspended
    res = c->suspend(ctx);
    ASSERT_EQ(res, connection::Result::success) << mesh::connection::result2str(res);
    ASSERT_EQ(c->state(), connection::State::suspended);

    // Change state Suspended -> Active
    res = c->resume(ctx);
    ASSERT_EQ(res, connection::Result::success) << mesh::connection::result2str(res);
    ASSERT_EQ(c->state(), connection::State::active);

    // Change state Active -> Closed
    res = c->shutdown(ctx);
    ASSERT_EQ(res, connection::Result::success) << mesh::connection::result2str(res);
    ASSERT_EQ(c->state(), connection::State::closed);
}

static void get_ST2110_session_config(MeshConfig_ST2110 &cfg_st2110, int transport)
{
    memcpy(cfg_st2110.local_ip_addr, "127.0.0.1", sizeof("127.0.0.1"));
    memcpy(cfg_st2110.remote_ip_addr, "127.0.0.1", sizeof("127.0.0.1"));
    cfg_st2110.local_port = 9001;
    cfg_st2110.remote_port = 9001;
    cfg_st2110.transport = transport;
}

static void get_ST2110_video_cfg(MeshConfig_Video &cfg_video)
{
    cfg_video.fps = 30;
    cfg_video.width = 1920;
    cfg_video.height = 1080;
    cfg_video.pixel_format = MESH_VIDEO_PIXEL_FORMAT_YUV422P10LE;
}

static void get_ST2110_audio_cfg(MeshConfig_Audio &cfg_audio)
{
    cfg_audio.channels = 2;
    cfg_audio.format = MESH_AUDIO_FORMAT_PCM_S16BE;
    cfg_audio.packet_time = MESH_AUDIO_PACKET_TIME_1MS;
    cfg_audio.sample_rate = MESH_AUDIO_SAMPLE_RATE_48000;
}

TEST(st2110_tx, state_change)
{
    auto ctx = context::WithCancel(mesh::context::Background());

    auto conn_tx = new EmulatedST2110_Tx;
    ASSERT_EQ(conn_tx->kind(), connection::Kind::transmitter);
    ASSERT_EQ(conn_tx->state(), connection::State::not_configured);

    // Change state: Not Configured -> Configured
    connection::Result res = conn_tx->configure(ctx);
    ASSERT_EQ(res, connection::Result::success) << mesh::connection::result2str(res);
    ASSERT_EQ(conn_tx->state(), connection::State::configured);

    validate_state_change(ctx, dynamic_cast<connection::Connection *>(conn_tx));

    delete conn_tx;
}

TEST(st2110_rx, state_change)
{
    auto ctx = context::WithCancel(mesh::context::Background());

    auto conn_rx = new EmulatedST2110_Rx;
    ASSERT_EQ(conn_rx->kind(), connection::Kind::receiver);
    ASSERT_EQ(conn_rx->state(), connection::State::not_configured);

    // Change state: Not Configured -> Configured
    connection::Result res = conn_rx->configure(ctx);
    ASSERT_EQ(res, connection::Result::success) << mesh::connection::result2str(res);
    ASSERT_EQ(conn_rx->state(), connection::State::configured);

    validate_state_change(ctx, dynamic_cast<connection::Connection *>(conn_rx));

    delete conn_rx;
}

TEST(DISABLED_st2110_20tx, state_change)
{
    auto ctx = context::WithCancel(mesh::context::Background());

    auto conn_rx = new connection::ST2110_20Tx;
    ASSERT_EQ(conn_rx->kind(), connection::Kind::transmitter);
    ASSERT_EQ(conn_rx->state(), connection::State::not_configured);

    MeshConfig_ST2110 cfg_st2110;
    get_ST2110_session_config(cfg_st2110, MESH_CONN_TRANSPORT_ST2110_20);

    MeshConfig_Video cfg_video;
    get_ST2110_video_cfg(cfg_video);

    // Change state: Not Configured -> Configured
    std::string dev_port("kernel:lo");
    connection::Result res = conn_rx->configure(ctx, dev_port, cfg_st2110, cfg_video);
    ASSERT_EQ(res, connection::Result::success) << mesh::connection::result2str(res);
    ASSERT_EQ(conn_rx->state(), connection::State::configured);

    validate_state_change(ctx, dynamic_cast<connection::Connection *>(conn_rx));
    validate_state_change(ctx, dynamic_cast<connection::Connection *>(conn_rx));

    delete conn_rx;
}

TEST(DISABLED_st2110_22tx, state_change)
{
    auto ctx = context::WithCancel(mesh::context::Background());

    auto conn_rx = new connection::ST2110_22Tx;
    ASSERT_EQ(conn_rx->kind(), connection::Kind::transmitter);
    ASSERT_EQ(conn_rx->state(), connection::State::not_configured);

    MeshConfig_ST2110 cfg_st2110;
    get_ST2110_session_config(cfg_st2110, MESH_CONN_TRANSPORT_ST2110_22);

    MeshConfig_Video cfg_video;
    get_ST2110_video_cfg(cfg_video);

    // Change state: Not Configured -> Configured
    std::string dev_port("kernel:lo");
    connection::Result res = conn_rx->configure(ctx, dev_port, cfg_st2110, cfg_video);
    ASSERT_EQ(res, connection::Result::success) << mesh::connection::result2str(res);
    ASSERT_EQ(conn_rx->state(), connection::State::configured);

    validate_state_change(ctx, dynamic_cast<connection::Connection *>(conn_rx));
    validate_state_change(ctx, dynamic_cast<connection::Connection *>(conn_rx));

    delete conn_rx;
}

TEST(DISABLED_st2110_30tx, state_change)
{
    auto ctx = context::WithCancel(mesh::context::Background());

    auto conn_rx = new connection::ST2110_30Tx;
    ASSERT_EQ(conn_rx->kind(), connection::Kind::transmitter);
    ASSERT_EQ(conn_rx->state(), connection::State::not_configured);

    MeshConfig_ST2110 cfg_st2110;
    get_ST2110_session_config(cfg_st2110, MESH_CONN_TRANSPORT_ST2110_30);

    MeshConfig_Audio cfg_audio;
    get_ST2110_audio_cfg(cfg_audio);

    // Change state: Not Configured -> Configured
    std::string dev_port("kernel:lo");
    connection::Result res = conn_rx->configure(ctx, dev_port, cfg_st2110, cfg_audio);
    ASSERT_EQ(res, connection::Result::success) << mesh::connection::result2str(res);
    ASSERT_EQ(conn_rx->state(), connection::State::configured);

    validate_state_change(ctx, dynamic_cast<connection::Connection *>(conn_rx));

    // Change state: Closed -> Configured
    res = conn_rx->configure(ctx, dev_port, cfg_st2110, cfg_audio);
    ASSERT_EQ(res, connection::Result::success) << mesh::connection::result2str(res);
    ASSERT_EQ(conn_rx->state(), connection::State::configured);
    validate_state_change(ctx, dynamic_cast<connection::Connection *>(conn_rx));

    delete conn_rx;
}

TEST(st2110_tx, send_data)
{
    auto ctx = context::WithCancel(mesh::context::Background());
    connection::Result res;

    auto conn_tx = new EmulatedST2110_Tx;
    auto emulated_tx = new EmulatedTransmitter(ctx);

    // Setup Tx connection
    res = conn_tx->configure(ctx);
    ASSERT_EQ(res, connection::Result::success) << mesh::connection::result2str(res);
    ASSERT_EQ(conn_tx->state(), connection::State::configured);

    // Change state Configured -> Active
    res = conn_tx->establish(ctx);
    ASSERT_EQ(res, connection::Result::success) << mesh::connection::result2str(res);
    ASSERT_EQ(conn_tx->state(), connection::State::active);

    // Setup Emulated Transmitter
    emulated_tx->establish(ctx);

    // Connect Emulated Transmitter to Tx connection
    emulated_tx->set_link(ctx, conn_tx);

    for (int i = 0; i < 5; i++) {
        res = emulated_tx->transmit_wrapper(ctx, (void *)DUMMY_DATA2, sizeof(DUMMY_DATA2));
        ASSERT_EQ(res, connection::Result::success) << mesh::connection::result2str(res);
        ASSERT_EQ(conn_tx->state(), connection::State::active);
        ASSERT_GT(wrapper::received_packets_dummy2, 0);
    }

    // Shutdown Tx connection
    res = conn_tx->shutdown(ctx);
    ASSERT_EQ(res, connection::Result::success) << mesh::connection::result2str(res);
    ASSERT_EQ(conn_tx->state(), connection::State::closed);

    // Destroy resources
    delete emulated_tx;
    delete conn_tx;
}

TEST(st2110_rx, get_data)
{
    auto ctx = context::WithCancel(mesh::context::Background());
    connection::Result res;

    // Setup Emulated Receiver
    auto emulated_rx = new EmulatedReceiver(ctx);
    emulated_rx->establish(ctx);

    // Setup Rx connection
    auto conn_rx = new EmulatedST2110_Rx;

    res = conn_rx->configure(ctx);
    ASSERT_EQ(res, connection::Result::success) << mesh::connection::result2str(res);
    ASSERT_EQ(conn_rx->state(), connection::State::configured);
    res = conn_rx->establish(ctx);
    ASSERT_EQ(res, connection::Result::success) << mesh::connection::result2str(res);
    ASSERT_EQ(conn_rx->state(), connection::State::active);

    // Connect Rx connection to Emulated Receiver
    conn_rx->set_link(ctx, emulated_rx);

    // Sleep some sufficient time to allow receiving the data from transmitter
    mesh::thread::Sleep(ctx, std::chrono::milliseconds(100));

    // Shutdown Rx connection
    res = conn_rx->shutdown(ctx);
    ASSERT_EQ(res, connection::Result::success) << mesh::connection::result2str(res);
    ASSERT_EQ(conn_rx->state(), connection::State::closed);

    ASSERT_GT(wrapper::received_packets_dummy1, 0);
    // Destroy resources
    delete conn_rx;
    delete emulated_rx;
}

/************************** */
static void tx_thread(context::Context &ctx, connection::Connection *conn_tx)
{
    auto emulated_tx = new EmulatedTransmitter(ctx);
    // Change state Configured -> Active
    connection::Result res = conn_tx->establish(ctx);
    ASSERT_EQ(res, connection::Result::success) << mesh::connection::result2str(res);
    ASSERT_EQ(conn_tx->state(), connection::State::active);

    // Setup Emulated Transmitter
    emulated_tx->establish(ctx);

    // Connect Emulated Transmitter to Tx connection
    emulated_tx->set_link(ctx, conn_tx);

    for (int i = 0; i < 50; i++) {
        res = emulated_tx->transmit_wrapper(ctx, (void *)DUMMY_DATA1, sizeof(DUMMY_DATA1));
        ASSERT_EQ(res, connection::Result::success) << mesh::connection::result2str(res);
        ASSERT_EQ(conn_tx->state(), connection::State::active);
    }

    // Shutdown Tx connection
    res = conn_tx->shutdown(ctx);
    ASSERT_EQ(res, connection::Result::success) << mesh::connection::result2str(res);
    ASSERT_EQ(conn_tx->state(), connection::State::closed);

    // Destroy resources
    delete emulated_tx;
}

static void rx_thread(context::Context &ctx, connection::Connection *conn_rx, bool is_lossless)
{
    auto emulated_rx = new EmulatedReceiver(ctx);
    emulated_rx->establish(ctx);

    conn_rx->set_link(ctx, emulated_rx);
    // Connect Rx connection to Emulated Receiver
    connection::Result res = conn_rx->establish(ctx);
    ASSERT_EQ(res, connection::Result::success) << mesh::connection::result2str(res);
    ASSERT_EQ(conn_rx->state(), connection::State::active);

    // Sleep some sufficient time to allow receiving the data from transmitter
    mesh::thread::Sleep(ctx, std::chrono::milliseconds(500));

    // Shutdown Rx connection
    res = conn_rx->shutdown(ctx);
    ASSERT_EQ(res, connection::Result::success) << mesh::connection::result2str(res);
    ASSERT_EQ(conn_rx->state(), connection::State::closed);

    if (is_lossless) {
        ASSERT_GT(emulated_rx->received_packets_lossless, 0);
        ASSERT_EQ(emulated_rx->received_packets_lossy, 0);
    } else {
        ASSERT_GT(emulated_rx->received_packets_lossy, 0);
        ASSERT_EQ(emulated_rx->received_packets_lossless, 0);
    }

    delete emulated_rx;
}

TEST(DISABLED_st2110_20, send_and_receive_data)
{
    auto ctx = context::WithCancel(mesh::context::Background());
    connection::Result res;

    // Setup Tx connection
    MeshConfig_ST2110 cfg_st2110;
    get_ST2110_session_config(cfg_st2110, MESH_CONN_TRANSPORT_ST2110_20);

    MeshConfig_Video cfg_video;
    get_ST2110_video_cfg(cfg_video);

    // Configure Tx
    std::string dev_port("kernel:lo");
    auto conn_tx = new connection::ST2110_20Tx;
    res = conn_tx->configure(ctx, dev_port, cfg_st2110, cfg_video);
    ASSERT_EQ(res, connection::Result::success) << mesh::connection::result2str(res);
    ASSERT_EQ(conn_tx->state(), connection::State::configured);

    // Configure Rx
    auto conn_rx = new connection::ST2110_20Rx;
    res = conn_rx->configure(ctx, dev_port, cfg_st2110, cfg_video);
    ASSERT_EQ(res, connection::Result::success) << mesh::connection::result2str(res);
    ASSERT_EQ(conn_rx->state(), connection::State::configured);

    std::jthread rx_th, tx_th;
    try {
        rx_th = std::jthread(&rx_thread, std::ref(ctx), conn_rx, 1);
        tx_th = std::jthread(&tx_thread, std::ref(ctx), conn_tx);
    } catch (const std::system_error &e) {
        ASSERT_TRUE(0) << e.what();
    }

    rx_th.join();
    tx_th.join();

    delete conn_tx;
    delete conn_rx;
}

TEST(DISABLED_st2110_22, send_and_receive_data)
{
    auto ctx = context::WithCancel(mesh::context::Background());
    connection::Result res;

    // Setup Tx connection
    MeshConfig_ST2110 cfg_st2110;
    get_ST2110_session_config(cfg_st2110, MESH_CONN_TRANSPORT_ST2110_22);

    MeshConfig_Video cfg_video;
    get_ST2110_video_cfg(cfg_video);

    // Configure Tx
    std::string dev_port("kernel:lo");
    auto conn_tx = new connection::ST2110_22Tx;
    res = conn_tx->configure(ctx, dev_port, cfg_st2110, cfg_video);
    ASSERT_EQ(res, connection::Result::success) << mesh::connection::result2str(res);
    ASSERT_EQ(conn_tx->state(), connection::State::configured);

    // Configure Rx
    auto conn_rx = new connection::ST2110_22Rx;
    res = conn_rx->configure(ctx, dev_port, cfg_st2110, cfg_video);
    ASSERT_EQ(res, connection::Result::success) << mesh::connection::result2str(res);
    ASSERT_EQ(conn_rx->state(), connection::State::configured);

    std::jthread rx_th, tx_th;
    try {
        rx_th = std::jthread(&rx_thread, std::ref(ctx), conn_rx, 0);
        tx_th = std::jthread(&tx_thread, std::ref(ctx), conn_tx);
    } catch (const std::system_error &e) {
        ASSERT_TRUE(0) << e.what();
    }

    rx_th.join();
    tx_th.join();

    delete conn_tx;
    delete conn_rx;
}

TEST(DISABLED_st2110_30, send_and_receive_data)
{
    auto ctx = context::WithCancel(mesh::context::Background());
    connection::Result res;

    // Setup Tx connection
    MeshConfig_ST2110 cfg_st2110;
    get_ST2110_session_config(cfg_st2110, MESH_CONN_TRANSPORT_ST2110_30);

    MeshConfig_Audio cfg_audio;
    get_ST2110_audio_cfg(cfg_audio);

    // Configure Tx
    std::string dev_port("kernel:lo");
    auto conn_tx = new connection::ST2110_30Tx;
    res = conn_tx->configure(ctx, dev_port, cfg_st2110, cfg_audio);
    ASSERT_EQ(res, connection::Result::success) << mesh::connection::result2str(res);
    ASSERT_EQ(conn_tx->state(), connection::State::configured);

    // Configure Rx
    auto conn_rx = new connection::ST2110_30Rx;
    res = conn_rx->configure(ctx, dev_port, cfg_st2110, cfg_audio);
    ASSERT_EQ(res, connection::Result::success) << mesh::connection::result2str(res);
    ASSERT_EQ(conn_rx->state(), connection::State::configured);

    std::jthread rx_th, tx_th;
    try {
        rx_th = std::jthread(&rx_thread, std::ref(ctx), conn_rx, 1);
        tx_th = std::jthread(&tx_thread, std::ref(ctx), conn_tx);
    } catch (const std::system_error &e) {
        ASSERT_TRUE(0) << e.what();
    }

    rx_th.join();
    tx_th.join();

    delete conn_tx;
    delete conn_rx;
}
