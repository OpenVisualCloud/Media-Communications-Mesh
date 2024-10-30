/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef __PROXY_CONTEXT_H
#define __PROXY_CONTEXT_H

#include <atomic>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "libfabric_dev.h"
#include <mcm_dp.h>

#include "session-mtl.h"
#include "session-rdma.h"

#pragma once

class ProxyContext {
public:
    std::string mRpcCtrlAddr;
    int mRpcCtrlPort;

    std::string mTcpCtrlAddr;
    int mTcpCtrlPort;

    std::string mVideoFormat;

    // direction mDir;
    std::vector<Session *> mDpCtx;
    mtl_handle mDevHandle = NULL;
    libfabric_ctx *mDevHandle_rdma = NULL;

    bool imtl_init_preparing;
    pthread_mutex_t mutex_lock;

    std::string mDevPort;
    std::string mDpAddress;
    std::string mDpPort;

    ProxyContext(void);
    ProxyContext(std::string_view rpc_addr, std::string_view tcp_addr);

    void setRPCListenAddress(std::string_view addr);
    void setTCPListenAddress(std::string_view addr);
    void setDevicePort(std::string_view dev);
    void setDataPlaneAddress(std::string_view ip);
    void setDataPlanePort(std::string_view port);

    std::string getDevicePort(void);
    std::string getDataPlaneAddress(void);
    std::string getDataPlanePort(void);

    std::string getRPCListenAddress(void);
    std::string getTCPListenAddress(void);
    int getTCPListenPort(void);

    void ParseStInitParam(const mcm_conn_param* request, struct mtl_init_params* init_param);

    void ParseMemIFParam(const mcm_conn_param* request, memif_ops_t& memif_ops);

    int TxStart(const mcm_conn_param* request);
    int RxStart(const mcm_conn_param* request);
    int Stop(const int32_t session_id);

private:
    std::atomic<std::uint32_t> mSessionCount;
    pthread_mutex_t sessions_count_mutex_lock;

    ProxyContext(const ProxyContext&) = delete;
    ProxyContext& operator=(const ProxyContext&) = delete;
    uint32_t incrementMSessionCount(bool postIncrement);

    int TxStart_mtl(const mcm_conn_param *request);
    int RxStart_mtl(const mcm_conn_param *request);
    int TxStart_rdma(const mcm_conn_param *request);
    int RxStart_rdma(const mcm_conn_param *request);
};

#endif // __PROXY_CONTEXT_H
