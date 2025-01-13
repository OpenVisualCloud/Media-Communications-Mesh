#include <gtest/gtest.h>
#include <bsd/string.h>
#include "mesh_client.h"
#include "mesh_conn.h"
#include "mcm_dp.h"

/**
 * Test creation and deletion of a mesh client
 */
TEST(APITests_MeshClient, Test_CreateDeleteClient) {
    MeshClient *mc = NULL;
    int err;

    err = mesh_create_client(&mc, NULL);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_NE(mc, (MeshClient *)NULL);

    err = mesh_delete_client(&mc);
    EXPECT_EQ(err, 0) << mesh_err2str(err);
    EXPECT_EQ(mc, (MeshClient *)NULL);
}

/**
 * Test negative scenario of creating a mesh client
 */
TEST(APITests_MeshClient, TestNegative_CreateClient) {
    int err = mesh_create_client(NULL, NULL);
    ASSERT_EQ(err, -MESH_ERR_BAD_CLIENT_PTR) << mesh_err2str(err);
}

/**
 * Test negative scenario of deteling a mesh client
 */
TEST(APITests_MeshClient, TestNegative_DeleteClient) {
    int err = mesh_delete_client(NULL);
    ASSERT_EQ(err, -MESH_ERR_BAD_CLIENT_PTR) << mesh_err2str(err);
}

/**
 * Create a mock mesh connection
 */
mcm_conn_context * mock_create_connection(mcm_conn_param* param)
{
    mcm_conn_context *conn;
    
    conn = (mcm_conn_context *)calloc(1, sizeof(mcm_conn_context));
    conn->frame_size = 1024; /* Magic number for checking frame size */

    return conn;
}

/**
 * Destroy a mock mesh connection
 */
void mock_destroy_connection(mcm_conn_context *pctx)
{
    free(pctx);
}

/**
 * Variable to hold the last timeout value passed to any API function
 */
int __last_timeout;

/**
 * Dequeue a mock mesh buffer
 */
mcm_buffer * mock_dequeue_buf(mcm_conn_context *pctx, int timeout, int *error_code)
{
    mcm_buffer *buf;

    *error_code = 0;
    __last_timeout = timeout;

    /* Magic number for simulating that the connection is closed */
    if (timeout == 12345)
        return NULL;

    buf = (mcm_buffer *)calloc(1, sizeof(mcm_buffer));
    if (buf) {
        buf->len = 192;         /* Magic number for checking buf data len */
        pctx->frame_size = 384; /* Magic number for checking buf size */
    }

    return buf;
}

/**
 * Enqueue a mock mesh buffer
 */
int mock_enqueue_buf(mcm_conn_context *pctx, mcm_buffer *buf)
{
    free(buf);
    return 0;
}

void * mock_grpc_create_client()
{
    return NULL;
}

void * mock_grpc_create_client_json(const std::string& endpoint)
{
    return NULL;
}

void mock_grpc_destroy_client(void *client)
{
}

void * mock_grpc_create_conn(void *client, mcm_conn_param *param)
{
    return NULL;
}

void * mock_grpc_create_conn_json(void *client, const mesh::ConnectionJsonConfig& cfg)
{
    return NULL;
}

void mock_grpc_destroy_conn(void *conn)
{
}

/**
 * APITests setup
 */
void APITests_Setup()
{
    mesh_internal_ops.create_conn = mock_create_connection;
    mesh_internal_ops.destroy_conn = mock_destroy_connection;
    mesh_internal_ops.dequeue_buf = mock_dequeue_buf;
    mesh_internal_ops.enqueue_buf = mock_enqueue_buf;

    mesh_internal_ops.grpc_create_client = mock_grpc_create_client;
    mesh_internal_ops.grpc_create_client_json = mock_grpc_create_client_json;
    mesh_internal_ops.grpc_destroy_client = mock_grpc_destroy_client;
    mesh_internal_ops.grpc_create_conn = mock_grpc_create_conn;
    mesh_internal_ops.grpc_create_conn_json = mock_grpc_create_conn_json;
    mesh_internal_ops.grpc_destroy_conn = mock_grpc_destroy_conn;
}

/**
 * Test negative scenarios of deleting a client with live connections
 */
TEST(APITests_MeshConnection, TestNegative_DeleteClientLiveConnections) {
    MeshConfig_Memif memif_config = {};
    MeshConfig_Video video_config = {};
    MeshConnection *conn = NULL;
    MeshClient *mc, *mc_copy = NULL;
    int err;

    APITests_Setup();

    mesh_create_client(&mc, NULL);
    mesh_create_connection(mc, &conn);

    mc_copy = mc;

    err = mesh_delete_client(&mc);
    EXPECT_EQ(err, -MESH_ERR_FOUND_ALLOCATED) << mesh_err2str(err);
    EXPECT_EQ(mc, mc_copy);
}

/**
 * Test creation, establishing, shutting down, and deletion of a mesh connection
 */
TEST(APITests_MeshConnection, Test_CreateEstablishShutdownDeleteConnection) {
    MeshConfig_Memif memif_config = {
        .socket_path = "/run/mcm/mcm_memif_0.sock",
        .interface_id = 123,
    };
    MeshConfig_ST2110 st2110_config = {
        .remote_ip_addr = "192.168.95.2",
        .remote_port = 9002,
        .local_ip_addr = "192.168.95.1",
        .local_port = 9001,
        .transport = MESH_CONN_TRANSPORT_ST2110_22,
    };
    MeshConfig_RDMA rdma_config = {
        .remote_ip_addr = "192.168.95.2",
        .remote_port = 9002,
        .local_ip_addr = "192.168.95.1",
        .local_port = 9001,
    };
    MeshConfig_Video video_config = {
        .width = 1920,
        .height = 1080,
        .fps = 60,
        .pixel_format = MESH_VIDEO_PIXEL_FORMAT_YUV422PLANAR10LE,
    };
    MeshConfig_Audio audio_config = {
        .channels = 2,
        .sample_rate = MESH_AUDIO_SAMPLE_RATE_44100,
        .format = MESH_AUDIO_FORMAT_PCM_S24BE,
        .packet_time = MESH_AUDIO_PACKET_TIME_1_09MS,
    };
    MeshConnection *conn = NULL;
    MeshClient *mc = NULL;
    int err;

    APITests_Setup();

    err = mesh_create_client(&mc, NULL);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_NE(mc, (MeshClient *)NULL);

    err = mesh_create_connection(mc, &conn);
    EXPECT_EQ(err, 0) << mesh_err2str(err);
    EXPECT_NE(conn, (MeshConnection *)NULL);
    EXPECT_EQ(conn->client, mc);
    if (err || !conn)
        goto exit;

    /**
     * Case A - Transmit video over memif
     */
    err = mesh_apply_connection_config_memif(conn, &memif_config);
    EXPECT_EQ(err, 0) << mesh_err2str(err);

    err = mesh_apply_connection_config_video(conn, &video_config);
    EXPECT_EQ(err, 0) << mesh_err2str(err);

    err = mesh_establish_connection(conn, MESH_CONN_KIND_SENDER);
    EXPECT_EQ(err, 0) << mesh_err2str(err);
    EXPECT_EQ(conn->buf_size, 1024); /* Magic number hardcoded in mock function */
    EXPECT_EQ(conn->client, mc);

    err = mesh_shutdown_connection(conn);
    EXPECT_EQ(err, 0) << mesh_err2str(err);
    EXPECT_EQ(conn->client, mc);

    /**
     * Case B - Receive video over ST2110-30
     */
    err = mesh_apply_connection_config_st2110(conn, &st2110_config);
    EXPECT_EQ(err, 0) << mesh_err2str(err);

    err = mesh_apply_connection_config_video(conn, &video_config);
    EXPECT_EQ(err, 0) << mesh_err2str(err);

    err = mesh_establish_connection(conn, MESH_CONN_KIND_RECEIVER);
    EXPECT_EQ(err, 0) << mesh_err2str(err);
    EXPECT_EQ(conn->buf_size, 1024); /* Magic number hardcoded in mock function */
    EXPECT_EQ(conn->client, mc);

    err = mesh_shutdown_connection(conn);
    EXPECT_EQ(err, 0) << mesh_err2str(err);
    EXPECT_EQ(conn->client, mc);

    /**
     * Case B - Transmit audio over RDMA
     */
    err = mesh_apply_connection_config_rdma(conn, &rdma_config);
    EXPECT_EQ(err, 0) << mesh_err2str(err);

    err = mesh_apply_connection_config_audio(conn, &audio_config);
    EXPECT_EQ(err, 0) << mesh_err2str(err);

    err = mesh_establish_connection(conn, MESH_CONN_KIND_SENDER);
    EXPECT_EQ(err, 0) << mesh_err2str(err);
    EXPECT_EQ(conn->buf_size, 1024); /* Magic number hardcoded in mock function */
    EXPECT_EQ(conn->client, mc);

    err = mesh_shutdown_connection(conn);
    EXPECT_EQ(err, 0) << mesh_err2str(err);
    EXPECT_EQ(conn->client, mc);

    err = mesh_delete_connection(&conn);
    EXPECT_EQ(err, 0) << mesh_err2str(err);
    EXPECT_EQ(conn, (MeshConnection *)NULL);

exit:
    err = mesh_delete_client(&mc);
    EXPECT_EQ(err, 0) << mesh_err2str(err);
    EXPECT_EQ(mc, (MeshClient *)NULL);
}

/**
 * Test apply connection configuration functions
 */
TEST(APITests_MeshConnection, Test_ApplyConfig) {
    MeshConfig_Memif memif_config_empty = {};
    MeshConfig_Memif memif_config = {
        .socket_path = "/run/mcm/mcm_memif_0.sock",
        .interface_id = 123,
    };
    MeshConfig_ST2110 st2110_config_empty = {};
    MeshConfig_ST2110 st2110_config = {
        .remote_ip_addr = "192.168.95.2",
        .remote_port = 9002,
        .local_ip_addr = "192.168.95.1",
        .local_port = 9001,
        .transport = MESH_CONN_TRANSPORT_ST2110_22,
    };
    MeshConfig_RDMA rdma_config_empty = {};
    MeshConfig_RDMA rdma_config = {
        .remote_ip_addr = "192.168.95.2",
        .remote_port = 9002,
        .local_ip_addr = "192.168.95.1",
        .local_port = 9001,
    };
    MeshConfig_Video video_config_empty = {};
    MeshConfig_Video video_config = {
        .width = 1920,
        .height = 1080,
        .fps = 60,
        .pixel_format = MESH_VIDEO_PIXEL_FORMAT_YUV422PLANAR10LE,
    };
    MeshConfig_Audio audio_config_empty = {};
    MeshConfig_Audio audio_config = {
        .channels = 2,
        .sample_rate = MESH_AUDIO_SAMPLE_RATE_44100,
        .format = MESH_AUDIO_FORMAT_PCM_S24BE,
        .packet_time = MESH_AUDIO_PACKET_TIME_1_09MS,
    };
    mesh::ClientContext mc_ctx(nullptr);
    mesh::ConnectionContext ctx(&mc_ctx);
    MeshConnection *conn;
    int err;

    conn = (MeshConnection *)&ctx;

    /**
     * Case A - memif connection type
     */
    err = mesh_apply_connection_config_memif(conn, &memif_config);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(ctx.cfg.conn_type, MESH_CONN_TYPE_MEMIF);
    ASSERT_EQ(memcmp(&ctx.cfg.conn.memif, &memif_config,
        sizeof(MeshConfig_Memif)), 0);

    memset(&ctx.cfg.conn.memif, 0, sizeof(MeshConfig_Memif));
    err = mesh_apply_connection_config_memif(conn, &memif_config_empty);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(ctx.cfg.conn_type, MESH_CONN_TYPE_MEMIF);
    ASSERT_EQ(memcmp(&ctx.cfg.conn.memif, &memif_config_empty,
        sizeof(MeshConfig_Memif)), 0);

    /**
     * Case B - SMPTE ST2110-XX connection type
     */
    err = mesh_apply_connection_config_st2110(conn, &st2110_config);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(ctx.cfg.conn_type, MESH_CONN_TYPE_ST2110);
    ASSERT_EQ(memcmp(&ctx.cfg.conn.st2110, &st2110_config,
        sizeof(MeshConfig_ST2110)), 0);

    memset(&ctx.cfg.conn.st2110, 0, sizeof(MeshConfig_ST2110));
    err = mesh_apply_connection_config_st2110(conn, &st2110_config_empty);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(ctx.cfg.conn_type, MESH_CONN_TYPE_ST2110);
    ASSERT_EQ(memcmp(&ctx.cfg.conn.st2110, &st2110_config_empty,
        sizeof(MeshConfig_ST2110)), 0);

    /**
     * Case C - RDMA connection type
     */
    err = mesh_apply_connection_config_rdma(conn, &rdma_config);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(ctx.cfg.conn_type, MESH_CONN_TYPE_RDMA);
    ASSERT_EQ(memcmp(&ctx.cfg.conn.rdma, &rdma_config,
        sizeof(MeshConfig_RDMA)), 0);

    memset(&ctx.cfg.conn.rdma, 0, sizeof(MeshConfig_RDMA));
    err = mesh_apply_connection_config_rdma(conn, &rdma_config_empty);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(ctx.cfg.conn_type, MESH_CONN_TYPE_RDMA);
    ASSERT_EQ(memcmp(&ctx.cfg.conn.rdma, &rdma_config_empty,
        sizeof(MeshConfig_RDMA)), 0);

    /**
     * Case D - Video type
     */
    err = mesh_apply_connection_config_video(conn, &video_config);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(ctx.cfg.payload_type, MESH_PAYLOAD_TYPE_VIDEO);
    ASSERT_EQ(memcmp(&ctx.cfg.payload.video, &video_config,
        sizeof(MeshConfig_Video)), 0);

    memset(&ctx.cfg.payload.video, 0, sizeof(MeshConfig_Video));
    err = mesh_apply_connection_config_video(conn, &video_config_empty);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(ctx.cfg.payload_type, MESH_PAYLOAD_TYPE_VIDEO);
    ASSERT_EQ(memcmp(&ctx.cfg.payload.video, &video_config_empty,
        sizeof(MeshConfig_Video)), 0);

    /**
     * Case E - Audio type
     */
    err = mesh_apply_connection_config_audio(conn, &audio_config);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(ctx.cfg.payload_type, MESH_PAYLOAD_TYPE_AUDIO);
    ASSERT_EQ(memcmp(&ctx.cfg.payload.audio, &audio_config,
        sizeof(MeshConfig_Audio)), 0);

    memset(&ctx.cfg.payload.audio, 0, sizeof(MeshConfig_Audio));
    err = mesh_apply_connection_config_audio(conn, &audio_config_empty);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(ctx.cfg.payload_type, MESH_PAYLOAD_TYPE_AUDIO);
    ASSERT_EQ(memcmp(&ctx.cfg.payload.audio, &audio_config_empty,
        sizeof(MeshConfig_Audio)), 0);
}

/**
 * Test negative scenarios of applying connection configuration
 */
TEST(APITests_MeshConnection, TestNegative_ApplyConfig) {
    MeshConfig_Memif memif_config = {};
    MeshConfig_ST2110 st2110_config = {};
    MeshConfig_RDMA rdma_config = {};
    MeshConfig_Video video_config = {};
    MeshConfig_Audio audio_config = {};
    mesh::ClientContext mc_ctx(nullptr);
    mesh::ConnectionContext ctx(&mc_ctx);
    MeshConnection *conn;
    int err;

    conn = (MeshConnection *)&ctx;

    /**
     * Case A - memif connection type
     */
    err = mesh_apply_connection_config_memif(NULL, NULL);
    ASSERT_EQ(err, -MESH_ERR_BAD_CONN_PTR) << mesh_err2str(err);

    err = mesh_apply_connection_config_memif(conn, NULL);
    ASSERT_EQ(err, -MESH_ERR_BAD_CONFIG_PTR) << mesh_err2str(err);

    err = mesh_apply_connection_config_st2110(conn, &st2110_config);
    err = mesh_apply_connection_config_memif(conn, &memif_config);
    ASSERT_EQ(err, 0) << mesh_err2str(err);

    err = mesh_apply_connection_config_rdma(conn, &rdma_config);
    err = mesh_apply_connection_config_memif(conn, &memif_config);
    ASSERT_EQ(err, 0) << mesh_err2str(err);

    err = mesh_apply_connection_config_video(conn, &video_config);
    err = mesh_apply_connection_config_memif(conn, &memif_config);
    ASSERT_EQ(err, 0) << mesh_err2str(err);

    err = mesh_apply_connection_config_audio(conn, &audio_config);
    err = mesh_apply_connection_config_memif(conn, &memif_config);
    ASSERT_EQ(err, 0) << mesh_err2str(err);

    /**
     * Case B - SMPTE ST2110-XX connection type
     */
    err = mesh_apply_connection_config_st2110(NULL, NULL);
    ASSERT_EQ(err, -MESH_ERR_BAD_CONN_PTR) << mesh_err2str(err);

    err = mesh_apply_connection_config_st2110(conn, NULL);
    ASSERT_EQ(err, -MESH_ERR_BAD_CONFIG_PTR) << mesh_err2str(err);

    err = mesh_apply_connection_config_memif(conn, &memif_config);
    err = mesh_apply_connection_config_st2110(conn, &st2110_config);
    ASSERT_EQ(err, 0) << mesh_err2str(err);

    err = mesh_apply_connection_config_rdma(conn, &rdma_config);
    err = mesh_apply_connection_config_st2110(conn, &st2110_config);
    ASSERT_EQ(err, 0) << mesh_err2str(err);

    err = mesh_apply_connection_config_video(conn, &video_config);
    err = mesh_apply_connection_config_st2110(conn, &st2110_config);
    ASSERT_EQ(err, 0) << mesh_err2str(err);

    err = mesh_apply_connection_config_audio(conn, &audio_config);
    err = mesh_apply_connection_config_st2110(conn, &st2110_config);
    ASSERT_EQ(err, 0) << mesh_err2str(err);

    /**
     * Case C - RDMA connection type
     */
    err = mesh_apply_connection_config_rdma(NULL, NULL);
    ASSERT_EQ(err, -MESH_ERR_BAD_CONN_PTR) << mesh_err2str(err);

    err = mesh_apply_connection_config_rdma(conn, NULL);
    ASSERT_EQ(err, -MESH_ERR_BAD_CONFIG_PTR) << mesh_err2str(err);

    err = mesh_apply_connection_config_memif(conn, &memif_config);
    err = mesh_apply_connection_config_rdma(conn, &rdma_config);
    ASSERT_EQ(err, 0) << mesh_err2str(err);

    err = mesh_apply_connection_config_st2110(conn, &st2110_config);
    err = mesh_apply_connection_config_rdma(conn, &rdma_config);
    ASSERT_EQ(err, 0) << mesh_err2str(err);

    err = mesh_apply_connection_config_video(conn, &video_config);
    err = mesh_apply_connection_config_rdma(conn, &rdma_config);
    ASSERT_EQ(err, 0) << mesh_err2str(err);

    err = mesh_apply_connection_config_audio(conn, &audio_config);
    err = mesh_apply_connection_config_rdma(conn, &rdma_config);
    ASSERT_EQ(err, 0) << mesh_err2str(err);

    /**
     * Case D - Video type
     */
    err = mesh_apply_connection_config_video(NULL, NULL);
    ASSERT_EQ(err, -MESH_ERR_BAD_CONN_PTR) << mesh_err2str(err);

    err = mesh_apply_connection_config_video(conn, NULL);
    ASSERT_EQ(err, -MESH_ERR_BAD_CONFIG_PTR) << mesh_err2str(err);

    err = mesh_apply_connection_config_memif(conn, &memif_config);
    err = mesh_apply_connection_config_video(conn, &video_config);
    ASSERT_EQ(err, 0) << mesh_err2str(err);

    err = mesh_apply_connection_config_st2110(conn, &st2110_config);
    err = mesh_apply_connection_config_video(conn, &video_config);
    ASSERT_EQ(err, 0) << mesh_err2str(err);

    err = mesh_apply_connection_config_rdma(conn, &rdma_config);
    err = mesh_apply_connection_config_video(conn, &video_config);
    ASSERT_EQ(err, 0) << mesh_err2str(err);

    err = mesh_apply_connection_config_audio(conn, &audio_config);
    err = mesh_apply_connection_config_video(conn, &video_config);
    ASSERT_EQ(err, 0) << mesh_err2str(err);

    /**
     * Case E - Audio type
     */
    err = mesh_apply_connection_config_audio(NULL, NULL);
    ASSERT_EQ(err, -MESH_ERR_BAD_CONN_PTR) << mesh_err2str(err);

    err = mesh_apply_connection_config_audio(conn, NULL);
    ASSERT_EQ(err, -MESH_ERR_BAD_CONFIG_PTR) << mesh_err2str(err);

    err = mesh_apply_connection_config_memif(conn, &memif_config);
    err = mesh_apply_connection_config_audio(conn, &audio_config);
    ASSERT_EQ(err, 0) << mesh_err2str(err);

    err = mesh_apply_connection_config_st2110(conn, &st2110_config);
    err = mesh_apply_connection_config_audio(conn, &audio_config);
    ASSERT_EQ(err, 0) << mesh_err2str(err);

    err = mesh_apply_connection_config_rdma(conn, &rdma_config);
    err = mesh_apply_connection_config_audio(conn, &audio_config);
    ASSERT_EQ(err, 0) << mesh_err2str(err);

    err = mesh_apply_connection_config_video(conn, &video_config);
    err = mesh_apply_connection_config_audio(conn, &audio_config);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
}

/**
 * Test parsing connection configuration
 */
TEST(APITests_MeshConnection, Test_ParseConnectionConfig) {
    MeshConfig_Memif memif_config = {
        .socket_path = "/run/mcm/mcm_memif_0.sock",
        .interface_id = 123,
    };
    MeshConfig_ST2110 st2110_config = {
        .remote_ip_addr = "192.168.95.2",
        .remote_port = 9002,
        .local_ip_addr = "192.168.95.1",
        .local_port = 9001,
        .transport = MESH_CONN_TRANSPORT_ST2110_22,
    };
    MeshConfig_RDMA rdma_config = {
        .remote_ip_addr = "192.168.95.2",
        .remote_port = 9002,
        .local_ip_addr = "192.168.95.1",
        .local_port = 9001,
    };
    MeshConfig_Video video_config = {};
    MeshConfig_Audio audio_config = {};
    mesh::ClientContext mc_ctx(nullptr);
    mesh::ConnectionContext ctx(&mc_ctx);
    MeshConnection *conn;
    mcm_conn_param param;
    int err;

    conn = (MeshConnection *)&ctx;

    /**
     * Case A - memif connection type
     */
    ctx.cfg.kind = MESH_CONN_KIND_SENDER;
    mesh_apply_connection_config_memif(conn, &memif_config);
    mesh_apply_connection_config_video(conn, &video_config);
    err = ctx.parse_conn_config(&param);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(param.type, is_tx);
    ASSERT_EQ(param.protocol, PROTO_MEMIF);
    ASSERT_STREQ(param.memif_interface.socket_path, "/run/mcm/mcm_memif_0.sock");
    ASSERT_EQ(param.memif_interface.interface_id, 123);
    ASSERT_EQ(param.memif_interface.is_master, 1);
    ASSERT_EQ(param.payload_type, PAYLOAD_TYPE_ST20_VIDEO);

    ctx.cfg.kind = MESH_CONN_KIND_RECEIVER;
    mesh_apply_connection_config_memif(conn, &memif_config);
    err = ctx.parse_conn_config(&param);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(param.type, is_rx);
    ASSERT_EQ(param.protocol, PROTO_MEMIF);
    ASSERT_STREQ(param.memif_interface.socket_path, "/run/mcm/mcm_memif_0.sock");
    ASSERT_EQ(param.memif_interface.interface_id, 123);
    ASSERT_EQ(param.memif_interface.is_master, 0);
    ASSERT_EQ(param.payload_type, PAYLOAD_TYPE_ST20_VIDEO);

    ctx.cfg.kind = MESH_CONN_KIND_SENDER;
    mesh_apply_connection_config_memif(conn, &memif_config);
    mesh_apply_connection_config_audio(conn, &audio_config);
    err = ctx.parse_conn_config(&param);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(param.type, is_tx);
    ASSERT_EQ(param.protocol, PROTO_MEMIF);
    ASSERT_STREQ(param.memif_interface.socket_path, "/run/mcm/mcm_memif_0.sock");
    ASSERT_EQ(param.memif_interface.interface_id, 123);
    ASSERT_EQ(param.memif_interface.is_master, 1);
    ASSERT_EQ(param.payload_type, PAYLOAD_TYPE_ST30_AUDIO);

    ctx.cfg.kind = MESH_CONN_KIND_RECEIVER;
    mesh_apply_connection_config_memif(conn, &memif_config);
    err = ctx.parse_conn_config(&param);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(param.type, is_rx);
    ASSERT_EQ(param.protocol, PROTO_MEMIF);
    ASSERT_STREQ(param.memif_interface.socket_path, "/run/mcm/mcm_memif_0.sock");
    ASSERT_EQ(param.memif_interface.interface_id, 123);
    ASSERT_EQ(param.memif_interface.is_master, 0);
    ASSERT_EQ(param.payload_type, PAYLOAD_TYPE_ST30_AUDIO);

    /**
     * Case B - SMPTE ST2110-XX connection type
     */
    ctx.cfg.kind = MESH_CONN_KIND_SENDER;
    mesh_apply_connection_config_st2110(conn, &st2110_config);
    mesh_apply_connection_config_video(conn, &video_config);
    err = ctx.parse_conn_config(&param);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(param.type, is_tx);
    ASSERT_EQ(param.protocol, PROTO_AUTO);
    ASSERT_STREQ(param.local_addr.ip, "192.168.95.1");
    ASSERT_STREQ(param.local_addr.port, "9001");
    ASSERT_STREQ(param.remote_addr.ip, "192.168.95.2");
    ASSERT_STREQ(param.remote_addr.port, "9002");
    ASSERT_EQ(param.payload_type, PAYLOAD_TYPE_ST22_VIDEO);

    ctx.cfg.kind = MESH_CONN_KIND_RECEIVER;
    mesh_apply_connection_config_st2110(conn, &st2110_config);
    mesh_apply_connection_config_video(conn, &video_config);
    err = ctx.parse_conn_config(&param);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(param.type, is_rx);
    ASSERT_EQ(param.protocol, PROTO_AUTO);
    ASSERT_STREQ(param.local_addr.ip, "192.168.95.1");
    ASSERT_STREQ(param.local_addr.port, "9001");
    ASSERT_STREQ(param.remote_addr.ip, "192.168.95.2");
    ASSERT_STREQ(param.remote_addr.port, "9002");
    ASSERT_EQ(param.payload_type, PAYLOAD_TYPE_ST22_VIDEO);

    ctx.cfg.kind = MESH_CONN_KIND_SENDER;
    st2110_config.transport = MESH_CONN_TRANSPORT_ST2110_20,
    mesh_apply_connection_config_st2110(conn, &st2110_config);
    mesh_apply_connection_config_video(conn, &video_config);
    err = ctx.parse_conn_config(&param);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(param.type, is_tx);
    ASSERT_EQ(param.protocol, PROTO_AUTO);
    ASSERT_STREQ(param.local_addr.ip, "192.168.95.1");
    ASSERT_STREQ(param.local_addr.port, "9001");
    ASSERT_STREQ(param.remote_addr.ip, "192.168.95.2");
    ASSERT_STREQ(param.remote_addr.port, "9002");
    ASSERT_EQ(param.payload_type, PAYLOAD_TYPE_ST20_VIDEO);

    ctx.cfg.kind = MESH_CONN_KIND_RECEIVER;
    mesh_apply_connection_config_st2110(conn, &st2110_config);
    mesh_apply_connection_config_video(conn, &video_config);
    err = ctx.parse_conn_config(&param);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(param.type, is_rx);
    ASSERT_EQ(param.protocol, PROTO_AUTO);
    ASSERT_STREQ(param.local_addr.ip, "192.168.95.1");
    ASSERT_STREQ(param.local_addr.port, "9001");
    ASSERT_STREQ(param.remote_addr.ip, "192.168.95.2");
    ASSERT_STREQ(param.remote_addr.port, "9002");
    ASSERT_EQ(param.payload_type, PAYLOAD_TYPE_ST20_VIDEO);

    ctx.cfg.kind = MESH_CONN_KIND_SENDER;
    st2110_config.transport = MESH_CONN_TRANSPORT_ST2110_30,
    mesh_apply_connection_config_st2110(conn, &st2110_config);
    mesh_apply_connection_config_audio(conn, &audio_config);
    err = ctx.parse_conn_config(&param);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(param.type, is_tx);
    ASSERT_EQ(param.protocol, PROTO_AUTO);
    ASSERT_STREQ(param.local_addr.ip, "192.168.95.1");
    ASSERT_STREQ(param.local_addr.port, "9001");
    ASSERT_STREQ(param.remote_addr.ip, "192.168.95.2");
    ASSERT_STREQ(param.remote_addr.port, "9002");
    ASSERT_EQ(param.payload_type, PAYLOAD_TYPE_ST30_AUDIO);

    ctx.cfg.kind = MESH_CONN_KIND_RECEIVER;
    mesh_apply_connection_config_st2110(conn, &st2110_config);
    mesh_apply_connection_config_audio(conn, &audio_config);
    err = ctx.parse_conn_config(&param);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(param.type, is_rx);
    ASSERT_EQ(param.protocol, PROTO_AUTO);
    ASSERT_STREQ(param.local_addr.ip, "192.168.95.1");
    ASSERT_STREQ(param.local_addr.port, "9001");
    ASSERT_STREQ(param.remote_addr.ip, "192.168.95.2");
    ASSERT_STREQ(param.remote_addr.port, "9002");
    ASSERT_EQ(param.payload_type, PAYLOAD_TYPE_ST30_AUDIO);

    /**
     * Case C - RDMA connection type
     */
    ctx.cfg.kind = MESH_CONN_KIND_SENDER;
    mesh_apply_connection_config_rdma(conn, &rdma_config);
    mesh_apply_connection_config_video(conn, &video_config);
    err = ctx.parse_conn_config(&param);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(param.type, is_tx);
    ASSERT_EQ(param.protocol, PROTO_AUTO);
    ASSERT_STREQ(param.local_addr.ip, "192.168.95.1");
    ASSERT_STREQ(param.local_addr.port, "9001");
    ASSERT_STREQ(param.remote_addr.ip, "192.168.95.2");
    ASSERT_STREQ(param.remote_addr.port, "9002");
    ASSERT_EQ(param.payload_type, PAYLOAD_TYPE_RDMA_VIDEO);

    ctx.cfg.kind = MESH_CONN_KIND_RECEIVER;
    mesh_apply_connection_config_rdma(conn, &rdma_config);
    mesh_apply_connection_config_video(conn, &video_config);
    err = ctx.parse_conn_config(&param);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(param.type, is_rx);
    ASSERT_EQ(param.protocol, PROTO_AUTO);
    ASSERT_STREQ(param.local_addr.ip, "192.168.95.1");
    ASSERT_STREQ(param.local_addr.port, "9001");
    ASSERT_STREQ(param.remote_addr.ip, "192.168.95.2");
    ASSERT_STREQ(param.remote_addr.port, "9002");
    ASSERT_EQ(param.payload_type, PAYLOAD_TYPE_RDMA_VIDEO);

    ctx.cfg.kind = MESH_CONN_KIND_SENDER;
    mesh_apply_connection_config_rdma(conn, &rdma_config);
    mesh_apply_connection_config_audio(conn, &audio_config);
    err = ctx.parse_conn_config(&param);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(param.type, is_tx);
    ASSERT_EQ(param.protocol, PROTO_AUTO);
    ASSERT_STREQ(param.local_addr.ip, "192.168.95.1");
    ASSERT_STREQ(param.local_addr.port, "9001");
    ASSERT_STREQ(param.remote_addr.ip, "192.168.95.2");
    ASSERT_STREQ(param.remote_addr.port, "9002");
    /**
     * TODO: Rework this to enable both video and audio payloads.
     */
    ASSERT_EQ(param.payload_type, PAYLOAD_TYPE_RDMA_VIDEO);

    ctx.cfg.kind = MESH_CONN_KIND_RECEIVER;
    mesh_apply_connection_config_rdma(conn, &rdma_config);
    mesh_apply_connection_config_audio(conn, &audio_config);
    err = ctx.parse_conn_config(&param);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(param.type, is_rx);
    ASSERT_EQ(param.protocol, PROTO_AUTO);
    ASSERT_STREQ(param.local_addr.ip, "192.168.95.1");
    ASSERT_STREQ(param.local_addr.port, "9001");
    ASSERT_STREQ(param.remote_addr.ip, "192.168.95.2");
    ASSERT_STREQ(param.remote_addr.port, "9002");
    /**
     * TODO: Rework this to enable both video and audio payloads.
     */
    ASSERT_EQ(param.payload_type, PAYLOAD_TYPE_RDMA_VIDEO);
}

/**
 * Test negative scenarios of parsing connection configuration - invalid config
 */
TEST(APITests_MeshConnection, TestNegative_ParseConnectionConfigInval) {
    MeshConfig_ST2110 st2110_config = {};
    mesh::ClientContext mc_ctx(nullptr);
    mesh::ConnectionContext ctx(&mc_ctx);
    MeshConnection *conn;
    mcm_conn_param param;
    int err;

    conn = (MeshConnection *)&ctx;

    ctx.cfg.kind = 2;
    err = ctx.parse_conn_config(&param);
    ASSERT_EQ(err, -MESH_ERR_CONN_CONFIG_INVAL) << mesh_err2str(err);

    ctx.cfg.kind = MESH_CONN_KIND_SENDER;
    ctx.cfg.conn_type = 4,
    err = ctx.parse_conn_config(&param);
    ASSERT_EQ(err, -MESH_ERR_CONN_CONFIG_INVAL) << mesh_err2str(err);

    st2110_config.transport = 3;
    mesh_apply_connection_config_st2110(conn, &st2110_config);
    err = ctx.parse_conn_config(&param);
    ASSERT_EQ(err, -MESH_ERR_CONN_CONFIG_INVAL) << mesh_err2str(err);
}

/**
 * Test negative scenarios of parsing connection configuration - incompatible config
 */
TEST(APITests_MeshConnection, TestNegative_ParseConnectionConfigIncompat) {
    MeshConfig_ST2110 st2110_config = {};
    MeshConfig_Video video_config = {};
    MeshConfig_Audio audio_config = {};
    mesh::ClientContext mc_ctx(nullptr);
    mesh::ConnectionContext ctx(&mc_ctx);
    MeshConnection *conn;
    mcm_conn_param param;
    int err;

    conn = (MeshConnection *)&ctx;

    st2110_config.transport = MESH_CONN_TRANSPORT_ST2110_20;
    mesh_apply_connection_config_st2110(conn, &st2110_config);
    mesh_apply_connection_config_audio(conn, &audio_config);
    err = ctx.parse_conn_config(&param);
    ASSERT_EQ(err, -MESH_ERR_CONN_CONFIG_INCOMPAT) << mesh_err2str(err);

    st2110_config.transport = MESH_CONN_TRANSPORT_ST2110_22;
    mesh_apply_connection_config_st2110(conn, &st2110_config);
    mesh_apply_connection_config_audio(conn, &audio_config);
    err = ctx.parse_conn_config(&param);
    ASSERT_EQ(err, -MESH_ERR_CONN_CONFIG_INCOMPAT) << mesh_err2str(err);

    st2110_config.transport = MESH_CONN_TRANSPORT_ST2110_30;
    mesh_apply_connection_config_st2110(conn, &st2110_config);
    mesh_apply_connection_config_video(conn, &video_config);
    err = ctx.parse_conn_config(&param);
    ASSERT_EQ(err, -MESH_ERR_CONN_CONFIG_INCOMPAT) << mesh_err2str(err);
}

/**
 * Test parsing video payload configuration
 */
TEST(APITests_MeshConnection, Test_ParseVideoPayloadConfig) {
    MeshConfig_Video cfg = {
        .width = 1920,
        .height = 1080,
        .fps = 60,
        .pixel_format = MESH_VIDEO_PIXEL_FORMAT_YUV422PLANAR10LE,
    };
    mesh::ClientContext mc_ctx(nullptr);
    mesh::ConnectionContext ctx(&mc_ctx);
    MeshConnection *conn;
    mcm_conn_param param;
    int err;

    conn = (MeshConnection *)&ctx;

    mesh_apply_connection_config_video(conn, &cfg);
    err = ctx.parse_payload_config(&param);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(param.width, 1920);
    ASSERT_EQ(param.payload_args.video_args.width, 1920);
    ASSERT_EQ(param.height, 1080);
    ASSERT_EQ(param.payload_args.video_args.height, 1080);
    ASSERT_EQ(param.fps, 60);
    ASSERT_EQ(param.payload_args.video_args.fps, 60);
    ASSERT_EQ(param.pix_fmt, PIX_FMT_YUV422PLANAR10LE);
    ASSERT_EQ(param.payload_args.video_args.pix_fmt, PIX_FMT_YUV422PLANAR10LE);

    cfg.pixel_format = MESH_VIDEO_PIXEL_FORMAT_V210;
    mesh_apply_connection_config_video(conn, &cfg);
    err = ctx.parse_payload_config(&param);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(param.pix_fmt, PIX_FMT_V210);
    ASSERT_EQ(param.payload_args.video_args.pix_fmt, PIX_FMT_V210);

    cfg.pixel_format = MESH_VIDEO_PIXEL_FORMAT_YUV422RFC4175BE10;
    mesh_apply_connection_config_video(conn, &cfg);
    err = ctx.parse_payload_config(&param);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(param.pix_fmt, PIX_FMT_YUV422RFC4175BE10);
    ASSERT_EQ(param.payload_args.video_args.pix_fmt, PIX_FMT_YUV422RFC4175BE10);
}

/**
 * Test parsing audio payload configuration
 */
TEST(APITests_MeshConnection, Test_ParseAudioPayloadConfig) {
    MeshConfig_Audio cfg = {
        .channels = 2,
        .sample_rate = MESH_AUDIO_SAMPLE_RATE_44100,
        .format = MESH_AUDIO_FORMAT_PCM_S24BE,
        .packet_time = MESH_AUDIO_PACKET_TIME_1_09MS,
    };
    mesh::ClientContext mc_ctx(nullptr);
    mesh::ConnectionContext ctx(&mc_ctx);
    MeshConnection *conn;
    mcm_conn_param param;
    int err;

    conn = (MeshConnection *)&ctx;

    mesh_apply_connection_config_audio(conn, &cfg);
    err = ctx.parse_payload_config(&param);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(param.payload_args.audio_args.type, AUDIO_TYPE_FRAME_LEVEL);
    ASSERT_EQ(param.payload_args.audio_args.channel, 2);
    ASSERT_EQ(param.payload_args.audio_args.sampling, AUDIO_SAMPLING_44K);
    ASSERT_EQ(param.payload_args.audio_args.format, AUDIO_FMT_PCM24);
    ASSERT_EQ(param.payload_args.audio_args.ptime, AUDIO_PTIME_1_09MS);

    cfg.packet_time = MESH_AUDIO_PACKET_TIME_0_14MS;
    mesh_apply_connection_config_audio(conn, &cfg);
    err = ctx.parse_payload_config(&param);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(param.payload_args.audio_args.ptime, AUDIO_PTIME_0_14MS);

    cfg.packet_time = MESH_AUDIO_PACKET_TIME_0_09MS;
    mesh_apply_connection_config_audio(conn, &cfg);
    err = ctx.parse_payload_config(&param);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(param.payload_args.audio_args.ptime, AUDIO_PTIME_0_09MS);

    cfg.sample_rate = MESH_AUDIO_SAMPLE_RATE_48000;
    cfg.packet_time = MESH_AUDIO_PACKET_TIME_1MS;
    mesh_apply_connection_config_audio(conn, &cfg);
    err = ctx.parse_payload_config(&param);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(param.payload_args.audio_args.sampling, AUDIO_SAMPLING_48K);
    ASSERT_EQ(param.payload_args.audio_args.ptime, AUDIO_PTIME_1MS);

    cfg.sample_rate = MESH_AUDIO_SAMPLE_RATE_96000;
    mesh_apply_connection_config_audio(conn, &cfg);
    err = ctx.parse_payload_config(&param);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(param.payload_args.audio_args.sampling, AUDIO_SAMPLING_96K);

    cfg.packet_time = MESH_AUDIO_PACKET_TIME_125US;
    mesh_apply_connection_config_audio(conn, &cfg);
    err = ctx.parse_payload_config(&param);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(param.payload_args.audio_args.ptime, AUDIO_PTIME_125US);

    cfg.packet_time = MESH_AUDIO_PACKET_TIME_250US;
    mesh_apply_connection_config_audio(conn, &cfg);
    err = ctx.parse_payload_config(&param);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(param.payload_args.audio_args.ptime, AUDIO_PTIME_250US);

    cfg.packet_time = MESH_AUDIO_PACKET_TIME_333US;
    mesh_apply_connection_config_audio(conn, &cfg);
    err = ctx.parse_payload_config(&param);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(param.payload_args.audio_args.ptime, AUDIO_PTIME_333US);

    cfg.packet_time = MESH_AUDIO_PACKET_TIME_4MS;
    mesh_apply_connection_config_audio(conn, &cfg);
    err = ctx.parse_payload_config(&param);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(param.payload_args.audio_args.ptime, AUDIO_PTIME_4MS);

    cfg.packet_time = MESH_AUDIO_PACKET_TIME_80US;
    mesh_apply_connection_config_audio(conn, &cfg);
    err = ctx.parse_payload_config(&param);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(param.payload_args.audio_args.ptime, AUDIO_PTIME_80US);

    cfg.format = MESH_AUDIO_FORMAT_PCM_S8;
    mesh_apply_connection_config_audio(conn, &cfg);
    err = ctx.parse_payload_config(&param);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(param.payload_args.audio_args.format, AUDIO_FMT_PCM8);

    cfg.format = MESH_AUDIO_FORMAT_PCM_S16BE;
    mesh_apply_connection_config_audio(conn, &cfg);
    err = ctx.parse_payload_config(&param);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_EQ(param.payload_args.audio_args.format, AUDIO_FMT_PCM16);
}

/**
 * Test negative scenarios of parsing payload configuration - invalid parameters
 */
TEST(APITests_MeshConnection, TestNegative_ParsePayloadConfigInval) {
    MeshConfig_Video video_cfg = { .pixel_format = 5 };
    MeshConfig_Audio audio_cfg = { .sample_rate = 3 };
    mesh::ClientContext mc_ctx(nullptr);
    mesh::ConnectionContext ctx(&mc_ctx);
    MeshConnection *conn;
    mcm_conn_param param;
    int err;

    conn = (MeshConnection *)&ctx;

    mesh_apply_connection_config_video(conn, &video_cfg);
    err = ctx.parse_payload_config(&param);
    ASSERT_EQ(err, -MESH_ERR_CONN_CONFIG_INVAL) << mesh_err2str(err);

    mesh_apply_connection_config_audio(conn, &audio_cfg);
    err = ctx.parse_payload_config(&param);
    ASSERT_EQ(err, -MESH_ERR_CONN_CONFIG_INVAL) << mesh_err2str(err);
}

/**
 * Test negative scenarios of parsing payload configuration - incompatible parameters
 */
TEST(APITests_MeshConnection, TestNegative_ParsePayloadConfigIncompat) {
    int i;

    MeshConfig_Audio cases[] = {
        {
            .sample_rate = MESH_AUDIO_SAMPLE_RATE_48000,
            .packet_time = MESH_AUDIO_PACKET_TIME_1_09MS,
        },
        {
            .sample_rate = MESH_AUDIO_SAMPLE_RATE_48000,
            .packet_time = MESH_AUDIO_PACKET_TIME_0_14MS,
        },
        {
            .sample_rate = MESH_AUDIO_SAMPLE_RATE_48000,
            .packet_time = MESH_AUDIO_PACKET_TIME_0_09MS,
        },
        {
            .sample_rate = MESH_AUDIO_SAMPLE_RATE_48000,
            .packet_time = 9,
        },
        {
            .sample_rate = MESH_AUDIO_SAMPLE_RATE_96000,
            .packet_time = MESH_AUDIO_PACKET_TIME_1_09MS,
        },
        {
            .sample_rate = MESH_AUDIO_SAMPLE_RATE_96000,
            .packet_time = MESH_AUDIO_PACKET_TIME_0_14MS,
        },
        {
            .sample_rate = MESH_AUDIO_SAMPLE_RATE_96000,
            .packet_time = MESH_AUDIO_PACKET_TIME_0_09MS,
        },
        {
            .sample_rate = MESH_AUDIO_SAMPLE_RATE_96000,
            .packet_time = 9,
        },
        {
            .sample_rate = MESH_AUDIO_SAMPLE_RATE_44100,
            .packet_time = MESH_AUDIO_PACKET_TIME_1MS,
        },
        {
            .sample_rate = MESH_AUDIO_SAMPLE_RATE_44100,
            .packet_time = MESH_AUDIO_PACKET_TIME_125US,
        },
        {
            .sample_rate = MESH_AUDIO_SAMPLE_RATE_44100,
            .packet_time = MESH_AUDIO_PACKET_TIME_250US,
        },
        {
            .sample_rate = MESH_AUDIO_SAMPLE_RATE_44100,
            .packet_time = MESH_AUDIO_PACKET_TIME_333US,
        },
        {
            .sample_rate = MESH_AUDIO_SAMPLE_RATE_44100,
            .packet_time = MESH_AUDIO_PACKET_TIME_4MS,
        },
        {
            .sample_rate = MESH_AUDIO_SAMPLE_RATE_44100,
            .packet_time = MESH_AUDIO_PACKET_TIME_80US,
        },
        {
            .sample_rate = MESH_AUDIO_SAMPLE_RATE_44100,
            .packet_time = 9,
        },
    };

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        MeshConfig_Audio *cfg = &cases[i];
        mesh::ClientContext mc_ctx(nullptr);
        mesh::ConnectionContext ctx(&mc_ctx);
        MeshConnection *conn;
        mcm_conn_param param;
        int err;

        conn = (MeshConnection *)&ctx;

        mesh_apply_connection_config_audio(conn, cfg);
        err = ctx.parse_payload_config(&param);
        ASSERT_EQ(err, -MESH_ERR_CONN_CONFIG_INCOMPAT) << mesh_err2str(err);
    }
}

/**
 * Test negative scenario of establishing a mesh connection with invalid config
 */
TEST(APITests_MeshConnection, TestNegative_EstablishConnectionConfigInval) {
    MeshConfig_Memif memif_cfg = {};
    MeshConfig_Video video_cfg = {};
    MeshConnection *conn = NULL;
    MeshClient *mc = NULL;
    int err;

    APITests_Setup();

    mesh_create_client(&mc, NULL);
    mesh_create_connection(mc, &conn);

    err = mesh_establish_connection(conn, MESH_CONN_KIND_SENDER);
    EXPECT_EQ(err, -MESH_ERR_CONN_CONFIG_INVAL) << mesh_err2str(err);

    mesh_apply_connection_config_memif(conn, &memif_cfg);
    err = mesh_establish_connection(conn, MESH_CONN_KIND_SENDER);
    EXPECT_EQ(err, -MESH_ERR_CONN_CONFIG_INVAL) << mesh_err2str(err);

    mesh_apply_connection_config_video(conn, &video_cfg);
    err = mesh_establish_connection(conn, 2);
    EXPECT_EQ(err, -MESH_ERR_CONN_CONFIG_INVAL) << mesh_err2str(err);

    mesh_delete_connection(&conn);
    mesh_delete_client(&mc);
}

/**
 * Test negative scenario of establishing a mesh connection with incompatible config
 */
TEST(APITests_MeshConnection, TestNegative_CreateConnectionConfigIncompat) {
    MeshConfig_Memif memif_cfg = {};
    MeshConfig_Audio audio_cfg = { 
        .sample_rate = MESH_AUDIO_SAMPLE_RATE_48000,
        .packet_time = MESH_AUDIO_PACKET_TIME_1_09MS,
    };
    MeshConnection *conn = NULL;
    MeshClient *mc = NULL;
    int err;

    APITests_Setup();

    mesh_create_client(&mc, NULL);
    mesh_create_connection(mc, &conn);

    mesh_apply_connection_config_memif(conn, &memif_cfg);
    mesh_apply_connection_config_audio(conn, &audio_cfg);
    err = mesh_establish_connection(conn, MESH_CONN_KIND_SENDER);
    EXPECT_EQ(err, -MESH_ERR_CONN_CONFIG_INCOMPAT) << mesh_err2str(err);

    mesh_delete_connection(&conn);
    mesh_delete_client(&mc);
}

/**
 * Test negative scenario of creating a mesh connection - nulled client and conn
 */
TEST(APITests_MeshConnection, TestNegative_CreateConnection_NulledClientAndConn) {
    int err;

    APITests_Setup();

    err = mesh_create_connection(NULL, NULL);
    EXPECT_EQ(err, -MESH_ERR_BAD_CLIENT_PTR) << mesh_err2str(err);
}

/**
 * Test negative scenario of creating a mesh connection - nulled conn
 */
TEST(APITests_MeshConnection, TestNegative_CreateConnection_NulledConn) {
    MeshConnection *conn = NULL;
    MeshClient *mc = NULL;
    int err;

    APITests_Setup();

    mesh_create_client(&mc, NULL);

    err = mesh_create_connection(mc, NULL);
    EXPECT_EQ(err, -MESH_ERR_BAD_CONN_PTR) << mesh_err2str(err);
    EXPECT_EQ(conn, (MeshConnection *)NULL);

    mesh_delete_client(&mc);
}

/**
 * Test creation of a mesh connection - two connections & max conn number
 */
TEST(APITests_MeshConnection, Test_CreateConnection_MaxConnNumber) {
    MeshConnection *conn = NULL;
    MeshClientConfig mc_cfg = { .max_conn_num = 1 };
    MeshClient *mc = NULL;
    int err;

    APITests_Setup();

    mesh_create_client(&mc, &mc_cfg);

    err = mesh_create_connection(mc, &conn);
    EXPECT_EQ(err, 0) << mesh_err2str(err);
    EXPECT_NE(conn, (MeshConnection *)NULL);
    if (err || !conn)
        goto exit;

    mesh_delete_connection(&conn);

    err = mesh_create_connection(mc, &conn);
    EXPECT_EQ(err, 0) << mesh_err2str(err);
    EXPECT_NE(conn, (MeshConnection *)NULL);
    if (err || !conn)
        goto exit;

    mesh_delete_connection(&conn);

exit:
    mesh_delete_client(&mc);
}

/**
 * Test negative scenario of creating a mesh connection - max conn number
 */
TEST(APITests_MeshConnection, TestNegative_CreateConnection_MaxConnNumber) {
    MeshConnection *conn = NULL;
    MeshClientConfig mc_cfg = { .max_conn_num = -1 };
    MeshClient *mc = NULL;
    int err;

    APITests_Setup();

    mesh_create_client(&mc, &mc_cfg);

    err = mesh_create_connection(mc, &conn);
    EXPECT_EQ(err, -MESH_ERR_MAX_CONN) << mesh_err2str(err);
    EXPECT_EQ(conn, (MeshConnection *)NULL);

    mesh_delete_client(&mc);
}

/**
 * Test negative scenario of shutting down a mesh connection - nulled conn
 */
TEST(APITests_MeshConnection, TestNegative_ShutdownConnection_NulledConn) {
    int err;

    APITests_Setup();

    err = mesh_shutdown_connection(NULL);
    ASSERT_EQ(err, -MESH_ERR_BAD_CONN_PTR) << mesh_err2str(err);
}

/**
 * Test negative scenario of deleting a mesh connection - nulled conn
 */
TEST(APITests_MeshConnection, TestNegative_DeleteConnection_NulledConn) {
    MeshConnection *conn = NULL;
    int err;

    APITests_Setup();

    err = mesh_delete_connection(NULL);
    ASSERT_EQ(err, -MESH_ERR_BAD_CONN_PTR) << mesh_err2str(err);

    err = mesh_delete_connection(&conn);
    EXPECT_EQ(err, -MESH_ERR_BAD_CONN_PTR) << mesh_err2str(err);
    EXPECT_EQ(conn, (MeshConnection *)NULL);
}

/**
 * Test getting and putting of a mesh buffer
 */
TEST(APITests_MeshBuffer, Test_GetPutBuffer) {
    MeshConfig_Memif memif_cfg = {};
    MeshConfig_Video video_cfg = {};
    MeshConnection *conn = NULL;
    MeshBuffer *buf = NULL;
    MeshClient *mc = NULL;
    int err;

    APITests_Setup();

    err = mesh_create_client(&mc, NULL);
    ASSERT_EQ(err, 0) << mesh_err2str(err);
    ASSERT_NE(mc, (MeshClient *)NULL);

    err = mesh_create_connection(mc, &conn);
    EXPECT_EQ(err, 0) << mesh_err2str(err);
    EXPECT_NE(conn, (MeshConnection *)NULL);
    if (err || !conn)
        goto exit_delete_client;

    err = mesh_apply_connection_config_memif(conn, &memif_cfg);
    EXPECT_EQ(err, 0) << mesh_err2str(err);
    if (err)
        goto exit_delete_conn;

    err = mesh_apply_connection_config_video(conn, &video_cfg);
    EXPECT_EQ(err, 0) << mesh_err2str(err);
    if (err)
        goto exit_delete_conn;

    err = mesh_establish_connection(conn, MESH_CONN_KIND_SENDER);
    EXPECT_EQ(err, 0) << mesh_err2str(err);
    if (err)
        goto exit_delete_conn;

    /**
     * Case A - Get buffer with default timeout
     */
    err = mesh_get_buffer(conn, &buf);
    EXPECT_EQ(err, 0) << mesh_err2str(err);
    EXPECT_NE(buf, (MeshBuffer *)NULL);
    EXPECT_EQ(buf->conn, conn);
    EXPECT_EQ(buf->payload_ptr, (void *)NULL);
    EXPECT_EQ(buf->payload_len, 192); /* Magic number hardcoded in mock function */
    EXPECT_EQ(__last_timeout, -1);
    if (err || !buf)
        goto exit_delete_conn;

    err = mesh_put_buffer(&buf);
    EXPECT_EQ(err, 0) << mesh_err2str(err);
    EXPECT_EQ(buf, (MeshBuffer *)NULL);

    /**
     * Case B - Get buffer with an infinite timeout
     */
    err = mesh_get_buffer_timeout(conn, &buf, MESH_TIMEOUT_INFINITE);
    EXPECT_EQ(err, 0) << mesh_err2str(err);
    EXPECT_NE(buf, (MeshBuffer *)NULL);
    EXPECT_EQ(buf->conn, conn);
    EXPECT_EQ(buf->payload_ptr, (void *)NULL);
    EXPECT_EQ(buf->payload_len, 192); /* Magic number hardcoded in mock function */
    EXPECT_EQ(__last_timeout, -1);
    if (err || !buf)
        goto exit_delete_conn;

    err = mesh_put_buffer(&buf);
    EXPECT_EQ(err, 0) << mesh_err2str(err);
    EXPECT_EQ(buf, (MeshBuffer *)NULL);

    /**
     * Case C - Get buffer with a zero timeout
     */
    err = mesh_get_buffer_timeout(conn, &buf, MESH_TIMEOUT_ZERO);
    EXPECT_EQ(err, 0) << mesh_err2str(err);
    EXPECT_NE(buf, (MeshBuffer *)NULL);
    EXPECT_EQ(buf->conn, conn);
    EXPECT_EQ(buf->payload_ptr, (void *)NULL);
    EXPECT_EQ(buf->payload_len, 192); /* Magic number hardcoded in mock function */
    EXPECT_EQ(__last_timeout, 0);
    if (err || !buf)
        goto exit_delete_conn;

    err = mesh_put_buffer(&buf);
    EXPECT_EQ(err, 0) << mesh_err2str(err);
    EXPECT_EQ(buf, (MeshBuffer *)NULL);

    /**
     * Case D - Get buffer with a 5000ms timeout
     */
    err = mesh_get_buffer_timeout(conn, &buf, 5000);
    EXPECT_EQ(err, 0) << mesh_err2str(err);
    EXPECT_NE(buf, (MeshBuffer *)NULL);
    EXPECT_EQ(buf->conn, conn);
    EXPECT_EQ(buf->payload_ptr, (void *)NULL);
    EXPECT_EQ(buf->payload_len, 192); /* Magic number hardcoded in mock function */
    EXPECT_EQ(__last_timeout, 5000);
    if (err || !buf)
        goto exit_delete_conn;

    err = mesh_put_buffer(&buf);
    EXPECT_EQ(err, 0) << mesh_err2str(err);
    EXPECT_EQ(buf, (MeshBuffer *)NULL);

exit_delete_conn:
    err = mesh_delete_connection(&conn);
    EXPECT_EQ(err, 0) << mesh_err2str(err);
    EXPECT_EQ(conn, (MeshConnection *)NULL);

exit_delete_client:
    err = mesh_delete_client(&mc);
    EXPECT_EQ(err, 0) << mesh_err2str(err);
    EXPECT_EQ(mc, (MeshClient *)NULL);
}

/**
 * Test getting a mesh buffer when connection is closed
 */
TEST(APITests_MeshBuffer, Test_GetBufferConnClosed) {
    MeshConfig_Memif memif_cfg = {};
    MeshConfig_Video video_cfg = {};
    MeshConnection *conn = NULL;
    MeshBuffer *buf = NULL;
    MeshClient *mc = NULL;
    int err;

    APITests_Setup();

    mesh_create_client(&mc, NULL);
    mesh_create_connection(mc, &conn);
    mesh_apply_connection_config_memif(conn, &memif_cfg);
    mesh_apply_connection_config_video(conn, &video_cfg);
    mesh_establish_connection(conn, MESH_CONN_KIND_SENDER);

    /**
     * Magic number 12345 is used below as a timeout value to tell
     * the mock function to simulate that the connection is closed.
     */
    err = mesh_get_buffer_timeout(conn, &buf, 12345);
    EXPECT_EQ(err, -MESH_ERR_CONN_CLOSED) << mesh_err2str(err);
    EXPECT_EQ(buf, (MeshBuffer *)NULL);

    mesh_delete_connection(&conn);
    mesh_delete_client(&mc);
}

/**
 * Test getting a mesh buffer with default timeout
 */
TEST(APITests_MeshBuffer, Test_GetBufferDefaultTimeout) {
    MeshConfig_Memif memif_cfg = {};
    MeshConfig_Video video_cfg = {};
    MeshClientConfig mc_cfg = {};
    MeshConnection *conn = NULL;
    MeshBuffer *buf = NULL;
    MeshClient *mc = NULL;
    int err;

    APITests_Setup();

    /**
     * Case A - Get buffer with implicitly specified default timeout
     */
    mc_cfg.timeout_ms = 1000;
    err = mesh_create_client(&mc, &mc_cfg);
    ASSERT_EQ(err, 0) << mesh_err2str(err);

    err = mesh_create_connection(mc, &conn);
    EXPECT_EQ(err, 0) << mesh_err2str(err);
    if (err || !conn)
        goto exit;

    mesh_apply_connection_config_memif(conn, &memif_cfg);
    mesh_apply_connection_config_video(conn, &video_cfg);
    
    err = mesh_establish_connection(conn, MESH_CONN_KIND_SENDER);
    EXPECT_EQ(err, 0) << mesh_err2str(err);
    if (err || !conn)
        goto exit;

    err = mesh_get_buffer(conn, &buf);
    EXPECT_EQ(err, 0) << mesh_err2str(err);
    EXPECT_EQ(__last_timeout, 1000);

    mesh_put_buffer(&buf);
    mesh_delete_connection(&conn);
    mesh_delete_client(&mc);

    /**
     * Case B - Get buffer with explicitly specified default timeout
     */
    mc_cfg.timeout_ms = 2000;
    err = mesh_create_client(&mc, &mc_cfg);
    ASSERT_EQ(err, 0) << mesh_err2str(err);

    err = mesh_create_connection(mc, &conn);
    EXPECT_EQ(err, 0) << mesh_err2str(err);
    if (err || !conn)
        goto exit;

    mesh_apply_connection_config_memif(conn, &memif_cfg);
    mesh_apply_connection_config_video(conn, &video_cfg);
    
    err = mesh_establish_connection(conn, MESH_CONN_KIND_SENDER);
    EXPECT_EQ(err, 0) << mesh_err2str(err);
    if (err || !conn)
        goto exit;

    err = mesh_get_buffer_timeout(conn, &buf, MESH_TIMEOUT_DEFAULT);
    EXPECT_EQ(err, 0) << mesh_err2str(err);
    EXPECT_EQ(__last_timeout, 2000);

    mesh_put_buffer(&buf);
    mesh_delete_connection(&conn);

exit:
    mesh_delete_client(&mc);
}

/**
 * Test negative scenario of getting a mesh buffer - nulled conn and buffer
 */
TEST(APITests_MeshBuffer, TestNegative_GetBuffer_NulledConnAndBuf) {
    int err;

    APITests_Setup();

    err = mesh_get_buffer(NULL, NULL);
    EXPECT_EQ(err, -MESH_ERR_BAD_CONN_PTR) << mesh_err2str(err);
}

/**
 * Test negative scenario of getting a mesh buffer - nulled buffer
 */
TEST(APITests_MeshBuffer, TestNegative_GetBuffer_NulledBuf) {
    MeshConfig_Memif memif_cfg = {};
    MeshConfig_Video video_cfg = {};
    MeshConnection *conn;
    MeshClient *mc;
    int err;

    APITests_Setup();

    mesh_create_client(&mc, NULL);
    mesh_create_connection(mc, &conn);
    mesh_apply_connection_config_memif(conn, &memif_cfg);
    mesh_apply_connection_config_video(conn, &video_cfg);
    mesh_establish_connection(conn, MESH_CONN_KIND_SENDER);

    err = mesh_get_buffer(conn, NULL);
    EXPECT_EQ(err, -MESH_ERR_BAD_BUF_PTR) << mesh_err2str(err);

    mesh_delete_connection(&conn);
    mesh_delete_client(&mc);
}

/**
 * Test negative scenario of putting a mesh buffer - nulled buffer
 */
TEST(APITests_MeshBuffer, TestNegative_PutBuffer_NulledBuf) {
    int err;

    APITests_Setup();

    err = mesh_put_buffer(NULL);
    EXPECT_EQ(err, -MESH_ERR_BAD_BUF_PTR) << mesh_err2str(err);
}

/**
 * Test important constants
 */
TEST(APITests_MeshBuffer, Test_ImportantConstants) {
    EXPECT_EQ(MESH_SOCKET_PATH_SIZE, 108);
    EXPECT_EQ(MESH_IP_ADDRESS_SIZE, 253);

    EXPECT_EQ(MESH_ERR_BAD_CLIENT_PTR,       1000);
    EXPECT_EQ(MESH_ERR_BAD_CONN_PTR,         1001);
    EXPECT_EQ(MESH_ERR_BAD_CONFIG_PTR,       1002);
    EXPECT_EQ(MESH_ERR_BAD_BUF_PTR,          1003);
    EXPECT_EQ(MESH_ERR_CLIENT_CONFIG_INVAL,  1004);
    EXPECT_EQ(MESH_ERR_MAX_CONN,             1005);
    EXPECT_EQ(MESH_ERR_FOUND_ALLOCATED,      1006);
    EXPECT_EQ(MESH_ERR_CONN_FAILED,          1007);
    EXPECT_EQ(MESH_ERR_CONN_CONFIG_INVAL,    1008);
    EXPECT_EQ(MESH_ERR_CONN_CONFIG_INCOMPAT, 1009);
    EXPECT_EQ(MESH_ERR_CONN_CLOSED,          1010);
    EXPECT_EQ(MESH_ERR_TIMEOUT,              1011);
    EXPECT_EQ(MESH_ERR_NOT_IMPLEMENTED,      1012);

    EXPECT_EQ(MESH_TIMEOUT_DEFAULT,  -2);
    EXPECT_EQ(MESH_TIMEOUT_INFINITE, -1);
    EXPECT_EQ(MESH_TIMEOUT_ZERO,      0);
}
