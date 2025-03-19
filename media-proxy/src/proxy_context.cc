/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <math.h>
#include <charconv>
#include <bsd/string.h>
#include <algorithm>

#include "proxy_context.h"

ProxyContext::ProxyContext(void)
    : mRpcCtrlAddr("0.0.0.0:8001")
    , mTcpCtrlAddr("0.0.0.0:8002")
    , imtl_init_preparing(false), mSessionCount(0)
{
    mRpcCtrlPort = 8001;
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

void ProxyContext::ParseStInitParam(const mcm_conn_param* request, struct mtl_init_params* st_param)
{
    if (getenv("MEDIA_PROXY_MAIN_LCORE") != NULL) {
        try {
            st_param->main_lcore = std::stoul(std::string(getenv("MEDIA_PROXY_MAIN_LCORE")));
        } catch (const std::invalid_argument& e) {
            ERROR("Conversion failed, Env MEDIA_PROXY_MAIN_LCORE must be an integer");
            st_param->main_lcore = 0;
        } catch (const std::out_of_range& e) {
            ERROR("Conversion failed, Env MEDIA_PROXY_MAIN_LCORE is out of range");
            st_param->main_lcore = 0;
        }
    }
    strlcpy(st_param->port[MTL_PORT_P], getDevicePort().c_str(), MTL_PORT_MAX_LEN);
    inet_pton(AF_INET, getDataPlaneAddress().c_str(), st_param->sip_addr[MTL_PORT_P]);
    st_param->pmd[MTL_PORT_P] = mtl_pmd_by_port_name(st_param->port[MTL_PORT_P]);
    st_param->num_ports = 1;
    st_param->flags = MTL_FLAG_BIND_NUMA;
    st_param->flags |= MTL_FLAG_TX_VIDEO_MIGRATE;
    st_param->flags |= MTL_FLAG_RX_VIDEO_MIGRATE;
    st_param->flags |= MTL_FLAG_RX_SEPARATE_VIDEO_LCORE;
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
    st_param->lcores = getenv("MEDIA_PROXY_LCORES");
    st_param->main_lcore = 10;
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

int ProxyContext::RxStart_rdma(const mcm_conn_param *request)
{
    memif_ops_t memif_ops = {0};
    int ret;

    if (!mDevHandle_rdma) {
        ret = libfabric_dev_ops.rdma_init(&mDevHandle_rdma);
        if (ret) {
            INFO("%s, Failed to initialize libfabric.", __func__);
            return -EINVAL;
        }
    }

    ParseMemIFParam(request, memif_ops);

    RxRdmaSession *session_ptr = new RxRdmaSession(mDevHandle_rdma, *request, memif_ops);

    if (session_ptr->init()) {
        ERROR("%s, Failed to initialize session.", __func__);
        delete session_ptr;
        return -1;
    }

    INFO("%s, session id: %d", __func__, session_ptr->get_id());
    std::lock_guard<std::mutex> lock(ctx_mtx);
    mDpCtx.push_back(session_ptr);
    return session_ptr->get_id();
}

int ProxyContext::RxStart_mtl(const mcm_conn_param *request)
{
    INFO("ProxyContext: RxStart(const mcm_conn_param* request)");
    struct st20p_rx_ops opts = {0};
    memif_ops_t memif_ops = {0};
    int ret;

    /*add lock to protect MTL library initialization to aviod being called by multi-session
     * simultaneously*/
    if (!mDevHandle && imtl_init_preparing == false) {

        imtl_init_preparing = true;
        struct mtl_init_params st_param = {0};

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

    Session *session_ptr = NULL;
    switch (request->payload_type) {
    case PAYLOAD_TYPE_ST20_VIDEO: {
        session_ptr = new RxSt20MtlSession(mDevHandle, *request, getDevicePort(), memif_ops);
        break;
    }
    case PAYLOAD_TYPE_ST22_VIDEO: {
        session_ptr = new RxSt22MtlSession(mDevHandle, *request, getDevicePort(), memif_ops);
        break;
    }
    case PAYLOAD_TYPE_ST30_AUDIO: {
        session_ptr = new RxSt30MtlSession(mDevHandle, *request, getDevicePort(), memif_ops);
        break;
    }
    case PAYLOAD_TYPE_ST40_ANCILLARY:
    default: {
        ERROR("Unsupported payload\n");
        return -1;
    }
    }

    if (session_ptr->init()) {
        ERROR("%s, Failed to initialize session.", __func__);
        delete session_ptr;
        return -1;
    }

    INFO("%s, session id: %d", __func__, session_ptr->get_id());
    std::lock_guard<std::mutex> lock(ctx_mtx);
    mDpCtx.push_back(session_ptr);
    return session_ptr->get_id();
}

int ProxyContext::RxStart(const mcm_conn_param *request)
{
    if (request->payload_type == PAYLOAD_TYPE_RDMA_VIDEO)
        return RxStart_rdma(request);

    return RxStart_mtl(request);
}

int ProxyContext::TxStart_rdma(const mcm_conn_param *request)
{
    memif_ops_t memif_ops = {0};
    int ret;

    if (!mDevHandle_rdma) {
        ret = libfabric_dev_ops.rdma_init(&mDevHandle_rdma);
        if (ret) {
            INFO("%s, Failed to initialize libfabric.", __func__);
            return -EINVAL;
        }
    }

    ParseMemIFParam(request, memif_ops);

    TxRdmaSession *session_ptr = new TxRdmaSession(mDevHandle_rdma, *request, memif_ops);

    if (session_ptr->init()) {
        ERROR("%s, Failed to initialize session.", __func__);
        delete session_ptr;
        return -1;
    }

    INFO("%s, session id: %d", __func__, session_ptr->get_id());
    std::lock_guard<std::mutex> lock(ctx_mtx);
    mDpCtx.push_back(session_ptr);
    return session_ptr->get_id();
}

int ProxyContext::TxStart_mtl(const mcm_conn_param *request)
{
    INFO("ProxyContext: TxStart(const mcm_conn_param* request)");
    memif_ops_t memif_ops = {0};

    /* add lock to protect MTL library initialization to avoid being called by multi-session
     * simultaneously */
    if (mDevHandle == NULL && imtl_init_preparing == false) {

        imtl_init_preparing = true;

        struct mtl_init_params st_param = {0};

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

    Session *session_ptr = NULL;
    switch (request->payload_type) {
    case PAYLOAD_TYPE_ST20_VIDEO: {
        session_ptr = new TxSt20MtlSession(mDevHandle, *request, getDevicePort(), memif_ops);
        break;
    }
    case PAYLOAD_TYPE_ST22_VIDEO: {
        session_ptr = new TxSt22MtlSession(mDevHandle, *request, getDevicePort(), memif_ops);
        break;
    }
    case PAYLOAD_TYPE_ST30_AUDIO: {
        session_ptr = new TxSt30MtlSession(mDevHandle, *request, getDevicePort(), memif_ops);
        break;
    }
    case PAYLOAD_TYPE_ST40_ANCILLARY:
    default: {
        ERROR("Unsupported payload\n");
        return -1;
    }
    }

    if (session_ptr->init()) {
        ERROR("%s, Failed to initialize session.", __func__);
        delete session_ptr;
        return -1;
    }

    INFO("%s, session id: %d", __func__, session_ptr->get_id());
    std::lock_guard<std::mutex> lock(ctx_mtx);
    mDpCtx.push_back(session_ptr);
    return session_ptr->get_id();
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
    std::lock_guard<std::mutex> lock(ctx_mtx);
    auto ctx = std::find_if(mDpCtx.begin(), mDpCtx.end(),
                            [session_id](auto it) { return it->get_id() == session_id; });
    if (ctx != mDpCtx.end()) {
        INFO("%s, Stop session ID: %d", __func__, session_id);

        Session *session_ptr = *ctx;
        delete session_ptr;
        mDpCtx.erase(ctx);
    } else {
        ERROR("%s, Illegal session ID: %d", __func__, session_id);
        ret = -1;
    }

    return ret;
}
