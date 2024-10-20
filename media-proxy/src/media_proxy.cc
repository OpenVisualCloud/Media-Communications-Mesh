/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <getopt.h>
#include <thread>

#include "api_server_grpc.h"
#include "api_server_tcp.h"

#ifndef IMTL_CONFIG_PATH
#define IMTL_CONFIG_PATH "./imtl.json"
#endif

#define DEFAULT_DEV_PORT "0000:31:00.0"
#define DEFAULT_DP_IP "192.168.96.1"
#define DEFAULT_GRPC_PORT "8001"
#define DEFAULT_TCP_PORT "8002"

/* print a description of all supported options */
void usage(FILE* fp, const char* path)
{
    /* take only the last portion of the path */
    const char* basename = strrchr(path, '/');
    basename = basename ? basename + 1 : path;

    fprintf(fp, "Usage: %s [OPTION]\n", basename);
    fprintf(fp, "-h, --help\t\t"
                "Print this help and exit.\n");
    fprintf(fp, "-d, --dev=dev_port\t"
                "PCI device port (defaults: %s).\n",
        DEFAULT_DEV_PORT);
    fprintf(fp, "-i, --ip=ip_address\t"
                "IP address for media data transportation (defaults: %s).\n",
        DEFAULT_DP_IP);
    fprintf(fp, "-g, --grpc=port_number\t"
                "Port number gRPC controller (defaults: %s).\n",
        DEFAULT_GRPC_PORT);
    fprintf(fp, "-t, --tcp=port_number\t"
                "Port number for TCP socket controller (defaults: %s).\n",
        DEFAULT_TCP_PORT);
}

int main(int argc, char* argv[])
{
    std::string grpc_port = DEFAULT_GRPC_PORT;
    std::string tcp_port = DEFAULT_TCP_PORT;
    std::string dev_port = DEFAULT_DEV_PORT;
    std::string dp_ip = DEFAULT_DP_IP;
    int help_flag = 0;
    int opt;
    struct option longopts[] = {
        { "help", no_argument, &help_flag, 1 },
        { "dev", required_argument, NULL, 'd' },
        { "ip", required_argument, NULL, 'i' },
        { "grpc", required_argument, NULL, 'g' },
        { "tcp", required_argument, NULL, 't' },
        { 0 }
    };

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

    if (getenv("KAHAWAI_CFG_PATH") == NULL) {
        DEBUG("Set MTL configure file path to %s", IMTL_CONFIG_PATH);
        setenv("KAHAWAI_CFG_PATH", IMTL_CONFIG_PATH, 0);
    }

    ProxyContext* ctx = new ProxyContext("0.0.0.0:" + grpc_port, "0.0.0.0:" + tcp_port);
    ctx->setDevicePort(dev_port);
    ctx->setDataPlaneAddress(dp_ip);

    /* start gRPC server */
    std::thread rpcThread(RunRPCServer, ctx);

    /* start TCP server */
    std::thread tcpThread(RunTCPServer, ctx);

    rpcThread.join();
    tcpThread.join();

    delete (ctx);

    return 0;
}
