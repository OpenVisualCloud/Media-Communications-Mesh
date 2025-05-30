/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <getopt.h>
#include <thread>

#include <csignal>
#include "concurrency.h"
#include "sdk_api.h"
#include "logger.h"
#include "manager_local.h"
#include "proxy_api.h"
#include "metrics_collector.h"
#include "proxy_config.h"
#include "mcm-version.h"

#include <execinfo.h>
#include <dlfcn.h>
#include <cxxabi.h>

#ifndef IMTL_CONFIG_PATH
#define IMTL_CONFIG_PATH "./imtl.json"
#endif

#define DEFAULT_GRPC_PORT "8001"
#define DEFAULT_SDK_API_PORT "8002"
#define DEFAULT_AGENT_PROXY_API_ADDR "localhost:50051"

using namespace mesh;

/* print a description of all supported options */
void usage(FILE* fp, const char* path)
{
    /* take only the last portion of the path */
    const char* basename = strrchr(path, '/');
    basename = basename ? basename + 1 : path;

    fprintf(fp, "Usage: %s [OPTION]\n", basename);
    fprintf(fp, "-h, --help\t\t\t"
                "Print this help and exit\n");
    fprintf(fp, "-t, --sdk=port_number\t\t"
                "Port number for SDK API server (default: %s)\n",
        DEFAULT_SDK_API_PORT);
    fprintf(fp, "-a, --agent=host:port\t\t"
                "MCM Agent Proxy API address in the format host:port (default: %s)\n",
        DEFAULT_AGENT_PROXY_API_ADDR);
    fprintf(fp, "-d, --st2110_dev=dev_port\t"
                "PCI device port for SMPTE 2110 media data transportation (default: %s)\n",
            config::proxy.st2110.dev_port_bdf.c_str());
    fprintf(fp, "-i, --st2110_ip=ip_address\t"
                "IP address for SMPTE 2110 (default: %s)\n",
            config::proxy.st2110.dataplane_ip_addr.c_str());
    fprintf(fp, "-r, --rdma_ip=ip_address\t"
                "IP address for RDMA (default: %s)\n",
            config::proxy.rdma.dataplane_ip_addr.c_str());
    fprintf(fp, "-p, --rdma_ports=ports_ranges\t"
                "Local port ranges for incoming RDMA connections (default: %s)\n",
            config::proxy.rdma.dataplane_local_ports.c_str());
}

void PrintStackTrace() {
    const int max_frames = 128;
    void* buffer[max_frames];
    int num_frames = backtrace(buffer, max_frames);
    char** symbols = backtrace_symbols(buffer, num_frames);

    std::cerr << "Stack trace:" << std::endl;
    for (int i = 0; i < num_frames; ++i) {
        Dl_info info;
        if (dladdr(buffer[i], &info) && info.dli_sname) {
            char* demangled = nullptr;
            int status = -1;
            if (info.dli_sname[0] == '_') {
                demangled = abi::__cxa_demangle(info.dli_sname, nullptr, 0, &status);
            }
            std::cerr << "  " << (status == 0 ? demangled : info.dli_sname) << " + " 
                      << ((char*)buffer[i] - (char*)info.dli_saddr) << " at " 
                      << info.dli_fname << std::endl;
            free(demangled);
        } else {
            std::cerr << symbols[i] << std::endl;
        }
    }

    free(symbols);
}

void SignalHandler(int signal) {
    std::cerr << "Error: signal " << signal << std::endl;
    PrintStackTrace();
    exit(1);
}

// Main context with cancellation
auto ctx = context::WithCancel(context::Background());

int main(int argc, char* argv[])
{
    signal(SIGSEGV, SignalHandler);

    std::string sdk_port = DEFAULT_SDK_API_PORT;
    std::string agent_addr = DEFAULT_AGENT_PROXY_API_ADDR;
    std::string st2110_dev_port = config::proxy.st2110.dev_port_bdf;
    std::string st2110_ip_addr = config::proxy.st2110.dataplane_ip_addr;
    std::string rdma_ip_addr = config::proxy.rdma.dataplane_ip_addr;
    std::string rdma_ports = config::proxy.rdma.dataplane_local_ports;
    int help_flag = 0;

    int opt;
    struct option longopts[] = {
        { "help", no_argument, &help_flag, 1 },
        { "sdk", required_argument, NULL, 't' },
        { "agent", required_argument, NULL, 'a' },
        { "st2110_dev", required_argument, NULL, 'd' },
        { "st2110_ip", required_argument, NULL, 'i' },
        { "rdma_ip", required_argument, NULL, 'r' },
        { "rdma_ports", required_argument, NULL, 'p' },
        { 0 }
    };

    /* infinite loop, to be broken when we are done parsing options */
    while (1) {
        opt = getopt_long(argc, argv, "h?t:a:d:i:r:p:", longopts, 0);
        if (opt == -1)
            break;

        switch (opt) {
        case 'h':
        case '?':
            help_flag = 1;
            break;
        case 't':
            sdk_port = optarg;
            break;
        case 'a':
            agent_addr = optarg;
            break;
        case 'd':
            st2110_dev_port = optarg;
            break;
        case 'i':
            st2110_ip_addr = optarg;
            break;
        case 'r':
            rdma_ip_addr = optarg;
            break;
        case 'p':
            rdma_ports = optarg;
            break;
        }
    }

    if (help_flag) {
        usage(stdout, argv[0]);
        return 0;
    }

    log::setFormatter(std::make_unique<log::StandardFormatter>());
    log::info("Media Proxy started, version %s #%s", VERSION_TAG, VERSION_HASH);

    if (getenv("KAHAWAI_CFG_PATH") == NULL) {
        log::debug("Set MTL configure file path to %s", IMTL_CONFIG_PATH);
        setenv("KAHAWAI_CFG_PATH", IMTL_CONFIG_PATH, 0);
    }

    config::proxy.agent_addr                 = std::move(agent_addr);
    config::proxy.st2110.dev_port_bdf        = std::move(st2110_dev_port);
    config::proxy.st2110.dataplane_ip_addr   = std::move(st2110_ip_addr);
    config::proxy.rdma.dataplane_ip_addr     = std::move(rdma_ip_addr);
    config::proxy.rdma.dataplane_local_ports = std::move(rdma_ports);

    try {
        config::proxy.sdk_api_port = std::stoi(sdk_port);
    } catch (...) {
        log::warn("Can't parse SDK API port. Using default port: %d",
                  config::proxy.sdk_api_port);
    }

    log::info("SDK API port: %u", config::proxy.sdk_api_port);
    log::info("MCM Agent Proxy API addr: %s", config::proxy.agent_addr.c_str());
    log::info("ST2110 device port BDF: %s",
              config::proxy.st2110.dev_port_bdf.c_str());
    log::info("ST2110 dataplane local IP addr: %s",
              config::proxy.st2110.dataplane_ip_addr.c_str());
    log::info("RDMA dataplane local IP addr: %s",
              config::proxy.rdma.dataplane_ip_addr.c_str());
    log::info("RDMA dataplane local port ranges: %s",
              config::proxy.rdma.dataplane_local_ports.c_str());

    // Intercept shutdown signals to cancel the main context
    auto signal_handler = [](int sig) {
        if (sig == SIGINT || sig == SIGTERM) {
            log::info("Shutdown signal received");
            ctx.cancel();
        }
    };
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Start ProxyAPI client
    auto err = RunProxyAPIClient(ctx);
    if (err)
        ctx.cancel();

    // Start metrics collector
    std::thread metricsCollectorThread([&]() {
        telemetry::MetricsCollector collector;
        collector.run(ctx);
    });

    // Start SDK API server
    auto sdk_ctx = context::WithCancel(context::Background());
    std::thread sdkApiThread([&]() { RunSDKAPIServer(sdk_ctx); });

    // Wait until the main context is cancelled
    ctx.done();

    // Stop Local connection manager
    log::info("Shutting down Local conn manager");
    auto tctx = context::WithTimeout(context::Background(),
                                     std::chrono::milliseconds(3000));
    connection::local_manager.shutdown(ctx);

    metricsCollectorThread.join();

    // Shutdown ProxyAPI client
    proxyApiClient->Shutdown();

    // Shutdown SDK API server
    sdk_ctx.cancel();
    sdkApiThread.join();

    log::info("Media Proxy exited");

    return 0;
}
