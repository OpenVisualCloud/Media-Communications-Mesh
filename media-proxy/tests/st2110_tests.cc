#include <gtest/gtest.h>
#include "mesh/st2110rx.h"
#include "mesh/st2110tx.h"
#include "mesh/conn.h"
using namespace mesh;

#define DUMMY_DATA1 "DUMMY_DATA1"
#define DUMMY_DATA2 "DUMMY_DATA2"

class EmulatedTransmitter : public connection::Connection {
  public:
    EmulatedTransmitter(context::Context& ctx) {
        _kind = connection::Kind::transmitter;
        set_state(ctx, connection::State::configured);
    }

    connection::Result on_establish(context::Context& ctx) {
        set_state(ctx, connection::State::active);
        return connection::Result::success;
    }

    connection::Result on_shutdown(context::Context& ctx) { return connection::Result::success; }

    connection::Result transmit_wrapper(context::Context& ctx, void *ptr, uint32_t sz) {
        return transmit(ctx, ptr, sz);
    }
};

class EmulatedReceiver : public connection::Connection {
  public:
    uint32_t received_packets_lossless;
    uint32_t received_packets_lossy;
    EmulatedReceiver(context::Context& ctx) {
        _kind = connection::Kind::receiver;
        received_packets_lossless = 0;
        received_packets_lossy = 0;
        set_state(ctx, connection::State::configured);
    }

    connection::Result on_establish(context::Context& ctx) {
        set_state(ctx, connection::State::active);
        return connection::Result::success;
    }

    connection::Result on_shutdown(context::Context& ctx) { return connection::Result::success; }

    connection::Result on_receive(context::Context& ctx, void *ptr, uint32_t sz, uint32_t& sent) {
        if (memcmp(ptr, DUMMY_DATA1, sizeof(DUMMY_DATA1)) == 0) {
            received_packets_lossless++;
        } else {
            received_packets_lossy++;
        }
        return connection::Result::success;
    }
};

class EmulatedST2110_Tx : public connection::ST2110Tx<st_frame, int *, st20p_tx_ops> {
  public:
    uint32_t received_packets_dummy1;
    uint32_t received_packets_dummy2;

    EmulatedST2110_Tx() {
        received_packets_dummy1 = 0;
        received_packets_dummy2 = 0;
        transfer_size = 10000;
    };
    ~EmulatedST2110_Tx() {};

    connection::Result configure(context::Context& ctx) {
        set_state(ctx, connection::State::configured);
        return connection::Result::success;
    }

    st_frame *get_frame(int *h) override {
        st_frame *f = new st_frame;
        f->addr[0] = calloc(1000, 1);
        memcpy(f->addr[0], DUMMY_DATA1, sizeof(DUMMY_DATA1));
        return f;
    }

    int put_frame(int *d, st_frame *f) override {
        if (memcmp(f->addr[0], DUMMY_DATA1, sizeof(DUMMY_DATA1)) == 0) {
            received_packets_dummy1++;
        } else if (memcmp(f->addr[0], DUMMY_DATA2, sizeof(DUMMY_DATA2)) == 0) {
            received_packets_dummy2++;
        }

        free(f->addr[0]);
        delete f;
        return 0;
    }

    int *create_session(mtl_handle, st20p_tx_ops *o) override { return (int *)malloc(1); }

    int close_session(int *h) override {
        free(h);
        return 0;
    }

    mtl_handle get_mtl_dev_wrapper(const std::string& dev_port, mtl_log_level log_level,
                                   const std::string& ip_addr) override {
        // For unit test purpose, return this object as mtl_handle, this will not be accessed except
        // checking for nullptr
        return (mtl_handle)(this);
    }
};

class EmulatedST2110_Rx : public connection::ST2110Rx<st_frame, int *, st20p_rx_ops> {
  public:
    uint32_t received_packets_dummy1;
    uint32_t received_packets_dummy2;
    EmulatedST2110_Rx() {
        received_packets_dummy1 = 0;
        received_packets_dummy2 = 0;
        transfer_size = 10000;
    };
    ~EmulatedST2110_Rx() {};

    connection::Result configure(context::Context& ctx) {
        set_state(ctx, connection::State::configured);
        return connection::Result::success;
    }

    st_frame *get_frame(int *h) override {
        st_frame *f = new st_frame;
        f->addr[0] = calloc(1000, 1);
        memcpy(f->addr[0], DUMMY_DATA1, sizeof(DUMMY_DATA1));
        return f;
    }

    int put_frame(int *d, st_frame *f) override {
        if (memcmp(f->addr[0], DUMMY_DATA1, sizeof(DUMMY_DATA1)) == 0) {
            received_packets_dummy1++;
        } else if (memcmp(f->addr[0], DUMMY_DATA2, sizeof(DUMMY_DATA2)) == 0) {
            received_packets_dummy2++;
        }

        free(f->addr[0]);
        delete f;
        return 0;
    }

    int *create_session(mtl_handle, st20p_rx_ops *o) override { return (int *)malloc(1); }

    int close_session(int *h) override {
        free(h);
        return 0;
    }

    mtl_handle get_mtl_dev_wrapper(const std::string& dev_port, mtl_log_level log_level,
                                   const std::string& ip_addr) override {
        // For unit test purpose, return this object as mtl_handle, this will not be accessed except
        // checking for nullptr
        return (mtl_handle)(this);
    }
};

static void validate_state_change(context::Context& ctx, connection::Connection *c) {
    connection::Result res;

    // Change state Configured -> Active
    res = c->establish(ctx);
    ASSERT_EQ(res, connection::Result::success) << connection::result2str(res);
    ASSERT_EQ(c->state(), connection::State::active);

    // Change state Active -> Suspended
    res = c->suspend(ctx);
    ASSERT_EQ(res, connection::Result::success) << connection::result2str(res);
    ASSERT_EQ(c->state(), connection::State::suspended);

    // Change state Suspended -> Active
    res = c->resume(ctx);
    ASSERT_EQ(res, connection::Result::success) << connection::result2str(res);
    ASSERT_EQ(c->state(), connection::State::active);

    // Change state Active -> Closed
    res = c->shutdown(ctx);
    ASSERT_EQ(res, connection::Result::success) << connection::result2str(res);
    ASSERT_EQ(c->state(), connection::State::closed);
}

static void get_ST2110_session_config(MeshConfig_ST2110& cfg_st2110, int transport) {
    memcpy(cfg_st2110.local_ip_addr, "127.0.0.1", sizeof("127.0.0.1"));
    memcpy(cfg_st2110.remote_ip_addr, "127.0.0.1", sizeof("127.0.0.1"));
    cfg_st2110.local_port = 9001;
    cfg_st2110.remote_port = 9001;
    cfg_st2110.transport = transport;
}

static void get_ST2110_video_cfg(MeshConfig_Video& cfg_video) {
    cfg_video.fps = 30;
    cfg_video.width = 1920;
    cfg_video.height = 1080;
    cfg_video.pixel_format = MESH_VIDEO_PIXEL_FORMAT_YUV422PLANAR10LE;
}

static void get_ST2110_audio_cfg(MeshConfig_Audio& cfg_audio) {
    cfg_audio.channels = 2;
    cfg_audio.format = MESH_AUDIO_FORMAT_PCM_S16BE;
    cfg_audio.packet_time = MESH_AUDIO_PACKET_TIME_1MS;
    cfg_audio.sample_rate = MESH_AUDIO_SAMPLE_RATE_48000;
}

TEST(st2110_tx, state_change) {
    auto ctx = context::WithCancel(context::Background());

    auto conn_tx = new EmulatedST2110_Tx;
    ASSERT_EQ(conn_tx->kind(), connection::Kind::transmitter);
    ASSERT_EQ(conn_tx->state(), connection::State::not_configured);

    // Change state: Not Configured -> Configured
    connection::Result res = conn_tx->configure(ctx);
    ASSERT_EQ(res, connection::Result::success) << connection::result2str(res);
    ASSERT_EQ(conn_tx->state(), connection::State::configured);

    validate_state_change(ctx, dynamic_cast<connection::Connection *>(conn_tx));

    delete conn_tx;
}

TEST(st2110_rx, state_change) {
    auto ctx = context::WithCancel(context::Background());

    auto conn_rx = new EmulatedST2110_Rx;
    ASSERT_EQ(conn_rx->kind(), connection::Kind::receiver);
    ASSERT_EQ(conn_rx->state(), connection::State::not_configured);

    // Change state: Not Configured -> Configured
    connection::Result res = conn_rx->configure(ctx);
    ASSERT_EQ(res, connection::Result::success) << connection::result2str(res);
    ASSERT_EQ(conn_rx->state(), connection::State::configured);

    validate_state_change(ctx, dynamic_cast<connection::Connection *>(conn_rx));

    delete conn_rx;
}

TEST(DISABLED_st2110_20tx, state_change) {
    auto ctx = context::WithCancel(context::Background());

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
    ASSERT_EQ(res, connection::Result::success) << connection::result2str(res);
    ASSERT_EQ(conn_rx->state(), connection::State::configured);

    validate_state_change(ctx, dynamic_cast<connection::Connection *>(conn_rx));
    validate_state_change(ctx, dynamic_cast<connection::Connection *>(conn_rx));

    delete conn_rx;
}

TEST(DISABLED_st2110_22tx, state_change) {
    auto ctx = context::WithCancel(context::Background());

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
    ASSERT_EQ(res, connection::Result::success) << connection::result2str(res);
    ASSERT_EQ(conn_rx->state(), connection::State::configured);

    validate_state_change(ctx, dynamic_cast<connection::Connection *>(conn_rx));
    validate_state_change(ctx, dynamic_cast<connection::Connection *>(conn_rx));

    delete conn_rx;
}

TEST(DISABLED_st2110_30tx, state_change) {
    auto ctx = context::WithCancel(context::Background());

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
    ASSERT_EQ(res, connection::Result::success) << connection::result2str(res);
    ASSERT_EQ(conn_rx->state(), connection::State::configured);

    validate_state_change(ctx, dynamic_cast<connection::Connection *>(conn_rx));

    // Change state: Closed -> Configured
    res = conn_rx->configure(ctx, dev_port, cfg_st2110, cfg_audio);
    ASSERT_EQ(res, connection::Result::success) << connection::result2str(res);
    ASSERT_EQ(conn_rx->state(), connection::State::configured);
    validate_state_change(ctx, dynamic_cast<connection::Connection *>(conn_rx));

    delete conn_rx;
}

TEST(st2110_tx, send_data) {
    auto ctx = context::WithCancel(context::Background());
    connection::Result res;

    auto conn_tx = new EmulatedST2110_Tx;
    auto emulated_tx = new EmulatedTransmitter(ctx);

    // Setup Tx connection
    res = conn_tx->configure(ctx);
    ASSERT_EQ(res, connection::Result::success) << connection::result2str(res);
    ASSERT_EQ(conn_tx->state(), connection::State::configured);

    // Change state Configured -> Active
    res = conn_tx->establish(ctx);
    ASSERT_EQ(res, connection::Result::success) << connection::result2str(res);
    ASSERT_EQ(conn_tx->state(), connection::State::active);

    // Setup Emulated Transmitter
    emulated_tx->establish(ctx);

    // Connect Emulated Transmitter to Tx connection
    emulated_tx->set_link(ctx, conn_tx);

    for (int i = 0; i < 5; i++) {
        res = emulated_tx->transmit_wrapper(ctx, (void *)DUMMY_DATA2, sizeof(DUMMY_DATA2));
        ASSERT_EQ(res, connection::Result::success) << connection::result2str(res);
        ASSERT_EQ(conn_tx->state(), connection::State::active);
        ASSERT_GT(conn_tx->received_packets_dummy2, 0);
    }

    // Shutdown Tx connection
    res = conn_tx->shutdown(ctx);
    ASSERT_EQ(res, connection::Result::success) << connection::result2str(res);
    ASSERT_EQ(conn_tx->state(), connection::State::closed);

    // Destroy resources
    delete emulated_tx;
    delete conn_tx;
}

TEST(st2110_rx, get_data) {
    auto ctx = context::WithCancel(context::Background());
    connection::Result res;

    // Setup Emulated Receiver
    auto emulated_rx = new EmulatedReceiver(ctx);
    emulated_rx->establish(ctx);

    // Setup Rx connection
    auto conn_rx = new EmulatedST2110_Rx;

    res = conn_rx->configure(ctx);
    ASSERT_EQ(res, connection::Result::success) << connection::result2str(res);
    ASSERT_EQ(conn_rx->state(), connection::State::configured);
    res = conn_rx->establish(ctx);
    ASSERT_EQ(res, connection::Result::success) << connection::result2str(res);
    ASSERT_EQ(conn_rx->state(), connection::State::active);

    // Connect Rx connection to Emulated Receiver
    conn_rx->set_link(ctx, emulated_rx);

    // Sleep some sufficient time to allow receiving the data from transmitter
    mesh::thread::Sleep(ctx, std::chrono::milliseconds(100));

    // Shutdown Rx connection
    res = conn_rx->shutdown(ctx);
    ASSERT_EQ(res, connection::Result::success) << connection::result2str(res);
    ASSERT_EQ(conn_rx->state(), connection::State::closed);

    ASSERT_GT(conn_rx->received_packets_dummy1, 0);
    // Destroy resources
    delete conn_rx;
    delete emulated_rx;
}

/************************** */
static void tx_thread(context::Context& ctx, connection::Connection *conn_tx) {
    auto emulated_tx = new EmulatedTransmitter(ctx);
    // Change state Configured -> Active
    connection::Result res = conn_tx->establish(ctx);
    ASSERT_EQ(res, connection::Result::success) << connection::result2str(res);
    ASSERT_EQ(conn_tx->state(), connection::State::active);

    // Setup Emulated Transmitter
    emulated_tx->establish(ctx);

    // Connect Emulated Transmitter to Tx connection
    emulated_tx->set_link(ctx, conn_tx);

    for (int i = 0; i < 50; i++) {
        res = emulated_tx->transmit_wrapper(ctx, (void *)DUMMY_DATA1, sizeof(DUMMY_DATA1));
        ASSERT_EQ(res, connection::Result::success) << connection::result2str(res);
        ASSERT_EQ(conn_tx->state(), connection::State::active);
    }

    // Shutdown Tx connection
    res = conn_tx->shutdown(ctx);
    ASSERT_EQ(res, connection::Result::success) << connection::result2str(res);
    ASSERT_EQ(conn_tx->state(), connection::State::closed);

    // Destroy resources
    delete emulated_tx;
}

static void rx_thread(context::Context& ctx, connection::Connection *conn_rx, bool is_lossless) {
    auto emulated_rx = new EmulatedReceiver(ctx);
    emulated_rx->establish(ctx);

    conn_rx->set_link(ctx, emulated_rx);
    // Connect Rx connection to Emulated Receiver
    connection::Result res = conn_rx->establish(ctx);
    ASSERT_EQ(res, connection::Result::success) << connection::result2str(res);
    ASSERT_EQ(conn_rx->state(), connection::State::active);

    // Sleep some sufficient time to allow receiving the data from transmitter
    mesh::thread::Sleep(ctx, std::chrono::milliseconds(500));

    // Shutdown Rx connection
    res = conn_rx->shutdown(ctx);
    ASSERT_EQ(res, connection::Result::success) << connection::result2str(res);
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

TEST(DISABLED_st2110_20, send_and_receive_data) {
    auto ctx = context::WithCancel(context::Background());
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
    ASSERT_EQ(res, connection::Result::success) << connection::result2str(res);
    ASSERT_EQ(conn_tx->state(), connection::State::configured);

    // Configure Rx
    auto conn_rx = new connection::ST2110_20Rx;
    res = conn_rx->configure(ctx, dev_port, cfg_st2110, cfg_video);
    ASSERT_EQ(res, connection::Result::success) << connection::result2str(res);
    ASSERT_EQ(conn_rx->state(), connection::State::configured);

    std::jthread rx_th, tx_th;
    try {
        rx_th = std::jthread(&rx_thread, std::ref(ctx), conn_rx, 1);
        tx_th = std::jthread(&tx_thread, std::ref(ctx), conn_tx);
    } catch (const std::system_error& e) {
        ASSERT_TRUE(0) << e.what();
    }

    rx_th.join();
    tx_th.join();

    delete conn_tx;
    delete conn_rx;
}

TEST(DISABLED_st2110_22, send_and_receive_data) {
    auto ctx = context::WithCancel(context::Background());
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
    ASSERT_EQ(res, connection::Result::success) << connection::result2str(res);
    ASSERT_EQ(conn_tx->state(), connection::State::configured);

    // Configure Rx
    auto conn_rx = new connection::ST2110_22Rx;
    res = conn_rx->configure(ctx, dev_port, cfg_st2110, cfg_video);
    ASSERT_EQ(res, connection::Result::success) << connection::result2str(res);
    ASSERT_EQ(conn_rx->state(), connection::State::configured);

    std::jthread rx_th, tx_th;
    try {
        rx_th = std::jthread(&rx_thread, std::ref(ctx), conn_rx, 0);
        tx_th = std::jthread(&tx_thread, std::ref(ctx), conn_tx);
    } catch (const std::system_error& e) {
        ASSERT_TRUE(0) << e.what();
    }

    rx_th.join();
    tx_th.join();

    delete conn_tx;
    delete conn_rx;
}

TEST(DISABLED_st2110_30, send_and_receive_data) {
    auto ctx = context::WithCancel(context::Background());
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
    ASSERT_EQ(res, connection::Result::success) << connection::result2str(res);
    ASSERT_EQ(conn_tx->state(), connection::State::configured);

    // Configure Rx
    auto conn_rx = new connection::ST2110_30Rx;
    res = conn_rx->configure(ctx, dev_port, cfg_st2110, cfg_audio);
    ASSERT_EQ(res, connection::Result::success) << connection::result2str(res);
    ASSERT_EQ(conn_rx->state(), connection::State::configured);

    std::jthread rx_th, tx_th;
    try {
        rx_th = std::jthread(&rx_thread, std::ref(ctx), conn_rx, 1);
        tx_th = std::jthread(&tx_thread, std::ref(ctx), conn_tx);
    } catch (const std::system_error& e) {
        ASSERT_TRUE(0) << e.what();
    }

    rx_th.join();
    tx_th.join();

    delete conn_tx;
    delete conn_rx;
}

/********************************************CONCEPT.md scenario**************************************** */
/**
 * How to run the test:
 * 1) Modify two variables port_card0 and port_card1 with the correct values of the network interfaces of the machine.
 * 2) Open 2 consoles
 * 2a) In the first console run the following command:
 *     ./media_proxy_unit_tests --gtest_also_run_disabled_tests --gtest_filter=*concept_scenarion.mtl_st20_rx
 * 2b) In the second console run the following command:
 *     ./media_proxy_unit_tests --gtest_also_run_disabled_tests --gtest_filter=*concept_scenarion.mtl_st20_tx
 * 3) Wait until test ends ~120s
 * 4) Check the output of the first console, it should show the number of received packets ~2000
 * example:
 *  received_packets_lossless: 2000
 *  received_packets_lossy: 0
 */

std::string port_card0("0000:4b:01.1");
std::string port_card1("0000:4b:11.1");

TEST(DISABLED_concept_scenarion, mtl_st20_tx) {
    auto ctx = context::WithCancel(context::Background());
    connection::Result res;

    MeshConfig_Video cfg_video;
    get_ST2110_video_cfg(cfg_video);

    MeshConfig_ST2110 cfg_st2110;
    get_ST2110_session_config(cfg_st2110, MESH_CONN_TRANSPORT_ST2110_20);
    memcpy(cfg_st2110.local_ip_addr, "192.168.96.2", sizeof("192.168.96.2"));
    memcpy(cfg_st2110.remote_ip_addr, "192.168.96.1", sizeof("192.168.96.1"));

    // Configure Tx
    auto conn_tx = new connection::ST2110_20Tx;
    res = conn_tx->configure(ctx, port_card0, cfg_st2110, cfg_video);
    ASSERT_EQ(res, connection::Result::success) << connection::result2str(res);
    ASSERT_EQ(conn_tx->state(), connection::State::configured);

    auto emulated_tx = new EmulatedTransmitter(ctx);
    // Change state Configured -> Active
    res = conn_tx->establish(ctx);
    ASSERT_EQ(res, connection::Result::success) << connection::result2str(res);
    ASSERT_EQ(conn_tx->state(), connection::State::active);

    // Setup Emulated Transmitter
    emulated_tx->establish(ctx);

    // Connect Emulated Transmitter to Tx connection
    emulated_tx->set_link(ctx, conn_tx);

    uint32_t data_size = cfg_video.width * cfg_video.height * 4;
    void *data = malloc(data_size);
    memcpy(data, DUMMY_DATA1, sizeof(DUMMY_DATA1));

    for (int i = 0; i < 2000; i++) {
        res = emulated_tx->transmit_wrapper(ctx, data, data_size);
        ASSERT_EQ(res, connection::Result::success) << connection::result2str(res);
        ASSERT_EQ(conn_tx->state(), connection::State::active);
    }

    // Shutdown Tx connection
    res = conn_tx->shutdown(ctx);
    ASSERT_EQ(res, connection::Result::success) << connection::result2str(res);
    ASSERT_EQ(conn_tx->state(), connection::State::closed);

    // Destroy resources
    delete emulated_tx;
    delete conn_tx;
    free(data);
}

TEST(DISABLED_concept_scenarion, mtl_st20_rx) {
    auto ctx = context::WithCancel(context::Background());
    connection::Result res;

    MeshConfig_Video cfg_video;
    get_ST2110_video_cfg(cfg_video);

    MeshConfig_ST2110 cfg_st2110;
    get_ST2110_session_config(cfg_st2110, MESH_CONN_TRANSPORT_ST2110_20);
    memcpy(cfg_st2110.local_ip_addr, "192.168.96.1", sizeof("192.168.96.1"));
    memcpy(cfg_st2110.remote_ip_addr, "192.168.96.2", sizeof("192.168.96.2"));

    // Configure Rx
    auto conn_rx = new connection::ST2110_20Rx;
    res = conn_rx->configure(ctx, port_card1, cfg_st2110, cfg_video);
    ASSERT_EQ(res, connection::Result::success) << connection::result2str(res);
    ASSERT_EQ(conn_rx->state(), connection::State::configured);

    auto emulated_rx = new EmulatedReceiver(ctx);
    emulated_rx->establish(ctx);

    conn_rx->set_link(ctx, emulated_rx);
    // Connect Rx connection to Emulated Receiver
    res = conn_rx->establish(ctx);
    ASSERT_EQ(res, connection::Result::success) << connection::result2str(res);
    ASSERT_EQ(conn_rx->state(), connection::State::active);

    // Sleep some sufficient time to allow receiving the data from transmitter
    mesh::thread::Sleep(ctx, std::chrono::milliseconds(1000 * 120)); // 120s

    // Shutdown Rx connection
    res = conn_rx->shutdown(ctx);
    ASSERT_EQ(res, connection::Result::success) << connection::result2str(res);
    ASSERT_EQ(conn_rx->state(), connection::State::closed);

    printf("received_packets_lossless: %d\n", emulated_rx->received_packets_lossless);
    printf("received_packets_lossy: %d\n", emulated_rx->received_packets_lossy);

    delete emulated_rx;
    delete conn_rx;
}
