/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <iostream>
#include <string>
#include <vector>
#include <atomic>

#include "mtl.h"
#include "controller.grpc.pb.h"
#include <mcm_dp.h>

using controller::RxControlRequest;
using controller::TxControlRequest;
using controller::StopControlRequest;
using controller::StInit;
using controller::St20pRxOps;
using controller::St20pTxOps;
using controller::StRxPort;
using controller::StTxPort;
using controller::MemIFOps;

#pragma once

class ProxyContext {
public:
    std::string mRpcCtrlAddr;

    std::string mTcpCtrlAddr;
    int mTcpCtrlPort;

    std::string mVideoFormat;

    // direction mDir;
    std::vector<mtl_session_context_t*> mStCtx;
    std::vector<rx_session_context_t*> mRxCtx;
    std::vector<tx_session_context_t*> mTxCtx;
    std::atomic<std::uint32_t> mSessionCount;
    mtl_handle mDevHandle = NULL;

    std::string mDevPort;
    std::string mDpAddress;
    std::string mDpPort;

    ProxyContext(void);
    ProxyContext(std::string rpc_addr, std::string tcp_addr);

    void setRPCListenAddress(std::string addr);
    void setTCPListenAddress(std::string addr);
    void setDevicePort(std::string dev);
    void setDataPlaneAddress(std::string ip);
    void setDataPlanePort(std::string port);

    std::string getDevicePort(void);
    std::string getDataPlaneAddress(void);
    std::string getDataPlanePort(void);

    std::string getRPCListenAddress(void);
    std::string getTCPListenAddress(void);
    int getTCPListenPort(void);

    void ParseStInitParam(const TxControlRequest * request, struct mtl_init_params* init_param);
    void ParseStInitParam(const RxControlRequest * request, struct mtl_init_params* init_param);
    void ParseStInitParam(const mcm_conn_param* request, struct mtl_init_params* init_param);

    void ParseMemIFParam(const TxControlRequest* request, memif_ops_t& memif_ops);
    void ParseMemIFParam(const RxControlRequest* request, memif_ops_t& memif_ops);
    void ParseMemIFParam(const mcm_conn_param* request, memif_ops_t& memif_ops);

    void ParseSt20TxOps(const TxControlRequest * request, struct st20p_tx_ops* opts);
    void ParseSt20RxOps(const RxControlRequest * request, struct st20p_rx_ops* opts);
    void ParseSt20TxOps(const mcm_conn_param* request, struct st20p_tx_ops* opts);
    void ParseSt20RxOps(const mcm_conn_param* request, struct st20p_rx_ops* opts);
    void ParseSt22TxOps(const mcm_conn_param* request, struct st22p_tx_ops* opts);
    void ParseSt22RxOps(const mcm_conn_param* request, struct st22p_rx_ops* opts);
    void ParseSt30TxOps(const mcm_conn_param* request, struct st30_tx_ops* opts);
    void ParseSt30RxOps(const mcm_conn_param* request, struct st30_rx_ops* opts);
    void ParseSt40TxOps(const mcm_conn_param* request, struct st40_tx_ops* opts);
    void ParseSt40RxOps(const mcm_conn_param* request, struct st40_rx_ops* opts);
    int TxStart(const TxControlRequest * request);
    int RxStart(const RxControlRequest * request);
    int TxStart(const mcm_conn_param* request);
    int RxStart(const mcm_conn_param* request);
    void TxStop(const int32_t session_id);
    void RxStop(const int32_t session_id);
    void Stop();
};
