/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <math.h>
#include <charconv>
#include <bsd/string.h>
#include <algorithm>
#include <arpa/inet.h>

#include "proxy_context.h"

ProxyContext::ProxyContext(void)
    : mRpcCtrlAddr("0.0.0.0:8001")
    , mTcpCtrlAddr("0.0.0.0:8002")
    , imtl_init_preparing(false), mSessionCount(0)
{
    mTcpCtrlPort = 8002;
}

ProxyContext::ProxyContext(std::string_view rpc_addr, std::string_view tcp_addr)
    : mRpcCtrlAddr(rpc_addr)
    , mTcpCtrlAddr(tcp_addr)
    , imtl_init_preparing(false), mSessionCount(0)
{
    auto colon = tcp_addr.find_first_of(":");
    if (colon >= tcp_addr.size() ||
        std::from_chars(tcp_addr.data() + colon + 1, tcp_addr.data() + tcp_addr.size(), mTcpCtrlPort).ec != std::errc())
    {
        ERROR("ProxyContext::ProxyContext(): Illegal TCP listen address.");
        throw;
    }

    pthread_mutex_init(&sessions_count_mutex_lock, NULL);
}

void ProxyContext::setRPCListenAddress(std::string_view addr)
{
    mRpcCtrlAddr = addr;
}

void ProxyContext::setTCPListenAddress(std::string_view addr)
{
    mTcpCtrlAddr = addr;
}

void ProxyContext::setDevicePort(std::string_view dev)
{
    mDevPort = dev;
}

void ProxyContext::setDataPlaneAddress(std::string_view ip)
{
    mDpAddress = ip;
}

void ProxyContext::setDataPlanePort(std::string_view port)
{
    mDpPort = port;
}

std::string ProxyContext::getRPCListenAddress(void)
{
    return mRpcCtrlAddr;
}

std::string ProxyContext::getTCPListenAddress(void)
{
    return mTcpCtrlAddr;
}

int ProxyContext::getTCPListenPort(void)
{
    return mTcpCtrlPort;
}

std::string ProxyContext::getDevicePort(void)
{
    return mDevPort;
}

std::string ProxyContext::getDataPlaneAddress(void)
{
    return mDpAddress;
}

std::string ProxyContext::getDataPlanePort(void)
{
    return mDpPort;
}

uint32_t ProxyContext::incrementMSessionCount(bool postIncrement=true)
{
    uint32_t retValue;
    pthread_mutex_lock(&this->sessions_count_mutex_lock);  /* lock to protect mSessionCount from change by multi-session simultaneously */
    if(postIncrement)
        retValue = (this->mSessionCount)++;
    else
        retValue = ++(this->mSessionCount);
    pthread_mutex_unlock(&this->sessions_count_mutex_lock);
    return retValue;
}

st_frame_fmt ProxyContext::getStFrameFmt(video_pixel_format mcm_frame_fmt)
{
    st_frame_fmt mtl_frame_fmt;
    switch(mcm_frame_fmt) {
        case PIX_FMT_NV12:
            mtl_frame_fmt = ST_FRAME_FMT_YUV420CUSTOM8;
            break;
        case PIX_FMT_YUV422P:
            mtl_frame_fmt = ST_FRAME_FMT_YUV422PLANAR8;
            break;
        case PIX_FMT_YUV444P_10BIT_LE:
            mtl_frame_fmt = ST_FRAME_FMT_YUV444PLANAR10LE;
            break;
        case PIX_FMT_RGB8:
            mtl_frame_fmt = ST_FRAME_FMT_RGB8;
            break;
        case PIX_FMT_YUV422P_10BIT_LE:
        default:
            mtl_frame_fmt = ST_FRAME_FMT_YUV422PLANAR10LE;
    }
    return mtl_frame_fmt;
}

void ProxyContext::ParseStInitParam(const mcm_conn_param* request, struct mtl_init_params* st_param)
{
    strlcpy(st_param->port[MTL_PORT_P], getDevicePort().c_str(), MTL_PORT_MAX_LEN);
    inet_pton(AF_INET, getDataPlaneAddress().c_str(), st_param->sip_addr[MTL_PORT_P]);
    st_param->pmd[MTL_PORT_P] = mtl_pmd_by_port_name(st_param->port[MTL_PORT_P]);
    st_param->num_ports = 1;
    st_param->flags = MTL_FLAG_BIND_NUMA;
    st_param->flags |= MTL_FLAG_TX_VIDEO_MIGRATE;
    st_param->flags |= MTL_FLAG_RX_VIDEO_MIGRATE;
    st_param->flags |= request->payload_mtl_flags_mask;
    st_param->pacing = (st21_tx_pacing_way) request->payload_mtl_pacing;
    st_param->log_level = MTL_LOG_LEVEL_DEBUG;
    st_param->priv = NULL;
    st_param->ptp_get_time_fn = NULL;
    // Native af_xdp have only 62 queues available
    if(st_param->pmd[MTL_PORT_P] == MTL_PMD_NATIVE_AF_XDP) {
      st_param->rx_queues_cnt[MTL_PORT_P] = 62;
      st_param->tx_queues_cnt[MTL_PORT_P] = 62;
    } else {
      st_param->rx_queues_cnt[MTL_PORT_P] = 128;
      st_param->tx_queues_cnt[MTL_PORT_P] = 128;
    }
    st_param->lcores = NULL;
    st_param->memzone_max = 9000;

    INFO("ProxyContext: ParseStInitParam(const mcm_conn_param* request, struct mtl_init_params* st_param)");
    INFO("num_ports : '%d'", st_param->num_ports);
    INFO("port      : '%s'", st_param->port[MTL_PORT_P]);
    INFO("port pmd  : '%d'", int(st_param->pmd[MTL_PORT_P]));
    INFO("sip_addr  : '%s'", getDataPlaneAddress().c_str());
    INFO("flags:    : '%ld'", st_param->flags);
    INFO("log_level : %d", st_param->log_level);
    if (st_param->lcores) {
        INFO("lcores    : %s", st_param->lcores);
    } else {
        INFO("lcores    : NULL");
    }
    INFO("rx_sessions_cnt_max : %d", st_param->rx_queues_cnt[MTL_PORT_P]);
    INFO("tx_sessions_cnt_max : %d", st_param->tx_queues_cnt[MTL_PORT_P]);
}

void ProxyContext::ParseSt20RxOps(const mcm_conn_param* request, struct st20p_rx_ops* ops_rx)
{
    static int session_id = 0;
    char session_name[NAME_MAX] = "";
    snprintf(session_name, NAME_MAX, "mcm_rx_st20_%d", session_id++);

    inet_pton(AF_INET, request->remote_addr.ip, ops_rx->port.ip_addr[MTL_PORT_P]);
    inet_pton(AF_INET, request->local_addr.ip, ops_rx->port.mcast_sip_addr[MTL_PORT_P]);

    ops_rx->port.udp_port[MTL_PORT_P] = atoi(request->local_addr.port);
    // ops_rx->port.udp_port[MTL_PORT_P] = RX_ST20_UDP_PORT;
    strlcpy(ops_rx->port.port[MTL_PORT_P], getDevicePort().c_str(), MTL_PORT_MAX_LEN);
    ops_rx->port.num_port = 1;
    if(request->payload_type_nr == 0 ) {
        ops_rx->port.payload_type = ST_APP_PAYLOAD_TYPE_VIDEO;
    } else {
        ops_rx->port.payload_type = request->payload_type_nr;
    }
    ops_rx->name = strdup(session_name);
    ops_rx->width = request->width;
    ops_rx->height = request->height;
    ops_rx->fps = st_frame_rate_to_st_fps((double)request->fps);
    ops_rx->transport_fmt = ST20_FMT_YUV_422_PLANAR10LE;
    ops_rx->output_fmt = getStFrameFmt(request->pix_fmt);
    ops_rx->device = ST_PLUGIN_DEVICE_AUTO;
    ops_rx->framebuff_cnt = 4;

    INFO("ProxyContext: %s...", __func__);
    INFO("port          : %s", ops_rx->port.port[MTL_PORT_P]);
    printf("INFO: ip_addr      :");
    for (int i = 0; i < MTL_IP_ADDR_LEN; ++i) {
        printf(" %d ", ops_rx->port.ip_addr[MTL_PORT_P][i]);
    }
    printf("\n");
    printf("INFO: mcast_sip_addr      :");
    for (int i = 0; i < MTL_IP_ADDR_LEN; ++i) {
        printf(" %d ", ops_rx->port.mcast_sip_addr[MTL_PORT_P][i]);
    }
    printf("\n");
    INFO("num_port      : %d", ops_rx->port.num_port);
    INFO("udp_port      : %d", ops_rx->port.udp_port[MTL_PORT_P]);
    INFO("payload_type  : %d", ops_rx->port.payload_type);
    INFO("name          : %s", ops_rx->name);
    INFO("width         : %d", ops_rx->width);
    INFO("height        : %d", ops_rx->height);
    INFO("fps           : %d", ops_rx->fps);
    INFO("transport_fmt : %d", ops_rx->transport_fmt);
    INFO("output_fmt    : %d", ops_rx->output_fmt);
    INFO("device        : %d", ops_rx->device);
    INFO("framebuff_cnt : %d", ops_rx->framebuff_cnt);
}

void ProxyContext::ParseMemIFParam(const mcm_conn_param* request, memif_ops_t& memif_ops)
{
    uint32_t sessionCount = incrementMSessionCount();
    std::string type_str = "";

    if (request->type == is_tx) {
        type_str = "tx";
    } else {
        type_str = "rx";
    }

    memif_ops.is_master = 1;
    memif_ops.interface_id = 0;
    snprintf(memif_ops.app_name, sizeof(memif_ops.app_name), "memif_%s_%d", type_str.c_str(), int(sessionCount));
    snprintf(memif_ops.interface_name, sizeof(memif_ops.interface_name), "memif_%s_%d", type_str.c_str(), int(sessionCount));
    snprintf(memif_ops.socket_path, sizeof(memif_ops.socket_path), "/run/mcm/media_proxy_%s_%d.sock", type_str.c_str(), int(sessionCount));
    memif_ops.m_session_count = ++sessionCount;
}

void ProxyContext::ParseSt20TxOps(const mcm_conn_param* request, struct st20p_tx_ops* ops_tx)
{
    static int session_id = 0;
    char session_name[NAME_MAX] = "";

    snprintf(session_name, NAME_MAX, "mcm_tx_st20_%d", session_id++);

    inet_pton(AF_INET, request->remote_addr.ip, ops_tx->port.dip_addr[MTL_PORT_P]);
    ops_tx->port.udp_port[MTL_PORT_P] = atoi(request->remote_addr.port);
    strlcpy(ops_tx->port.port[MTL_PORT_P], getDevicePort().c_str(), MTL_PORT_MAX_LEN);
    ops_tx->port.udp_src_port[MTL_PORT_P] = atoi(request->local_addr.port);
    ops_tx->port.num_port = 1;
    if(request->payload_type_nr == 0 ) {
        ops_tx->port.payload_type = ST_APP_PAYLOAD_TYPE_VIDEO;
    } else {
        ops_tx->port.payload_type = request->payload_type_nr;
    }
    ops_tx->name = strdup(session_name);
    ops_tx->width = request->width;
    ops_tx->height = request->height;
    ops_tx->fps = st_frame_rate_to_st_fps((double)request->fps);
    ops_tx->input_fmt = getStFrameFmt(request->pix_fmt);
    ops_tx->transport_fmt = ST20_FMT_YUV_422_PLANAR10LE;
    ops_tx->device = ST_PLUGIN_DEVICE_AUTO;
    ops_tx->framebuff_cnt = 4;

    INFO("ProxyContext: %s...", __func__);
    INFO("port          : %s", ops_tx->port.port[MTL_PORT_P]);
    printf("dip_addr    :");
    for (int i = 0; i < MTL_IP_ADDR_LEN; ++i) {
        printf(" %u", ops_tx->port.dip_addr[MTL_PORT_P][i]);
    }
    printf("\n");
    INFO("num_port      : %d", ops_tx->port.num_port);
    INFO("udp_port      : %d", ops_tx->port.udp_port[MTL_PORT_P]);
    INFO("udp_src_port  : %d", ops_tx->port.udp_src_port[MTL_PORT_P]);
    INFO("payload_type  : %d", ops_tx->port.payload_type);
    INFO("name          : %s", ops_tx->name);
    INFO("width         : %d", ops_tx->width);
    INFO("height        : %d", ops_tx->height);
    INFO("fps           : %d", ops_tx->fps);
    INFO("transport_fmt : %d", ops_tx->transport_fmt);
    INFO("input_fmt     : %d", ops_tx->input_fmt);
    INFO("device        : %d", ops_tx->device);
    INFO("framebuff_cnt : %d", ops_tx->framebuff_cnt);
}

void ProxyContext::ParseSt22TxOps(const mcm_conn_param* request, struct st22p_tx_ops* ops)
{
    static int session_id = 0;
    char session_name[NAME_MAX] = "";

    snprintf(session_name, NAME_MAX, "mcm_tx_st22_%d", session_id++);

    inet_pton(AF_INET, request->remote_addr.ip, ops->port.dip_addr[MTL_PORT_P]);
    ops->port.udp_port[MTL_PORT_P] = atoi(request->remote_addr.port);
    strlcpy(ops->port.port[MTL_PORT_P], getDevicePort().c_str(), MTL_PORT_MAX_LEN);
    ops->port.udp_src_port[MTL_PORT_P] = atoi(request->local_addr.port);
    ops->port.num_port = 1;
    if(request->payload_type_nr == 0 ) {
        ops->port.payload_type = ST_APP_PAYLOAD_TYPE_ST22;
    } else {
        ops->port.payload_type = request->payload_type_nr;
    }
    ops->name = strdup(session_name);
    ops->width = request->width;
    ops->height = request->height;
    ops->fps = st_frame_rate_to_st_fps((double)request->fps);
    ops->input_fmt = getStFrameFmt(request->pix_fmt);
    ops->device = ST_PLUGIN_DEVICE_AUTO;
    ops->framebuff_cnt = 4;
    ops->pack_type = ST22_PACK_CODESTREAM;
    ops->codec = ST22_CODEC_JPEGXS;
    ops->quality = ST22_QUALITY_MODE_SPEED;
    ops->codec_thread_cnt = 0;
    ops->codestream_size = ops->width * ops->height * 3 / 8;

    INFO("ProxyContext: %s...", __func__);
    INFO("port          : %s", ops->port.port[MTL_PORT_P]);
    printf("dip_addr    :");
    for (int i = 0; i < MTL_IP_ADDR_LEN; ++i) {
        printf(" %u", ops->port.dip_addr[MTL_PORT_P][i]);
    }
    printf("\n");
    INFO("num_port      : %d", ops->port.num_port);
    INFO("udp_port      : %d", ops->port.udp_port[MTL_PORT_P]);
    INFO("udp_src_port  : %d", ops->port.udp_src_port[MTL_PORT_P]);
    INFO("payload_type  : %d", ops->port.payload_type);
    INFO("name          : %s", ops->name);
    INFO("width         : %d", ops->width);
    INFO("height        : %d", ops->height);
    INFO("fps           : %d", ops->fps);
    // INFO("transport_fmt : %d", ops->transport_fmt);
    INFO("input_fmt     : %d", ops->input_fmt);
    INFO("device        : %d", ops->device);
    INFO("framebuff_cnt : %d", ops->framebuff_cnt);
}

void ProxyContext::ParseSt22RxOps(const mcm_conn_param* request, struct st22p_rx_ops* ops)
{
    static int session_id = 0;
    char session_name[NAME_MAX] = "";
    snprintf(session_name, NAME_MAX, "mcm_rx_st22_%d", session_id++);

    inet_pton(AF_INET, request->remote_addr.ip, ops->port.ip_addr[MTL_PORT_P]);
    inet_pton(AF_INET, request->local_addr.ip, ops->port.mcast_sip_addr[MTL_PORT_P]);
    ops->port.udp_port[MTL_PORT_P] = atoi(request->local_addr.port);

    // ops->port.udp_port[MTL_PORT_P] = RX_ST20_UDP_PORT;
    strlcpy(ops->port.port[MTL_PORT_P], getDevicePort().c_str(), MTL_PORT_MAX_LEN);
    ops->port.num_port = 1;
    if(request->payload_type_nr == 0 ) {
        ops->port.payload_type = ST_APP_PAYLOAD_TYPE_ST22;
    } else {
        ops->port.payload_type = request->payload_type_nr;
    }
    ops->name = strdup(session_name);
    ops->width = request->width;
    ops->height = request->height;
    ops->fps = st_frame_rate_to_st_fps((double)request->fps);
    ops->output_fmt = getStFrameFmt(request->pix_fmt);
    ops->device = ST_PLUGIN_DEVICE_AUTO;
    ops->framebuff_cnt = 4;
    ops->pack_type = ST22_PACK_CODESTREAM;
    ops->codec = ST22_CODEC_JPEGXS;
    ops->codec_thread_cnt = 0;
    ops->max_codestream_size = 0;

    INFO("ProxyContext: %s...", __func__);
    INFO("port          : %s", ops->port.port[MTL_PORT_P]);
    printf("INFO: ip_addr      :");
    for (int i = 0; i < MTL_IP_ADDR_LEN; ++i) {
        printf(" %d ", ops->port.ip_addr[MTL_PORT_P][i]);
    }
    printf("\n");
    printf("INFO: mcast_sip_addr      :");
    for (int i = 0; i < MTL_IP_ADDR_LEN; ++i) {
        printf(" %d ", ops->port.mcast_sip_addr[MTL_PORT_P][i]);
    }
    printf("\n");
    INFO("num_port      : %d", ops->port.num_port);
    INFO("udp_port      : %d", ops->port.udp_port[MTL_PORT_P]);
    INFO("payload_type  : %d", ops->port.payload_type);
    INFO("name          : %s", ops->name);
    INFO("width         : %d", ops->width);
    INFO("height        : %d", ops->height);
    INFO("fps           : %d", ops->fps);
    INFO("output_fmt    : %d", ops->output_fmt);
    INFO("device        : %d", ops->device);
    INFO("framebuff_cnt : %d", ops->framebuff_cnt);
}

void ProxyContext::ParseSt30TxOps(const mcm_conn_param* request, struct st30_tx_ops* ops)
{
    static int session_id = 0;
    char session_name[NAME_MAX] = "";

    snprintf(session_name, NAME_MAX, "mcm_tx_st30_%d", session_id++);

    inet_pton(AF_INET, request->remote_addr.ip, ops->dip_addr[MTL_PORT_P]);
    ops->udp_port[MTL_PORT_P] = atoi(request->remote_addr.port);
    strlcpy(ops->port[MTL_PORT_P], getDevicePort().c_str(), MTL_PORT_MAX_LEN);
    ops->num_port = 1;
    ops->payload_type = 111;
    ops->name = strdup(session_name);
    ops->framebuff_cnt = 4;

    ops->type = (st30_type)request->payload_args.audio_args.type;
    ops->fmt = (st30_fmt)request->payload_args.audio_args.format;
    ops->channel = request->payload_args.audio_args.channel;
    ops->sampling = (st30_sampling)request->payload_args.audio_args.sampling;
    ops->ptime = (st30_ptime)request->payload_args.audio_args.ptime;

    INFO("ProxyContext: %s...", __func__);
    INFO("port          : %s", ops->port[MTL_PORT_P]);
    printf("dip_addr    :");
    for (int i = 0; i < MTL_IP_ADDR_LEN; ++i) {
        printf(" %u", ops->dip_addr[MTL_PORT_P][i]);
    }
    printf("\n");
    INFO("num_port      : %d", ops->num_port);
    INFO("udp_port      : %d", ops->udp_port[MTL_PORT_P]);
    INFO("payload_type  : %d", ops->payload_type);
    INFO("name          : %s", ops->name);
    INFO("framebuff_cnt : %d", ops->framebuff_cnt);
}

void ProxyContext::ParseSt30RxOps(const mcm_conn_param* request, struct st30_rx_ops* ops)
{
    static int session_id = 0;
    char session_name[NAME_MAX] = "";
    snprintf(session_name, NAME_MAX, "mcm_rx_st30_%d", session_id++);

    inet_pton(AF_INET, request->remote_addr.ip, ops->ip_addr[MTL_PORT_P]);
    ops->udp_port[MTL_PORT_P] = atoi(request->local_addr.port);

    strlcpy(ops->port[MTL_PORT_P], getDevicePort().c_str(), MTL_PORT_MAX_LEN);
    ops->num_port = 1;
    ops->payload_type = 111;
    ops->name = strdup(session_name);
    ops->framebuff_cnt = 4;

    ops->type = (st30_type)request->payload_args.audio_args.type;
    ops->fmt = (st30_fmt)request->payload_args.audio_args.format;
    ops->channel = request->payload_args.audio_args.channel;
    ops->sampling = (st30_sampling)request->payload_args.audio_args.sampling;
    ops->ptime = (st30_ptime)request->payload_args.audio_args.ptime;

    INFO("ProxyContext: %s...", __func__);
    INFO("port          : %s", ops->port[MTL_PORT_P]);
    printf("INFO: ip_addr      :");
    for (int i = 0; i < MTL_IP_ADDR_LEN; ++i) {
        printf(" %d ", ops->ip_addr[MTL_PORT_P][i]);
    }
    printf("\n");
    INFO("num_port      : %d", ops->num_port);
    INFO("udp_port      : %d", ops->udp_port[MTL_PORT_P]);
    INFO("payload_type  : %d", ops->payload_type);
    INFO("name          : %s", ops->name);
    INFO("framebuff_cnt : %d", ops->framebuff_cnt);
}

void ProxyContext::ParseSt40TxOps(const mcm_conn_param* request, struct st40_tx_ops* ops)
{
    static int session_id = 0;
    char session_name[NAME_MAX] = "";

    snprintf(session_name, NAME_MAX, "mcm_tx_st40_%d", session_id++);

    inet_pton(AF_INET, request->remote_addr.ip, ops->dip_addr[MTL_PORT_P]);
    ops->udp_port[MTL_PORT_P] = atoi(request->remote_addr.port);
    strlcpy(ops->port[MTL_PORT_P], getDevicePort().c_str(), MTL_PORT_MAX_LEN);
    ops->num_port = 1;
    ops->payload_type = 113;
    ops->name = strdup(session_name);
    ops->framebuff_cnt = 4;

    ops->type = (st40_type)request->payload_args.anc_args.type;
    ops->fps = st_frame_rate_to_st_fps((double)request->payload_args.anc_args.fps);
    ops->rtp_ring_size = 1024;

    INFO("ProxyContext: %s...", __func__);
    INFO("port          : %s", ops->port[MTL_PORT_P]);
    printf("dip_addr    :");
    for (int i = 0; i < MTL_IP_ADDR_LEN; ++i) {
        printf(" %u", ops->dip_addr[MTL_PORT_P][i]);
    }
    printf("\n");
    INFO("num_port      : %d", ops->num_port);
    INFO("udp_port      : %d", ops->udp_port[MTL_PORT_P]);
    INFO("payload_type  : %d", ops->payload_type);
    INFO("name          : %s", ops->name);
    INFO("framebuff_cnt : %d", ops->framebuff_cnt);
    INFO("type          : %d", ops->type);
    INFO("fps           : %d", ops->fps);
}

void ProxyContext::ParseSt40RxOps(const mcm_conn_param* request, struct st40_rx_ops* ops)
{
    static int session_id = 0;
    char session_name[NAME_MAX] = "";
    snprintf(session_name, NAME_MAX, "mcm_rx_st40_%d", session_id++);

    inet_pton(AF_INET, request->remote_addr.ip, ops->ip_addr[MTL_PORT_P]);
    ops->udp_port[MTL_PORT_P] = atoi(request->local_addr.port);

    strlcpy(ops->port[MTL_PORT_P], getDevicePort().c_str(), MTL_PORT_MAX_LEN);
    ops->num_port = 1;
    ops->payload_type = 113;
    ops->rtp_ring_size = 1024;
    ops->name = strdup(session_name);

    INFO("ProxyContext: %s...", __func__);
    INFO("port          : %s", ops->port[MTL_PORT_P]);
    printf("INFO: ip_addr      :");
    for (int i = 0; i < MTL_IP_ADDR_LEN; ++i) {
        printf(" %d ", ops->ip_addr[MTL_PORT_P][i]);
    }
    printf("\n");
    INFO("num_port      : %d", ops->num_port);
    INFO("udp_port      : %d", ops->udp_port[MTL_PORT_P]);
    INFO("payload_type  : %d", ops->payload_type);
    INFO("name          : %s", ops->name);
}


int ProxyContext::RxStart_rdma(const mcm_conn_param *request)
{
    memif_ops_t memif_ops = { 0 };
    rdma_s_ops_t opts = { 0 };
    int ret;

    if (!mDevHandle_rdma) {
        ret = rdma_init(&mDevHandle_rdma);
        if (ret) {
            INFO("%s, Failed to initialize libfabric.", __func__);
            return -EINVAL;
        }
    }

    ParseMemIFParam(request, memif_ops);
    opts.transfer_size = request->payload_args.rdma_args.transfer_size;
    opts.dir = direction::RX;
    memcpy(&opts.remote_addr, &request->remote_addr, sizeof(request->remote_addr));
    memcpy(&opts.local_addr, &request->local_addr, sizeof(request->local_addr));

    rx_rdma_session *session_ptr = NULL;
    try {
        session_ptr = new rx_rdma_session(mDevHandle_rdma, &opts, &memif_ops);
    } catch (const exception &e) {
        ERROR("%s, Failed to create RX RDMA session.", __func__);
        ERROR("%s", e.what());
        return -1;
    }

    session_ptr->payload_type = request->payload_type;
    session_ptr->id = memif_ops.m_session_count;
    INFO("%s, session id: %d", __func__, session_ptr->id);
    session_ptr->type = RX;
    mDpCtx.push_back(session_ptr);

    return session_ptr->id;
}

int ProxyContext::RxStart_mtl(const mcm_conn_param *request)
{
    INFO("ProxyContext: RxStart(const mcm_conn_param* request)");
    struct st20p_rx_ops opts = { 0 };
    memif_ops_t memif_ops = { 0 };
    int ret;

    /*add lock to protect MTL library initialization to aviod being called by multi-session simultaneously*/
    if (!mDevHandle && imtl_init_preparing == false) {

        imtl_init_preparing = true;
        struct mtl_init_params st_param = { 0 };

        /* set default parameters */
        ParseStInitParam(request, &st_param);

        mDevHandle = inst_init(&st_param);
        if (mDevHandle == NULL) {
            ERROR("%s, Failed to initialize MTL.", __func__);
            return -1;
        } else {
            imtl_init_preparing = false;
        }
    }

    if (mDevHandle == NULL) {
        ERROR("%s, Failed to initialize MTL for RxStart function.", __func__);
        return -1;
    }

    ParseMemIFParam(request, memif_ops);

    session *session_ptr = NULL;
    try {
        switch (request->payload_type) {
        case PAYLOAD_TYPE_ST20_VIDEO: {
            struct st20p_rx_ops opts = {};
            ParseSt20RxOps(request, &opts);
            session_ptr = new rx_st20_mtl_session(mDevHandle, &opts, &memif_ops);
            break;
        }
        case PAYLOAD_TYPE_ST22_VIDEO: {
            struct st22p_rx_ops opts = {};
            ParseSt22RxOps(request, &opts);
            session_ptr = new rx_st22_mtl_session(mDevHandle, &opts, &memif_ops);
            break;
        }
        case PAYLOAD_TYPE_ST30_AUDIO: {
            struct st30_rx_ops opts = {};
            ParseSt30RxOps(request, &opts);
            session_ptr = new rx_st30_mtl_session(mDevHandle, &opts, &memif_ops);
            break;
        }
        case PAYLOAD_TYPE_ST40_ANCILLARY:
        default: {
            ERROR("Unsupported payload\n");
            return -1;
        }
        }
    } catch (const exception &e) {
        ERROR("%s, Failed to create RX MTL session.", __func__);
        ERROR("%s", e.what());
        return -1;
    }

    session_ptr->payload_type = request->payload_type;
    session_ptr->id = memif_ops.m_session_count;
    INFO("%s, session id: %d", __func__, session_ptr->id);
    session_ptr->type = RX;
    mDpCtx.push_back(session_ptr);

    return (session_ptr->id);
}

int ProxyContext::RxStart(const mcm_conn_param *request)
{
    if (request->payload_type == PAYLOAD_TYPE_RDMA_VIDEO)
        return RxStart_rdma(request);

    return RxStart_mtl(request);
}

int ProxyContext::TxStart_rdma(const mcm_conn_param *request)
{
    memif_ops_t memif_ops = { 0 };
    rdma_s_ops_t opts = { 0 };
    int ret;

    if (!mDevHandle_rdma) {
        ret = rdma_init(&mDevHandle_rdma);
        if (ret) {
            INFO("%s, Failed to initialize libfabric.", __func__);
            return -EINVAL;
        }
    }

    ParseMemIFParam(request, memif_ops);
    opts.dir = direction::TX;
    opts.transfer_size = request->payload_args.rdma_args.transfer_size;
    memcpy(&opts.remote_addr, &request->remote_addr, sizeof(request->remote_addr));
    memcpy(&opts.local_addr, &request->local_addr, sizeof(request->local_addr));

    tx_rdma_session *session_ptr = NULL;
    try {
        session_ptr = new tx_rdma_session(mDevHandle_rdma, &opts, &memif_ops);
    } catch (const exception &e) {
        ERROR("%s, Failed to create TX RDMA session.", __func__);
        ERROR("%s", e.what());
        return -1;
    }    

    session_ptr->payload_type = request->payload_type;
    session_ptr->id = memif_ops.m_session_count;
    INFO("%s, session id: %d", __func__, session_ptr->id);
    session_ptr->type = TX;
    mDpCtx.push_back(session_ptr);

    return session_ptr->id;
}

int ProxyContext::TxStart_mtl(const mcm_conn_param *request)
{
    INFO("ProxyContext: TxStart(const mcm_conn_param* request)");
    memif_ops_t memif_ops = { 0 };

    /* add lock to protect MTL library initialization to avoid being called by multi-session simultaneously */
    if (mDevHandle == NULL && imtl_init_preparing == false) {

        imtl_init_preparing = true;

        struct mtl_init_params st_param = { 0 };

        /* set default parameters */
        ParseStInitParam(request, &st_param);

        mDevHandle = inst_init(&st_param);
        if (mDevHandle == NULL) {
            ERROR("%s, Failed to initialize MTL.", __func__);
            return -1;
        } else {
            imtl_init_preparing = false;
        }
    }

    if (mDevHandle == NULL) {
        ERROR("%s, Failed to initialize MTL for TxStart function.", __func__);
        return -1;
    }

    ParseMemIFParam(request, memif_ops);

    session *session_ptr = NULL;
    try {
        switch (request->payload_type) {
        case PAYLOAD_TYPE_ST20_VIDEO: {
            struct st20p_tx_ops opts = {};
            ParseSt20TxOps(request, &opts);
            session_ptr = new tx_st20_mtl_session(mDevHandle, &opts, &memif_ops);
            break;
        }
        case PAYLOAD_TYPE_ST22_VIDEO: {
            struct st22p_tx_ops opts = {};
            ParseSt22TxOps(request, &opts);
            session_ptr = new tx_st22_mtl_session(mDevHandle, &opts, &memif_ops);
            break;
        }
        case PAYLOAD_TYPE_ST30_AUDIO: {
            struct st30_tx_ops opts = {};
            ParseSt30TxOps(request, &opts);
            session_ptr = new tx_st30_mtl_session(mDevHandle, &opts, &memif_ops);
            break;
        }
        case PAYLOAD_TYPE_ST40_ANCILLARY:
        default: {
            ERROR("Unsupported payload\n");
            return -1;
        }
        }
    } catch (const exception &e) {
        ERROR("%s, Failed to create TX MTL session.", __func__);
        ERROR("%s", e.what());
        return -1;
    }

    session_ptr->payload_type = request->payload_type;
    session_ptr->id = memif_ops.m_session_count;
    INFO("%s, session id: %d", __func__, session_ptr->id);
    session_ptr->type = TX;
    mDpCtx.push_back(session_ptr);

    return (session_ptr->id);
}

int ProxyContext::TxStart(const mcm_conn_param *request)
{
    if (request->payload_type == PAYLOAD_TYPE_RDMA_VIDEO)
        return TxStart_rdma(request);

    return TxStart_mtl(request);
}

int ProxyContext::Stop(const int32_t session_id)
{
    int ret = 0;
    auto ctx = std::find_if(mDpCtx.begin(), mDpCtx.end(),
                            [session_id](auto it) { return it->id == session_id; });
    if (ctx != mDpCtx.end()) {
        INFO("%s, Stop session ID: %d", __func__, session_id);

        session *session_ptr = *ctx;
        session_ptr->session_stop();
        delete session_ptr;
        mDpCtx.erase(ctx);
    } else {
        ERROR("%s, Illegal session ID: %d", __func__, session_id);
        ret = -1;
    }

    return ret;
}
