/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <math.h>
#include <charconv>
#include <bsd/string.h>

#include "proxy_context.h"
#include <mtl/mtl_sch_api.h>
#include "api_server_grpc.h"

ProxyContext::ProxyContext(void)
    : mRpcCtrlAddr("0.0.0.0:8001")
    , mTcpCtrlAddr("0.0.0.0:8002")
    , schs_ready(false), imtl_init_preparing(false), mSessionCount(0)
{
    mTcpCtrlPort = 8002;
}

ProxyContext::ProxyContext(std::string_view rpc_addr, std::string_view tcp_addr)
    : mRpcCtrlAddr(rpc_addr)
    , mTcpCtrlAddr(tcp_addr)
    , schs_ready(false), imtl_init_preparing(false), mSessionCount(0)
{
    auto colon = tcp_addr.find_first_of(":");
    if (colon >= tcp_addr.size() ||
        std::from_chars(tcp_addr.data() + colon + 1, tcp_addr.data() + tcp_addr.size(), mTcpCtrlPort).ec != std::errc())
    {
        ERROR("ProxyContext::ProxyContext(): Illegal TCP listen address.");
        throw;
    }

    st_pthread_mutex_init(&sessions_count_mutex_lock, NULL);
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
    st_pthread_mutex_lock(&this->sessions_count_mutex_lock);  /* lock to protect mSessionCount from change by multi-session simultaneously */
    if(postIncrement)
        retValue = (this->mSessionCount)++;
    else
        retValue = ++(this->mSessionCount);
    st_pthread_mutex_unlock(&this->sessions_count_mutex_lock);
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

void ProxyContext::ParseStInitParam(const TxControlRequest* request, struct mtl_init_params* st_param)
{
    StInit init = request->st_init();
    st_param->num_ports = init.number_ports();

    std::string port_p = init.primary_port();
    strlcpy(st_param->port[MTL_PORT_P], port_p.c_str(), MTL_PORT_MAX_LEN);

    for (int i = 0; i < init.primary_sip_addr_size(); ++i) {
        st_param->sip_addr[MTL_PORT_P][i] = init.primary_sip_addr(i);
    }

    st_param->pmd[MTL_PORT_P] = mtl_pmd_by_port_name(st_param->port[MTL_PORT_P]);
    st_param->flags = init.flags();
    st_param->log_level = (enum mtl_log_level)init.log_level();
    st_param->priv = NULL;
    st_param->ptp_get_time_fn = NULL;
    st_param->rx_queues_cnt[MTL_PORT_P] = init.rx_sessions_cnt_max();
    st_param->tx_queues_cnt[MTL_PORT_P] = init.tx_sessions_cnt_max();
    if (init.logical_cores().empty()) {
        st_param->lcores = NULL;
    } else {
        st_param->lcores = (char*)init.logical_cores().c_str();
    }

    INFO("ProxyContext: ParseStInitParam(const TxControlRequest* request, struct mtl_init_params* st_param)");
    INFO("num_ports : %d", st_param->num_ports);
    INFO("port      : %s", st_param->port[MTL_PORT_P]);
    INFO("sip_addr  :");
    for (int i = 0; i < init.primary_sip_addr_size(); ++i) {
        INFO(" %d, ", st_param->sip_addr[MTL_PORT_P][i]);
    }
    INFO("\nflags: %ld", st_param->flags);
    INFO("log_level : %d", st_param->log_level);
    if (st_param->lcores) {
        INFO("lcores    : %s", st_param->lcores);
    } else {
        INFO("lcores    : NULL");
    }
    INFO("rx_sessions_cnt_max : %d", st_param->rx_queues_cnt[MTL_PORT_P]);
    INFO("tx_sessions_cnt_max : %d", st_param->tx_queues_cnt[MTL_PORT_P]);
}

void ProxyContext::ParseStInitParam(const RxControlRequest* request, struct mtl_init_params* st_param)
{
    StInit init = request->st_init();
    st_param->num_ports = init.number_ports();

    std::string port_p = init.primary_port();
    strlcpy(st_param->port[MTL_PORT_P], port_p.c_str(), MTL_PORT_MAX_LEN);

    for (int i = 0; i < init.primary_sip_addr_size(); ++i) {
        st_param->sip_addr[MTL_PORT_P][i] = init.primary_sip_addr(i);
    }

    st_param->pmd[MTL_PORT_P] = mtl_pmd_by_port_name(st_param->port[MTL_PORT_P]);
    st_param->flags = init.flags();
    st_param->log_level = (enum mtl_log_level)init.log_level();
    st_param->priv = NULL;
    st_param->ptp_get_time_fn = NULL;
    st_param->rx_queues_cnt[MTL_PORT_P] = init.rx_sessions_cnt_max();
    st_param->tx_queues_cnt[MTL_PORT_P] = init.tx_sessions_cnt_max();
    if (init.logical_cores().empty()) {
        st_param->lcores = NULL;
    } else {
        st_param->lcores = (char*)init.logical_cores().c_str();
    }

    INFO("ProxyContext: ParseStInitParam(const RxControlRequest* request, struct mtl_init_params* st_param)");
    INFO("num_ports : %d", st_param->num_ports);
    INFO("port      : %s", st_param->port[MTL_PORT_P]);
    INFO("sip_addr  :");
    for (int i = 0; i < init.primary_sip_addr_size(); ++i) {
        INFO(" %d, ", st_param->sip_addr[MTL_PORT_P][i]);
    }
    INFO("\nflags: %ld", st_param->flags);
    INFO("log_level : %d", st_param->log_level);
    if (st_param->lcores) {
        INFO("lcores    : %s", st_param->lcores);
    } else {
        INFO("lcores    : NULL");
    }
    INFO("rx_sessions_cnt_max : %d", st_param->rx_queues_cnt[MTL_PORT_P]);
    INFO("tx_sessions_cnt_max : %d", st_param->tx_queues_cnt[MTL_PORT_P]);
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

void ProxyContext::ParseMemIFParam(const TxControlRequest* request, memif_ops_t& memif_ops)
{
    strlcpy(memif_ops.app_name, request->memif_ops().app_name().c_str(), sizeof(memif_ops.app_name));
    strlcpy(memif_ops.interface_name, request->memif_ops().interface_name().c_str(), sizeof(memif_ops.interface_name));
    strlcpy(memif_ops.socket_path, request->memif_ops().socket_path().c_str(), sizeof(memif_ops.socket_path));
}

void ProxyContext::ParseMemIFParam(const RxControlRequest* request, memif_ops_t& memif_ops)
{
    strlcpy(memif_ops.app_name, request->memif_ops().app_name().c_str(), sizeof(memif_ops.app_name));
    strlcpy(memif_ops.interface_name, request->memif_ops().interface_name().c_str(), sizeof(memif_ops.interface_name));
    strlcpy(memif_ops.socket_path, request->memif_ops().socket_path().c_str(), sizeof(memif_ops.socket_path));
}

void ProxyContext::ParseSt20RxOps(const RxControlRequest* request, struct st20p_rx_ops* ops_rx)
{
    St20pRxOps st20_rx = request->st20_rx();
    StRxPort rx_port = st20_rx.rx_port();

    std::string port = rx_port.port();
    strlcpy(ops_rx->port.port[MTL_PORT_P], port.c_str(), MTL_PORT_MAX_LEN);
    for (int i = 0; i < rx_port.sip_addr_size(); ++i) {
        ops_rx->port.ip_addr[MTL_PORT_P][i] = rx_port.sip_addr(i);
    }

    ops_rx->port.num_port = rx_port.number_ports();
    ops_rx->port.udp_port[MTL_PORT_P] = rx_port.udp_port();
    ops_rx->port.payload_type = rx_port.payload_type();

    ops_rx->name = st20_rx.name().c_str();
    ops_rx->width = st20_rx.width();
    ops_rx->height = st20_rx.height();
    ops_rx->fps = (enum st_fps)st20_rx.fps();
    ops_rx->transport_fmt = (enum st20_fmt)st20_rx.transport_fmt();
    ops_rx->output_fmt = (enum st_frame_fmt)st20_rx.output_fmt();
    ops_rx->device = (enum st_plugin_device)st20_rx.device();
    ops_rx->framebuff_cnt = st20_rx.framebuffer_cnt();

    INFO("ProxyContext: %s...", __func__);
    INFO("port          : %s", ops_rx->port.port[MTL_PORT_P]);
    printf("INFO: ip_addr      :");
    for (int i = 0; i < rx_port.sip_addr_size(); ++i) {
        printf(" %d ", ops_rx->port.ip_addr[MTL_PORT_P][i]);
    }
    printf("\n");
    printf("INFO: ip_addr      :");
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

void ProxyContext::ParseSt20TxOps(const TxControlRequest* request, struct st20p_tx_ops* ops_tx)
{
    St20pTxOps st20_tx = request->st20_tx();
    StTxPort tx_port = st20_tx.tx_port();

    std::string port = tx_port.port();
    strlcpy(ops_tx->port.port[MTL_PORT_P], port.c_str(), MTL_PORT_MAX_LEN);
    for (int i = 0; i < tx_port.dip_addr_size(); ++i) {
        ops_tx->port.dip_addr[MTL_PORT_P][i] = tx_port.dip_addr(i);
    }

    ops_tx->port.num_port = tx_port.number_ports();
    ops_tx->port.udp_port[MTL_PORT_P] = tx_port.udp_port();
    ops_tx->port.payload_type = tx_port.payload_type();
    ops_tx->name = st20_tx.name().c_str();
    ops_tx->width = st20_tx.width();
    ops_tx->height = st20_tx.height();
    ops_tx->fps = (enum st_fps)st20_tx.fps();
    ops_tx->transport_fmt = (enum st20_fmt)st20_tx.transport_fmt();
    ops_tx->input_fmt = (enum st_frame_fmt)st20_tx.input_fmt();
    ops_tx->device = (enum st_plugin_device)st20_tx.device();
    ops_tx->framebuff_cnt = st20_tx.framebuffer_cnt();

    INFO("ProxyContext: %s...", __func__);
    INFO("port          : %s", ops_tx->port.port[MTL_PORT_P]);
    printf("INFO: dip_addr      :");
    for (int i = 0; i < tx_port.dip_addr_size(); ++i) {
        printf(" %d ", ops_tx->port.dip_addr[MTL_PORT_P][i]);
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

int ProxyContext::RxStart(const RxControlRequest* request)
{
    INFO("ProxyContext: RxStart(const RxControlRequest* request)");
    dp_session_context_t *st_ctx = NULL;
    rx_session_context_t* rx_ctx = NULL;
    struct st20p_rx_ops opts = { 0 };
    memif_ops_t memif_ops = { 0 };

    if (mDevHandle == NULL) {
        struct mtl_init_params st_param = {};
        /* set default parameters */
        ParseStInitParam(request, &st_param);

        mDevHandle = inst_init(&st_param);
        if (mDevHandle == NULL) {
            INFO("%s, Failed to initialize MTL.", __func__);
            return -1;
        }
    }

    ParseSt20RxOps(request, &opts);

    ParseMemIFParam(request, memif_ops);

    rx_ctx = mtl_st20p_rx_session_create(mDevHandle, &opts, &memif_ops);
    if (rx_ctx == NULL) {
        INFO("%s, Failed to create RX session.", __func__);
        return -1;
    }

    st_ctx = new (dp_session_context_t);
    st_ctx->id = incrementMSessionCount();
    st_ctx->type = RX;
    st_ctx->rx_session = rx_ctx;

    st_ctx->registry_id = gRPC_RegisterConnection();

    mDpCtx.push_back(st_ctx);

    /* TODO: to be removed later. */
    mRxCtx.push_back(rx_ctx);

    return (st_ctx->id);
}

int ProxyContext::TxStart(const TxControlRequest* request)
{
    INFO("ProxyContext: TxStart(const TxControlRequest* request)");
    dp_session_context_t *st_ctx = NULL;
    tx_session_context_t* tx_ctx = NULL;
    struct st20p_tx_ops opts = { 0 };
    memif_ops_t memif_ops = { 0 };

    if (!mDevHandle) {
        struct mtl_init_params st_param = {};
        /* set default parameters */
        ParseStInitParam(request, &st_param);

        mDevHandle = inst_init(&st_param);
        if (!mDevHandle) {
            INFO("%s, Failed to initialize MTL.", __func__);
            return -1;
        }
    }

    ParseSt20TxOps(request, &opts);

    ParseMemIFParam(request, memif_ops);

    tx_ctx = mtl_st20p_tx_session_create(mDevHandle, &opts, &memif_ops);
    if (tx_ctx == NULL) {
        INFO("%s, Failed to create TX session.", __func__);
        return -1;
    }

    st_ctx = new (dp_session_context_t);
    st_ctx->id = incrementMSessionCount();
    st_ctx->type = TX;
    st_ctx->tx_session = tx_ctx;

    st_ctx->registry_id = gRPC_RegisterConnection();

    mDpCtx.push_back(st_ctx);

    /* TODO: to be removed later. */
    mTxCtx.push_back(tx_ctx);

    return (st_ctx->id);
}

int ProxyContext::RxStart_rdma(const mcm_conn_param *request)
{
    rx_rdma_session_context_t *rx_ctx = NULL;
    dp_session_context_t *dp_ctx = NULL;
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
    dp_ctx = new (dp_session_context_t);
    ParseMemIFParam(request, memif_ops);
    opts.transfer_size = request->payload_args.rdma_args.transfer_size;
    opts.dir = direction::RX;
    memcpy(&opts.remote_addr, &request->remote_addr, sizeof(request->remote_addr));
    memcpy(&opts.local_addr, &request->local_addr, sizeof(request->local_addr));
    rx_ctx = rdma_rx_session_create(mDevHandle_rdma, &opts, &memif_ops);
    if (!rx_ctx) {
        INFO("%s, Failed to create RDMA session.", __func__);
        delete dp_ctx;
        return -EINVAL;
    }
    dp_ctx->payload_type = request->payload_type;
    dp_ctx->rx_rdma_session = rx_ctx;
    dp_ctx->id = memif_ops.m_session_count;
    INFO("%s, session id: %d", __func__, dp_ctx->id);
    dp_ctx->type = RX;

    dp_ctx->registry_id = gRPC_RegisterConnection();

    mDpCtx.push_back(dp_ctx);

    return dp_ctx->id;
}

int ProxyContext::RxStart_mtl(const mcm_conn_param *request)
{
    INFO("ProxyContext: RxStart(const mcm_conn_param* request)");
    dp_session_context_t *st_ctx = NULL;
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
            /*udp pool*/
            if (schs_ready == false) {
                struct mtl_sch_ops sch_ops;
                memset(&sch_ops, 0x0, sizeof(sch_ops));

                sch_ops.nb_tasklets = TASKLETS;

                for (int i = 0; i < SCH_CNT; i++) {
                    char sch_name[32];

                    snprintf(sch_name, sizeof(sch_name), "sch_udp_%d", i);
                    sch_ops.name = sch_name;
                    mtl_sch_handle sch = mtl_sch_create(mDevHandle, &sch_ops);
                    if (sch == NULL) {
                        INFO("%s, error: schduler create fail.", __func__);
                        break;
                    }
                    ret = mtl_sch_start(sch);
                    INFO("%s, start schduler %d.", __func__, i);
                    if (ret < 0) {
                        INFO("%s, fail to start schduler %d.", __func__, i);
                        ret = mtl_sch_free(sch);
                        break;
                    }
                    schs[i] = sch;
                }
                schs_ready = true;
            }

            imtl_init_preparing = false;
        }
    }

    if (mDevHandle == NULL) {
        ERROR("%s, Failed to initialize MTL for RxStart function.", __func__);
        return -1;
    }

    st_ctx = new (dp_session_context_t);

    ParseMemIFParam(request, memif_ops);
    switch (request->payload_type) {
    case PAYLOAD_TYPE_ST22_VIDEO: {
        rx_st22p_session_context_t* rx_ctx = NULL;
        struct st22p_rx_ops opts = {};

        ParseSt22RxOps(request, &opts);
        rx_ctx = mtl_st22p_rx_session_create(mDevHandle, &opts, &memif_ops);
        if (rx_ctx == NULL) {
            INFO("%s, Failed to create RX session.", __func__);
            delete st_ctx;
            return -1;
        }

        st_ctx->rx_st22p_session = rx_ctx;
        break;
    }
    case PAYLOAD_TYPE_ST30_AUDIO: {
        rx_st30_session_context_t* rx_ctx = NULL;
        struct st30_rx_ops opts = {};

        ParseSt30RxOps(request, &opts);
        rx_ctx = mtl_st30_rx_session_create(mDevHandle, &opts, &memif_ops);
        if (rx_ctx == NULL) {
            INFO("%s, Failed to create RX session.", __func__);
            delete st_ctx;
            return -1;
        }

        st_ctx->rx_st30_session = rx_ctx;
        break;
    }
    case PAYLOAD_TYPE_ST40_ANCILLARY: {
        rx_st40_session_context_t* rx_ctx = NULL;
        struct st40_rx_ops opts = {};

        ParseSt40RxOps(request, &opts);
        rx_ctx = mtl_st40_rx_session_create(mDevHandle, &opts, &memif_ops);
        if (rx_ctx == NULL) {
            INFO("%s, Failed to create RX session.", __func__);
            delete st_ctx;
            return -1;
        }

        st_ctx->rx_st40_session = rx_ctx;
        break;
    }
    case PAYLOAD_TYPE_RTSP_VIDEO: {
        rx_udp_h264_session_context_t* rx_ctx = NULL;
        mcm_dp_addr local_addr = request->local_addr;
        /*udp poll*/
        rx_ctx = mtl_udp_h264_rx_session_create(mDevHandle, &local_addr, &memif_ops, schs);
        if (rx_ctx == NULL) {
            INFO("%s, Failed to create UDP H264 TX session.", __func__);
            delete st_ctx;
            return -1;
        }
        st_ctx->rx_udp_h264_session = rx_ctx;
        break;
    }
    case PAYLOAD_TYPE_ST20_VIDEO:
    case PAYLOAD_TYPE_NONE:
    default: {
        rx_session_context_t* rx_ctx = NULL;
        struct st20p_rx_ops opts = {};

        ParseSt20RxOps(request, &opts);
        rx_ctx = mtl_st20p_rx_session_create(mDevHandle, &opts, &memif_ops);
        if (rx_ctx == NULL) {
            INFO("%s, Failed to create RX session.", __func__);
            delete st_ctx;
            return -1;
        }

        st_ctx->rx_session = rx_ctx;
        break;
    }
    }

    st_ctx->payload_type = request->payload_type;
    st_ctx->id = memif_ops.m_session_count;
    std::cout << "session id: " << st_ctx->id << std::endl;
    INFO("%s, session id: %d", __func__, st_ctx->id);
    st_ctx->type = RX;

    st_ctx->registry_id = gRPC_RegisterConnection();

    mDpCtx.push_back(st_ctx);

    return (st_ctx->id);
}

int ProxyContext::RxStart(const mcm_conn_param *request)
{
    if (request->payload_type == PAYLOAD_TYPE_RDMA_VIDEO)
        return RxStart_rdma(request);

    return RxStart_mtl(request);
}

int ProxyContext::TxStart_rdma(const mcm_conn_param *request)
{
    tx_rdma_session_context_t *tx_ctx = NULL;
    dp_session_context_t *dp_ctx = NULL;
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
    dp_ctx = new (dp_session_context_t);
    ParseMemIFParam(request, memif_ops);
    opts.dir = direction::TX;
    opts.transfer_size = request->payload_args.rdma_args.transfer_size;
    memcpy(&opts.remote_addr, &request->remote_addr, sizeof(request->remote_addr));
    memcpy(&opts.local_addr, &request->local_addr, sizeof(request->local_addr));
    tx_ctx = rdma_tx_session_create(mDevHandle_rdma, &opts, &memif_ops);
    if (!tx_ctx) {
        INFO("%s, Failed to create RDMA session.", __func__);
        delete dp_ctx;
        return -EINVAL;
    }
    dp_ctx->payload_type = request->payload_type;
    dp_ctx->tx_rdma_session = tx_ctx;
    dp_ctx->id = memif_ops.m_session_count;
    INFO("%s, session id: %d", __func__, dp_ctx->id);
    dp_ctx->type = TX;

    dp_ctx->registry_id = gRPC_RegisterConnection();

    mDpCtx.push_back(dp_ctx);

    return dp_ctx->id;
}

int ProxyContext::TxStart_mtl(const mcm_conn_param *request)
{
    INFO("ProxyContext: TxStart(const mcm_conn_param* request)");
    dp_session_context_t *st_ctx = NULL;
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

    st_ctx = new (dp_session_context_t);

    ParseMemIFParam(request, memif_ops);

    switch (request->payload_type) {
    case PAYLOAD_TYPE_ST22_VIDEO: {
        tx_st22p_session_context_t* tx_ctx = NULL;
        struct st22p_tx_ops opts = {};

        ParseSt22TxOps(request, &opts);
        tx_ctx = mtl_st22p_tx_session_create(mDevHandle, &opts, &memif_ops);
        if (tx_ctx == NULL) {
            INFO("%s, Failed to create TX session.", __func__);
            delete st_ctx;
            return -1;
        }

        st_ctx->tx_st22p_session = tx_ctx;
        break;
    }
    case PAYLOAD_TYPE_ST30_AUDIO: {
        tx_st30_session_context_t* tx_ctx = NULL;
        struct st30_tx_ops opts = {};

        ParseSt30TxOps(request, &opts);
        tx_ctx = mtl_st30_tx_session_create(mDevHandle, &opts, &memif_ops);
        if (tx_ctx == NULL) {
            INFO("%s, Failed to create TX session.", __func__);
            delete st_ctx;
            return -1;
        }

        st_ctx->tx_st30_session = tx_ctx;
        break;
    }
    case PAYLOAD_TYPE_ST40_ANCILLARY: {
        tx_st40_session_context_t* tx_ctx = NULL;
        struct st40_tx_ops opts = {};

        ParseSt40TxOps(request, &opts);
        tx_ctx = mtl_st40_tx_session_create(mDevHandle, &opts, &memif_ops);
        if (tx_ctx == NULL) {
            INFO("%s, Failed to create TX session.", __func__);
            delete st_ctx;
            return -1;
        }

        st_ctx->tx_st40_session = tx_ctx;
        break;
    }
    case PAYLOAD_TYPE_ST20_VIDEO:
    default: {
        tx_session_context_t* tx_ctx = NULL;
        struct st20p_tx_ops opts = {};

        ParseSt20TxOps(request, &opts);
        tx_ctx = mtl_st20p_tx_session_create(mDevHandle, &opts, &memif_ops);
        if (tx_ctx == NULL) {
            INFO("%s, Failed to create TX session.", __func__);
            delete st_ctx;
            return -1;
        }

        st_ctx->tx_session = tx_ctx;
        break;
    }
    }

    st_ctx->payload_type = request->payload_type;
    st_ctx->id = incrementMSessionCount();
    st_ctx->type = TX;

    st_ctx->registry_id = gRPC_RegisterConnection();

    mDpCtx.push_back(st_ctx);

    return (st_ctx->id);
}

int ProxyContext::TxStart(const mcm_conn_param *request)
{
    if (request->payload_type == PAYLOAD_TYPE_RDMA_VIDEO)
        return TxStart_rdma(request);

    return TxStart_mtl(request);
}

void ProxyContext::TxStop(const int32_t session_id)
{
    auto ctx = std::find_if(mDpCtx.begin(), mDpCtx.end(),
                            [session_id](auto it) { return it->id == session_id; });

    if (ctx != mDpCtx.end()) {
        INFO("%s, Stop TX session ID: %d", __func__, session_id);

        switch ((*ctx)->payload_type) {
        case PAYLOAD_TYPE_ST22_VIDEO:
            mtl_st22p_tx_session_stop((*ctx)->tx_st22p_session);
            mtl_st22p_tx_session_destroy(&(*ctx)->tx_st22p_session);
            break;
        case PAYLOAD_TYPE_RDMA_VIDEO:
            rdma_tx_session_stop((*ctx)->tx_rdma_session);
            rdma_tx_session_destroy(&(*ctx)->tx_rdma_session);
            break;
        case PAYLOAD_TYPE_ST30_AUDIO:
            mtl_st30_tx_session_stop((*ctx)->tx_st30_session);
            mtl_st30_tx_session_destroy(&(*ctx)->tx_st30_session);
            break;
        case PAYLOAD_TYPE_ST40_ANCILLARY:
            mtl_st40_tx_session_stop((*ctx)->tx_st40_session);
            mtl_st40_tx_session_destroy(&(*ctx)->tx_st40_session);
            break;
        case PAYLOAD_TYPE_ST20_VIDEO:
        default:
            mtl_st20p_tx_session_stop((*ctx)->tx_session);
            mtl_st20p_tx_session_destroy(&(*ctx)->tx_session);
            break;
        }

        gRPC_UnregisterConnection((*ctx)->registry_id);

        mDpCtx.erase(ctx);
        delete (*ctx);

        /* Destroy device if all sessions stoped. */
        // if (mDpCtx.size() == 0) {
        //     mtl_deinit(mDevHandle);
        //     mDevHandle = NULL;
        // }
    } else {
        INFO("%s, Illegal TX session ID: %d", __func__, session_id);
    }

    return;
}

void ProxyContext::RxStop(const int32_t session_id)
{
    auto it = std::find_if(mDpCtx.begin(), mDpCtx.end(),
                           [session_id](auto it) { return it->id == session_id; });
    dp_session_context_t *ctx = *it;

    if (it != mDpCtx.end()) {
        INFO("%s, Stop RX session ID: %d", __func__, session_id);

        switch ((*it)->payload_type) {
        case PAYLOAD_TYPE_ST22_VIDEO:
            mtl_st22p_rx_session_stop((*it)->rx_st22p_session);
            mtl_st22p_rx_session_destroy(&(*it)->rx_st22p_session);
            break;
        case PAYLOAD_TYPE_ST30_AUDIO:
            mtl_st30_rx_session_stop((*it)->rx_st30_session);
            mtl_st30_rx_session_destroy(&(*it)->rx_st30_session);
            break;
        case PAYLOAD_TYPE_ST40_ANCILLARY:
            mtl_st40_rx_session_stop((*it)->rx_st40_session);
            mtl_st40_rx_session_destroy(&(*it)->rx_st40_session);
            break;
        case PAYLOAD_TYPE_RTSP_VIDEO:
            mtl_rtsp_rx_session_stop((*it)->rx_udp_h264_session);
            mtl_rtsp_rx_session_destroy(&(*it)->rx_udp_h264_session);
            break;
        case PAYLOAD_TYPE_RDMA_VIDEO:
            rdma_rx_session_stop((*it)->rx_rdma_session);
            rdma_rx_session_destroy(&(*it)->rx_rdma_session);
            break;
        case PAYLOAD_TYPE_ST20_VIDEO:
        default:
            mtl_st20p_rx_session_stop((*it)->rx_session);
            mtl_st20p_rx_session_destroy(&(*it)->rx_session);
            break;
        }

        gRPC_UnregisterConnection((*it)->registry_id);

        mDpCtx.erase(it);
        delete (ctx);

        /* Destroy device if all sessions stoped. */
        // if (mDpCtx.size() == 0) {
        //     mtl_deinit(mDevHandle);
        //     mDevHandle = NULL;
        // }
    } else {
        INFO("%s, Illegal RX session ID: %d", __func__, session_id);
    }

    return;
}

void ProxyContext::Stop()
{
    int err;

    for (auto it : mDpCtx) {
        if (it->type == TX) {
            mtl_st20p_tx_session_stop(it->tx_session);
            mtl_st20p_tx_session_destroy(&it->tx_session);
        } else {
            mtl_st20p_rx_session_stop(it->rx_session);
            mtl_st20p_rx_session_destroy(&it->rx_session);
        }

        delete (it);
    }

    mDpCtx.clear();
    st_pthread_mutex_destroy(&sessions_count_mutex_lock);

    mtl_deinit(mDevHandle);
    mDevHandle = NULL;

    if (mDevHandle_rdma) {
        err = rdma_deinit(&mDevHandle_rdma);
        if (err) {
            ERROR("%s, Failed to destroy rdma device.", __func__);
        }
    }
}
