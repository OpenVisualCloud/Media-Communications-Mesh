/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <getopt.h>
#include <thread>

#include "api_server_grpc.h"
#include "api_server_tcp.h"

#include <csignal>
#include "concurrency.h"
#include "client_api.h"
#include "logger.h"
#include "manager_local.h"

#ifndef IMTL_CONFIG_PATH
#define IMTL_CONFIG_PATH "./imtl.json"
#endif

#define DEFAULT_DEV_PORT "0000:31:00.1"
#define DEFAULT_DP_IP "192.168.1.20"
#define DEFAULT_GRPC_PORT "8001"
#define DEFAULT_TCP_PORT "8002"

#include "mesh/conn_rdma_rx.h"
#include "mesh/conn_rdma_tx.h"

/* print a description of all supported options */
void usage(FILE *fp, const char *path)
{
    /* take only the last portion of the path */
    const char *basename = strrchr(path, '/');
    basename = basename ? basename + 1 : path;

    fprintf(fp, "Usage: %s [OPTION]\n", basename);
    fprintf(fp, "-h, --help\t\t"
                "Print this help and exit.\n");
    fprintf(fp,
            "-d, --dev=dev_port\t"
            "PCI device port (defaults: %s).\n",
            DEFAULT_DEV_PORT);
    fprintf(fp,
            "-i, --ip=ip_address\t"
            "IP address for media data transportation (defaults: %s).\n",
            DEFAULT_DP_IP);
    fprintf(fp,
            "-g, --grpc=port_number\t"
            "Port number gRPC controller (defaults: %s).\n",
            DEFAULT_GRPC_PORT);
    fprintf(fp,
            "-t, --tcp=port_number\t"
            "Port number for TCP socket controller (defaults: %s).\n",
            DEFAULT_TCP_PORT);
}

using namespace mesh;

// Main context with cancellation
auto ctx = context::WithCancel(context::Background());

using namespace mesh;

class EmulatedReceiver : public connection::Connection
{
  public:
    EmulatedReceiver(context::Context &ctx)
    {
        _kind = connection::Kind::receiver;
        set_state(ctx, connection::State::configured);
    }

    connection::Result on_establish(context::Context &ctx) override
    {
        set_state(ctx, connection::State::active);
        return connection::Result::success;
    }

    connection::Result on_shutdown(context::Context &ctx) override
    {
        return connection::Result::success;
    }

    connection::Result on_receive(context::Context &ctx, void *ptr, uint32_t sz,
                                  uint32_t &sent) override
    {
        printf("Received %s (%u)\n", static_cast<char *>(ptr), sz);
        return connection::Result::success;
    }

    connection::Result configure(context::Context &ctx)
    {
        set_state(ctx, connection::State::configured);
        return connection::Result::success;
    }
};

class EmulatedTransmitter : public connection::Connection
{
  public:
    EmulatedTransmitter(context::Context &ctx)
    {
        _kind = connection::Kind::transmitter;
        set_state(ctx, connection::State::configured);
    }

    connection::Result on_establish(context::Context &ctx) override
    {
        set_state(ctx, connection::State::active);
        return connection::Result::success;
    }

    connection::Result on_shutdown(context::Context &ctx) override
    {
        return connection::Result::success;
    }

    connection::Result configure(context::Context &ctx)
    {
        set_state(ctx, connection::State::configured);
        return connection::Result::success;
    }

    connection::Result transmit_plaintext(context::Context &ctx, const void *ptr, uint32_t sz)
    {
        // Cast const void* to void* safely for transmit
        return transmit(ctx, const_cast<void *>(ptr), sz);
    }
};

int main(int argc, char *argv[])
{
    INFO("Starting media_proxy application...");
    std::string grpc_port = DEFAULT_GRPC_PORT;
    std::string tcp_port = DEFAULT_TCP_PORT;
    std::string dev_port = DEFAULT_DEV_PORT;
    std::string dp_ip = DEFAULT_DP_IP;
    int help_flag = 0;
    int opt;
    struct option longopts[] = {
        {"help", no_argument, &help_flag, 1},  {"dev", required_argument, NULL, 'd'},
        {"ip", required_argument, NULL, 'i'},  {"grpc", required_argument, NULL, 'g'},
        {"tcp", required_argument, NULL, 't'}, {0}};

    INFO("Parsing command-line arguments...");
    /* infinite loop, to be broken when we are done parsing options */
    while (1) {
        opt = getopt_long(argc, argv, "hd:i:g:t:", longopts, 0);
        if (opt == -1) {
            break;
        }

        switch (opt) {
        case 'h':
            help_flag = 1;
            break;
        case 'd':
            dev_port = optarg;
            break;
        case 'i':
            dp_ip = optarg;
            break;
        case 'g':
            grpc_port = optarg;
            break;
        case 't':
            tcp_port = optarg;
            break;
        case '?':
            usage(stderr, argv[0]);
            return 1;
        default:
            break;
        }
    }

    if (help_flag) {
        usage(stdout, argv[0]);
        return 0;
    }

    log::setFormatter(std::make_unique<log::StandardFormatter>());
    log::info("Media Proxy started");

    if (getenv("KAHAWAI_CFG_PATH") == NULL) {
      //  DEBUG("Set MTL configure file path to %s", IMTL_CONFIG_PATH);
        setenv("KAHAWAI_CFG_PATH", IMTL_CONFIG_PATH, 0);

        // Declare mDevHandle
        libfabric_ctx *mDevHandle = nullptr;

        INFO("Configuring connection parameters...");

        mcm_conn_param request = {};
        {
            // Initialize request based on tcp_port
            if (tcp_port == "8002") {
                request.type = is_rx;
                request.local_addr = {.ip = "192.168.1.20", .port = "8002"};
                request.remote_addr = {.ip = "192.168.1.25", .port = "8003"};
            } else {
                request.type = is_tx;
                request.local_addr = {.ip = "192.168.1.25", .port = "8003"};
                request.remote_addr = {.ip = "192.168.1.20", .port = "8002"};
            }

            // Initialize other required fields of request
            request.payload_type = PAYLOAD_TYPE_ST20_VIDEO;
            request.payload_codec = PAYLOAD_CODEC_NONE;
            // Initialize payload_args if needed
            request.payload_args.rdma_args.transfer_size =
                12; // Example transfer size

            // request.width = 1920;
            // request.height = 1080;
            // request.fps = 30;
            // request.pix_fmt = PIX_FMT_YUV422P_10BIT_LE;
            // request.payload_type_nr = 0;
            // request.payload_mtl_flags_mask = 0;
            // request.payload_mtl_pacing = 0;
        }

        auto ctx = context::WithCancel(mesh::context::Background());
        connection::Result res;

        INFO("Request initialized. TCP Port: %s", tcp_port.c_str());

        if (tcp_port == "8002") {
            INFO("Starting RX Path...");
            auto emulated_rx = std::make_unique<EmulatedReceiver>(ctx);
            INFO("Configuring Emulated Receiver...");
            emulated_rx->configure(ctx);
            INFO("Establishing Emulated Receiver...");
            emulated_rx->establish(ctx);

            // Setup Rx connection

            auto conn_rx = std::make_unique<mesh::connection::RdmaRx>();
            INFO("Configuring RDMA RX connection...");
            res = conn_rx->configure(ctx, request, dev_port, mDevHandle);
            if (res != connection::Result::success) {
                ERROR("Configure Rx failed: %s", mesh::connection::result2str(res));
                return 1;
            }

            INFO("Establishing RDMA RX connection...");
            try {
                res = conn_rx->establish(ctx);
                if (res != connection::Result::success) {
                    ERROR("Establish Rx failed: %s", mesh::connection::result2str(res));
                    return 1;
                }
            } catch (const std::exception &e) {
                ERROR("Exception during establish: %s", e.what());
                return 1;
            }

            INFO("Linking RDMA RX to Emulated Receiver...");
            conn_rx->set_link(ctx, emulated_rx.get());

            INFO("Sleeping to allow RX processing...");
            mesh::thread::Sleep(ctx, std::chrono::milliseconds(5000));

            INFO("Shutting down RDMA RX connection...");
            // Shutdown Rx connection
            res = conn_rx->shutdown(ctx);
            if (res != connection::Result::success) {
                ERROR("Shutdown Rx failed: %s", mesh::connection::result2str(res));
            }

            INFO("RX Path completed.");
            return 0;

        } else {
            auto conn_tx = std::make_unique<mesh::connection::RdmaTx>();
            INFO("Created unique_ptr for RdmaTx.");

            auto emulated_tx = std::make_unique<EmulatedTransmitter>(ctx);
            INFO("Created unique_ptr for EmulatedTransmitter.");

            INFO("Starting configuration of RdmaTx...");
            res = conn_tx->configure(ctx, request, dev_port, mDevHandle);
            if (res != connection::Result::success) {
                ERROR("Configure Tx failed: %s", mesh::connection::result2str(res));
                return 1;
            }
            INFO("Configuration of RdmaTx successful.");

            INFO("Establishing RdmaTx connection...");
            res = conn_tx->establish(ctx);
            if (res != connection::Result::success) {
                ERROR("Establish Tx failed: %s", mesh::connection::result2str(res));
                return 1;
            }
            INFO("RdmaTx connection established successfully.");

            INFO("Configuring EmulatedTransmitter...");
            res = emulated_tx->configure(ctx);
            if (res != connection::Result::success) {
                ERROR("Configure EmulatedTransmitter failed: %s",
                      mesh::connection::result2str(res));
                return 1;
            }
            INFO("Configuration of EmulatedTransmitter successful.");

            INFO("Establishing EmulatedTransmitter...");
            res = emulated_tx->establish(ctx);
            if (res != connection::Result::success) {
                ERROR("Establish EmulatedTransmitter failed: %s",
                      mesh::connection::result2str(res));
                return 1;
            }
            INFO("EmulatedTransmitter established successfully.");

            INFO("Linking EmulatedTransmitter with RdmaTx...");
            emulated_tx->set_link(ctx, conn_tx.get());
            INFO("Linking successful.");

            const char *plaintext = "Hello World";
            size_t text_size = strlen(plaintext) + 1; // Include null terminator
            INFO("Starting plaintext transmission of size %zu bytes.", text_size);

            for (int i = 0; i < 5; i++) {
                INFO("Starting transmission iteration %d...", i + 1);
                res = emulated_tx->transmit_plaintext(ctx, plaintext, text_size);
                if (res != connection::Result::success) {
                    ERROR("Transmit failed during iteration %d: %s", i + 1,
                          mesh::connection::result2str(res));
                    break;
                }
                INFO("Transmission iteration %d successful.", i + 1);
            }

            INFO("Shutting down RdmaTx connection...");
            res = conn_tx->shutdown(ctx);
            if (res != connection::Result::success) {
                ERROR("Shutdown Tx failed: %s", mesh::connection::result2str(res));
            } else {
                INFO("RdmaTx connection shut down successfully.");
            }

            INFO("Transmission path completed.");
            return 0;
        }
    }

    // ProxyContext* ctx = new ProxyContext("0.0.0.0:" + grpc_port, "0.0.0.0:" + tcp_port);
    // ctx->setDevicePort(dev_port);
    // ctx->setDataPlaneAddress(dp_ip);

    // /* start TCP server */
    // std::thread tcpThread(RunTCPServer, ctx);
    // tcpThread.join();

    // delete (ctx);

    return 0;
}
