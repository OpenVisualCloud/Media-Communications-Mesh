/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <math.h>
#include <sstream>
#include <vector>

#include "proxy_context.h"
#include <mcm_dp.h>
#include <mtl/mtl_sch_api.h>

using std::istringstream;
using std::string;
using std::vector;

ProxyContext::ProxyContext(void)
    : mRpcCtrlAddr("0.0.0.0:8001")
    , mTcpCtrlAddr("0.0.0.0:8002")
    , mSessionCount(0)
{
    mTcpCtrlPort = 8002;
    /*udp poll*/
    schs_ready = false;
}

ProxyContext::ProxyContext(std::string rpc_addr, std::string tcp_addr)
try : mRpcCtrlAddr(rpc_addr), mTcpCtrlAddr(tcp_addr), mSessionCount(0) {
    vector<string> sub_str;
    istringstream f(mTcpCtrlAddr);
    string s;

    while (getline(f, s, ':')) {
        sub_str.push_back(s);
    }

    if (sub_str.size() != 2) {
        INFO("Illegal TCP listen address.");
        throw;
    }

    mTcpCtrlPort = std::stoi(sub_str[1]);
    schs_ready = false;
} catch (...) {
}

void ProxyContext::setRPCListenAddress(std::string addr)
{
    mRpcCtrlAddr = addr;
}

void ProxyContext::setTCPListenAddress(std::string addr)
{
    mTcpCtrlAddr = addr;
}

void ProxyContext::setDevicePort(std::string dev)
{
    mDevPort = dev;
}

void ProxyContext::setDataPlaneAddress(std::string ip)
{
    mDpAddress = ip;
}

void ProxyContext::setDataPlanePort(std::string port)
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

void ProxyContext::ParseStInitParam(const TxControlRequest* request, struct mtl_init_params* st_param)
{
    StInit init = request->st_init();
    st_param->num_ports = init.number_ports();

    std::string port_p = init.primary_port();
    strncpy(st_param->port[MTL_PORT_P], port_p.c_str(), MTL_PORT_MAX_LEN);

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

    INFO("ProxyContext: ParseStInitParam...");
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
    strncpy(st_param->port[MTL_PORT_P], port_p.c_str(), MTL_PORT_MAX_LEN);

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

    INFO("ProxyContext: ParseStInitParam...");
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
    strncpy(st_param->port[MTL_PORT_P], getDevicePort().c_str(), MTL_PORT_MAX_LEN);
    inet_pton(AF_INET, getDataPlaneAddress().c_str(), st_param->sip_addr[MTL_PORT_P]);
    st_param->pmd[MTL_PORT_P] = mtl_pmd_by_port_name(st_param->port[MTL_PORT_P]);
    st_param->num_ports = 1;
    st_param->flags = MTL_FLAG_BIND_NUMA;
    st_param->flags |= MTL_FLAG_SHARED_RX_QUEUE;
    st_param->flags |= MTL_FLAG_SHARED_TX_QUEUE;
    // st_param->flags |= MTL_FLAG_TX_VIDEO_MIGRATE;
    // st_param->flags |= MTL_FLAG_RX_VIDEO_MIGRATE;
    st_param->log_level = MTL_LOG_LEVEL_DEBUG;
    st_param->priv = NULL;
    st_param->ptp_get_time_fn = NULL;
    st_param->rx_queues_cnt[MTL_PORT_P] = 128;
    st_param->tx_queues_cnt[MTL_PORT_P] = 128;
    st_param->lcores = NULL;
    st_param->memzone_max = 3000;

    INFO("ProxyContext: ParseStInitParam...");
    INFO("num_ports : %d", st_param->num_ports);
    INFO("port      : %s", st_param->port[MTL_PORT_P]);
    INFO("sip_addr  : %s", getDataPlaneAddress().c_str());
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

void ProxyContext::ParseMemIFParam(const TxControlRequest* request, memif_ops_t& memif_ops)
{
    strncpy(memif_ops.app_name, request->memif_ops().app_name().c_str(), sizeof(memif_ops.app_name));
    strncpy(memif_ops.interface_name, request->memif_ops().interface_name().c_str(), sizeof(memif_ops.interface_name));
    strncpy(memif_ops.socket_path, request->memif_ops().socket_path().c_str(), sizeof(memif_ops.socket_path));
}

void ProxyContext::ParseMemIFParam(const RxControlRequest* request, memif_ops_t& memif_ops)
{
    strncpy(memif_ops.app_name, request->memif_ops().app_name().c_str(), sizeof(memif_ops.app_name));
    strncpy(memif_ops.interface_name, request->memif_ops().interface_name().c_str(), sizeof(memif_ops.interface_name));
    strncpy(memif_ops.socket_path, request->memif_ops().socket_path().c_str(), sizeof(memif_ops.socket_path));
}

void ProxyContext::ParseSt20RxOps(const RxControlRequest* request, struct st20p_rx_ops* ops_rx)
{
    St20pRxOps st20_rx = request->st20_rx();
    StRxPort rx_port = st20_rx.rx_port();

    std::string port = rx_port.port();
    strncpy(ops_rx->port.port[MTL_PORT_P], port.c_str(), MTL_PORT_MAX_LEN);
    for (int i = 0; i < rx_port.sip_addr_size(); ++i) {
        ops_rx->port.sip_addr[MTL_PORT_P][i] = rx_port.sip_addr(i);
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
    printf("INFO: sip_addr      :");
    for (int i = 0; i < rx_port.sip_addr_size(); ++i) {
        printf(" %d ", ops_rx->port.sip_addr[MTL_PORT_P][i]);
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
    strncpy(ops_tx->port.port[MTL_PORT_P], port.c_str(), MTL_PORT_MAX_LEN);
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

    inet_pton(AF_INET, request->remote_addr.ip, ops_rx->port.sip_addr[MTL_PORT_P]);
    ops_rx->port.udp_port[MTL_PORT_P] = atoi(request->local_addr.port);
    // ops_rx->port.udp_port[MTL_PORT_P] = RX_ST20_UDP_PORT;
    strncpy(ops_rx->port.port[MTL_PORT_P], getDevicePort().c_str(), MTL_PORT_MAX_LEN);
    ops_rx->port.num_port = 1;
    ops_rx->port.payload_type = 112;
    ops_rx->name = strdup(session_name);
    ops_rx->width = request->width;
    ops_rx->height = request->height;
    ops_rx->fps = st_frame_rate_to_st_fps((double)request->fps);
    // ops_rx->transport_fmt = ST20_FMT_YUV_422_10BIT;
    // ops_rx->output_fmt = ST_FRAME_FMT_YUV422RFC4175PG2BE10;
    ops_rx->transport_fmt = ST20_FMT_YUV_422_8BIT;
    ops_rx->output_fmt = ST_FRAME_FMT_YUV422CUSTOM8;
    ops_rx->device = ST_PLUGIN_DEVICE_AUTO;
    ops_rx->framebuff_cnt = 4;

    INFO("ProxyContext: %s...", __func__);
    INFO("port          : %s", ops_rx->port.port[MTL_PORT_P]);
    printf("INFO: sip_addr      :");
    for (int i = 0; i < MTL_IP_ADDR_LEN; ++i) {
        printf(" %d ", ops_rx->port.sip_addr[MTL_PORT_P][i]);
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
    string type_str = "";

    if (request->type == is_tx) {
        type_str = "tx";
    } else {
        type_str = "rx";
    }

    memif_ops.is_master = 1;
    memif_ops.interface_id = 0;
    snprintf(memif_ops.app_name, sizeof(memif_ops.app_name), "memif_%s_%d", type_str.c_str(), int(mSessionCount));
    snprintf(memif_ops.interface_name, sizeof(memif_ops.interface_name), "memif_%s_%d", type_str.c_str(), int(mSessionCount));
    snprintf(memif_ops.socket_path, sizeof(memif_ops.socket_path), "/run/mcm/media_proxy_%s_%d.sock", type_str.c_str(), int(mSessionCount));
}

void ProxyContext::ParseSt20TxOps(const mcm_conn_param* request, struct st20p_tx_ops* ops_tx)
{
    static int session_id = 0;
    char session_name[NAME_MAX] = "";

    snprintf(session_name, NAME_MAX, "mcm_tx_st20_%d", session_id++);

    inet_pton(AF_INET, request->remote_addr.ip, ops_tx->port.dip_addr[MTL_PORT_P]);
    ops_tx->port.udp_port[MTL_PORT_P] = atoi(request->remote_addr.port);
    strncpy(ops_tx->port.port[MTL_PORT_P], getDevicePort().c_str(), MTL_PORT_MAX_LEN);
    ops_tx->port.num_port = 1;
    ops_tx->port.payload_type = 112;
    ops_tx->name = strdup(session_name);
    ops_tx->width = request->width;
    ops_tx->height = request->height;
    ops_tx->fps = st_frame_rate_to_st_fps((double)request->fps);
    // ops_tx->transport_fmt = ST20_FMT_YUV_422_10BIT;
    // ops_tx->input_fmt = ST_FRAME_FMT_YUV422RFC4175PG2BE10;
    ops_tx->input_fmt = ST_FRAME_FMT_YUV422CUSTOM8;
    ops_tx->transport_fmt = ST20_FMT_YUV_422_8BIT;
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
    strncpy(ops->port.port[MTL_PORT_P], getDevicePort().c_str(), MTL_PORT_MAX_LEN);
    ops->port.num_port = 1;
    ops->port.payload_type = 114;
    ops->name = strdup(session_name);
    ops->width = request->width;
    ops->height = request->height;
    ops->fps = st_frame_rate_to_st_fps((double)request->fps);
    ops->input_fmt = ST_FRAME_FMT_YUV422PLANAR8;
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

    inet_pton(AF_INET, request->remote_addr.ip, ops->port.sip_addr[MTL_PORT_P]);
    ops->port.udp_port[MTL_PORT_P] = atoi(request->local_addr.port);

    // ops->port.udp_port[MTL_PORT_P] = RX_ST20_UDP_PORT;
    strncpy(ops->port.port[MTL_PORT_P], getDevicePort().c_str(), MTL_PORT_MAX_LEN);
    ops->port.num_port = 1;
    ops->port.payload_type = 114;
    ops->name = strdup(session_name);
    ops->width = request->width;
    ops->height = request->height;
    ops->fps = st_frame_rate_to_st_fps((double)request->fps);
    ops->output_fmt = ST_FRAME_FMT_YUV422PLANAR8;
    ops->device = ST_PLUGIN_DEVICE_AUTO;
    ops->framebuff_cnt = 4;
    ops->pack_type = ST22_PACK_CODESTREAM;
    ops->codec = ST22_CODEC_JPEGXS;
    ops->codec_thread_cnt = 0;
    ops->max_codestream_size = 0;

    INFO("ProxyContext: %s...", __func__);
    INFO("port          : %s", ops->port.port[MTL_PORT_P]);
    printf("INFO: sip_addr      :");
    for (int i = 0; i < MTL_IP_ADDR_LEN; ++i) {
        printf(" %d ", ops->port.sip_addr[MTL_PORT_P][i]);
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
    strncpy(ops->port[MTL_PORT_P], getDevicePort().c_str(), MTL_PORT_MAX_LEN);
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

    inet_pton(AF_INET, request->remote_addr.ip, ops->sip_addr[MTL_PORT_P]);
    ops->udp_port[MTL_PORT_P] = atoi(request->local_addr.port);

    strncpy(ops->port[MTL_PORT_P], getDevicePort().c_str(), MTL_PORT_MAX_LEN);
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
    printf("INFO: sip_addr      :");
    for (int i = 0; i < MTL_IP_ADDR_LEN; ++i) {
        printf(" %d ", ops->sip_addr[MTL_PORT_P][i]);
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
    strncpy(ops->port[MTL_PORT_P], getDevicePort().c_str(), MTL_PORT_MAX_LEN);
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

    inet_pton(AF_INET, request->remote_addr.ip, ops->sip_addr[MTL_PORT_P]);
    ops->udp_port[MTL_PORT_P] = atoi(request->local_addr.port);

    strncpy(ops->port[MTL_PORT_P], getDevicePort().c_str(), MTL_PORT_MAX_LEN);
    ops->num_port = 1;
    ops->payload_type = 113;
    ops->rtp_ring_size = 1024;
    ops->name = strdup(session_name);

    INFO("ProxyContext: %s...", __func__);
    INFO("port          : %s", ops->port[MTL_PORT_P]);
    printf("INFO: sip_addr      :");
    for (int i = 0; i < MTL_IP_ADDR_LEN; ++i) {
        printf(" %d ", ops->sip_addr[MTL_PORT_P][i]);
    }
    printf("\n");
    INFO("num_port      : %d", ops->num_port);
    INFO("udp_port      : %d", ops->udp_port[MTL_PORT_P]);
    INFO("payload_type  : %d", ops->payload_type);
    INFO("name          : %s", ops->name);
}

int ProxyContext::RxStart(const RxControlRequest* request)
{
    INFO("ProxyContext: RxStart...");
    struct st20p_rx_ops opts = { 0 };
    mtl_session_context_t* st_ctx = NULL;
    rx_session_context_t* rx_ctx = NULL;
    memif_ops_t memif_ops = { 0 };

    if (mDevHandle == NULL) {
        struct mtl_init_params st_param = {};
        ParseStInitParam(request, &st_param);

        /* set default parameters */
        st_param.flags = MTL_FLAG_BIND_NUMA;

        mDevHandle = inst_init(&st_param);
        if (mDevHandle == NULL) {
            INFO("%s, Fail to initialize MTL.\n", __func__);
            return -1;
        }
    }

    ParseSt20RxOps(request, &opts);

    ParseMemIFParam(request, memif_ops);

    rx_ctx = mtl_st20p_rx_session_create(mDevHandle, &opts, &memif_ops);
    if (rx_ctx == NULL) {
        INFO("%s, Fail to create RX session.\n", __func__);
        return -1;
    }

    st_ctx = new (mtl_session_context_t);
    st_ctx->id = mSessionCount++;
    st_ctx->type = RX;
    st_ctx->rx_session = rx_ctx;
    mStCtx.push_back(st_ctx);

    /* TODO: to be removed later. */
    mRxCtx.push_back(rx_ctx);

    return (st_ctx->id);
}

int ProxyContext::TxStart(const TxControlRequest* request)
{
    INFO("ProxyContext: TxStart...");
    struct st20p_tx_ops opts = { 0 };
    mtl_session_context_t* st_ctx = NULL;
    tx_session_context_t* tx_ctx = NULL;
    memif_ops_t memif_ops = { 0 };

    if (mDevHandle == NULL) {
        struct mtl_init_params st_param = {};

        ParseStInitParam(request, &st_param);

        /* set default parameters */
        st_param.flags = MTL_FLAG_BIND_NUMA;

        mDevHandle = inst_init(&st_param);
        if (mDevHandle == NULL) {
            INFO("%s, Fail to initialize MTL.", __func__);
            return -1;
        }
    }

    ParseSt20TxOps(request, &opts);

    ParseMemIFParam(request, memif_ops);

    tx_ctx = mtl_st20p_tx_session_create(mDevHandle, &opts, &memif_ops);
    if (tx_ctx == NULL) {
        INFO("%s, Fail to create TX session.", __func__);
        return -1;
    }

    st_ctx = new (mtl_session_context_t);
    st_ctx->id = mSessionCount++;
    st_ctx->type = TX;
    st_ctx->tx_session = tx_ctx;
    mStCtx.push_back(st_ctx);

    /* TODO: to be removed later. */
    mTxCtx.push_back(tx_ctx);

    return (st_ctx->id);
}

int ProxyContext::RxStart(const mcm_conn_param* request)
{
    INFO("ProxyContext: RxStart...");
    struct st20p_rx_ops opts = { 0 };
    mtl_session_context_t* st_ctx = NULL;
    memif_ops_t memif_ops = { 0 };
    int ret;

    if (mDevHandle == NULL) {
        struct mtl_init_params st_param = {};

        ParseStInitParam(request, &st_param);

        /* set default parameters */
        // st_param.flags = MTL_FLAG_BIND_NUMA;

        mDevHandle = inst_init(&st_param);
        if (mDevHandle == NULL) {
            INFO("%s, Fail to initialize MTL.\n", __func__);
            return -1;
        }
    }

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
            if ( sch == NULL) {
                INFO("%s, error: schduler create fail.", __func__);
                break;
            }
            ret = mtl_sch_start(sch);
            INFO("%s, start schduler %d.", __func__, i);
            if (ret < 0) {
                ret = mtl_sch_free(sch);
                break;
            }
            schs[i] = sch;  
        }
        schs_ready = true;
    }

    st_ctx = new (mtl_session_context_t);

    ParseMemIFParam(request, memif_ops);
    switch (request->payload_type) {
    case PAYLOAD_TYPE_ST22_VIDEO: {
        rx_st22p_session_context_t* rx_ctx = NULL;
        struct st22p_rx_ops opts = {};

        ParseSt22RxOps(request, &opts);
        rx_ctx = mtl_st22p_rx_session_create(mDevHandle, &opts, &memif_ops);
        if (rx_ctx == NULL) {
            INFO("%s, Fail to create TX session.", __func__);
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
            INFO("%s, Fail to create RX session.", __func__);
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
            INFO("%s, Fail to create RX session.", __func__);
            return -1;
        }

        st_ctx->rx_st40_session = rx_ctx;
        break;
    }
    case PAYLOAD_TYPE_RTSP_VIDEO: {
        rx_udp_h264_session_context_t* rx_ctx = NULL;
        // mcm_dp_addr remote_addr = request->remote_addr;
        mcm_dp_addr local_addr = request->local_addr;
        /*udp poll*/
        //rx_ctx = mtl_udp_h264_rx_session_create(mDevHandle, &remote_addr, &memif_ops);
        rx_ctx = mtl_udp_h264_rx_session_create(mDevHandle, &local_addr, &memif_ops, schs);
        if (rx_ctx == NULL) {
            INFO("%s, Fail to create UDP H264 TX session.", __func__);
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
            INFO("%s, Fail to create RX session.\n", __func__);
            return -1;
        }

        st_ctx->rx_session = rx_ctx;
        break;
    }
    }

    st_ctx->payload_type = request->payload_type;
    st_ctx->id = mSessionCount++;
    std::cout << "session id: " << st_ctx->id << std::endl;
    st_ctx->type = RX;
    mStCtx.push_back(st_ctx);

    return (st_ctx->id);
}

int ProxyContext::TxStart(const mcm_conn_param* request)
{
    INFO("ProxyContext: TxStart...");
    mtl_session_context_t* st_ctx = NULL;
    memif_ops_t memif_ops = { 0 };

    if (mDevHandle == NULL) {
        struct mtl_init_params st_param = {};

        ParseStInitParam(request, &st_param);

        /* set default parameters */
        st_param.flags = MTL_FLAG_BIND_NUMA;

        mDevHandle = inst_init(&st_param);
        if (mDevHandle == NULL) {
            INFO("%s, Fail to initialize MTL.", __func__);
            return -1;
        }
    }

    st_ctx = new (mtl_session_context_t);

    ParseMemIFParam(request, memif_ops);

    switch (request->payload_type) {
    case PAYLOAD_TYPE_ST22_VIDEO: {
        tx_st22p_session_context_t* tx_ctx = NULL;
        struct st22p_tx_ops opts = {};

        ParseSt22TxOps(request, &opts);
        tx_ctx = mtl_st22p_tx_session_create(mDevHandle, &opts, &memif_ops);
        if (tx_ctx == NULL) {
            INFO("%s, Fail to create TX session.", __func__);
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
            INFO("%s, Fail to create TX session.", __func__);
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
            INFO("%s, Fail to create TX session.", __func__);
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
            INFO("%s, Fail to create TX session.", __func__);
            return -1;
        }

        st_ctx->tx_session = tx_ctx;
        break;
    }
    }

    st_ctx->payload_type = request->payload_type;
    st_ctx->id = mSessionCount++;
    st_ctx->type = TX;
    mStCtx.push_back(st_ctx);

    return (st_ctx->id);
}

void ProxyContext::TxStop(const int32_t session_id)
{
    auto ctx = std::find_if(mStCtx.begin(), mStCtx.end(),
        [session_id](auto it) {
            return it->id == session_id;
        });

    if (ctx != mStCtx.end()) {
        INFO("%s, Stop TX session ID: %d", __func__, session_id);

        switch ((*ctx)->payload_type) {
        case PAYLOAD_TYPE_ST22_VIDEO:
            mtl_st22p_tx_session_stop((*ctx)->tx_st22p_session);
            mtl_st22p_tx_session_destroy(&(*ctx)->tx_st22p_session);
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

        mStCtx.erase(ctx);
        delete (*ctx);

        /* Destroy device if all sessions stoped. */
        // if (mStCtx.size() == 0) {
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
    auto it = std::find_if(mStCtx.begin(), mStCtx.end(),
        [session_id](auto it) {
            return it->id == session_id;
        });
    mtl_session_context_t* ctx = *it;

    if (it != mStCtx.end()) {
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
        case PAYLOAD_TYPE_ST20_VIDEO:
        default:
            mtl_st20p_rx_session_stop((*it)->rx_session);
            mtl_st20p_rx_session_destroy(&(*it)->rx_session);
            break;
        }
        mStCtx.erase(it);
        delete (ctx);

        /* Destroy device if all sessions stoped. */
        // if (mStCtx.size() == 0) {
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
    for (auto it : mStCtx) {
        if (it->type == TX) {
            mtl_st20p_tx_session_stop(it->tx_session);
            mtl_st20p_tx_session_destroy(&it->tx_session);
        } else {
            mtl_st20p_rx_session_stop(it->rx_session);
            mtl_st20p_rx_session_destroy(&it->rx_session);
        }

        delete (it);
    }

    mStCtx.clear();

    // destroy device
    mtl_deinit(mDevHandle);
    mDevHandle = NULL;
}
