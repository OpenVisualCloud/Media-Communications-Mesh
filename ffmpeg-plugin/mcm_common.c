/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <pthread.h>
#include "mcm_common.h"
#include <bsd/string.h>
#include <stdatomic.h>
#include <signal.h>
#include "libavutil/pixdesc.h"

static pthread_mutex_t mx = PTHREAD_MUTEX_INITIALIZER;
static MeshClient *client;
static int refcnt;
static atomic_bool shutdown_requested = ATOMIC_VAR_INIT(false);

void mcm_replace_back_quotes(char *str) {
    while (*str) {
        if (*str == '`')
            *str = '"';
        str++;
    }
}

static volatile __sighandler_t prev_SIGINT_handler;
static volatile __sighandler_t prev_SIGTERM_handler;

static void mcm_handle_signal(int signal) {
    atomic_store(&shutdown_requested, true);

    if (signal == SIGINT && prev_SIGINT_handler) {
        prev_SIGINT_handler(signal);
    } else if (signal == SIGTERM && prev_SIGTERM_handler) {
        prev_SIGTERM_handler(signal);
    }
}

/**
 * Check if a termination signal has been received from OS.
 */
bool mcm_shutdown_requested()
{
    return atomic_load(&shutdown_requested);
}

/**
 * Get MCM client. Create one if needed.
 * Thread-safe.
 */
int mcm_get_client(MeshClient **mc)
{
    int err;

    err = pthread_mutex_lock(&mx);
    if (err)
        return err;

    if (!client) {
        static char json_config[250];

        static const char json_config_format[] =
            "{"
                "`apiVersion`: `v1`,"
                "`apiConnectionString`: `Server=; Port=`,"
                "`apiDefaultTimeoutMicroseconds`: 100000,"
                "`maxMediaConnections`: 32"
            "}";

        snprintf(json_config, sizeof(json_config), json_config_format);
        mcm_replace_back_quotes(json_config);

        err = mesh_create_client(&client, json_config);
        if (err) {
            client = NULL;
        } else {
            struct sigaction action = { 0 };
                    
            sigfillset(&action.sa_mask);

            action.sa_flags = SA_RESTART;
            action.sa_handler = mcm_handle_signal;

            if (!prev_SIGINT_handler) {
                prev_SIGINT_handler = signal(SIGINT, SIG_DFL);
                sigaction(SIGINT, &action, NULL);
            }
            if (!prev_SIGTERM_handler) {
                prev_SIGTERM_handler = signal(SIGTERM, SIG_DFL);
                sigaction(SIGTERM, &action, NULL);
            }

            refcnt = 1;
        }
    } else {
        refcnt++;
    }

    pthread_mutex_unlock(&mx);

    *mc = client;

    return err;
}

/**
 * Put MCM client, or release the client.
 * Thread-safe.
 */
int mcm_put_client(MeshClient **mc)
{
    int err;

    err = pthread_mutex_lock(&mx);
    if (err)
        return err;

    if (!client) {
        err = -EINVAL;
    } else {
        if (--refcnt == 0)
            mesh_delete_client(&client);

        *mc = NULL;
    }

    pthread_mutex_unlock(&mx);

    return err;
}

const char mcm_json_config_multipoint_group_video_format[] =
    "{"
      "`bufferQueueCapacity`: %u,"
      "`connCreationDelayMilliseconds`: %u,"
      "`connection`: {"
        "`multipointGroup`: {"
          "`urn`: `%s`"
        "}"
      "},"
      "`options`: {"
        "`rdma`: {"
          "`provider`: `%s`,"
          "`num_endpoints`: %d"
        "}"
      "},"
      "`payload`: {"
        "`video`: {"
          "`width`: %d,"
          "`height`: %d,"
          "`fps`: %0.2f,"
          "`pixelFormat`: `%s`"
        "}"
      "}"
    "}";

const char mcm_json_config_st2110_video_format[] =
    "{"
      "`bufferQueueCapacity`: %u,"
      "`connCreationDelayMilliseconds`: %u,"
      "`connection`: {"
        "`st2110`: {"
          "`ipAddr`: `%s`,"
          "`port`: %d,"
          "`multicastSourceIpAddr`: `%s`,"
          "`transport`: `%s`,"
          "`payloadType`: %d,"
          "`transportPixelFormat`: `%s`"
        "}"
      "},"
      "`options`: {"
        "`rdma`: {"
          "`provider`: `%s`,"
          "`num_endpoints`: %d"
        "}"
      "},"
      "`payload`: {"
        "`video`: {"
          "`width`: %d,"
          "`height`: %d,"
          "`fps`: %0.2f,"
          "`pixelFormat`: `%s`"
        "}"
      "}"
    "}";

const char mcm_json_config_multipoint_group_audio_format[] =
    "{"
      "`bufferQueueCapacity`: %u,"
      "`connCreationDelayMilliseconds`: %u,"
      "`connection`: {"
        "`multipointGroup`: {"
          "`urn`: `%s`"
        "}"
      "},"
      "`options`: {"
        "`rdma`: {"
          "`provider`: `%s`,"
          "`num_endpoints`: %d"
        "}"
      "},"
      "`payload`: {"
        "`audio`: {"
          "`channels`: %d,"
          "`sampleRate`: %d,"
          "`format`: `%s`,"
          "`packetTime`: `%s`"
        "}"
      "}"
    "}";

const char mcm_json_config_st2110_audio_format[] =
    "{"
      "`bufferQueueCapacity`: %u,"
      "`connCreationDelayMilliseconds`: %u,"
      "`connection`: {"
        "`st2110`: {"
          "`ipAddr`: `%s`,"
          "`port`: %d,"
          "`multicastSourceIpAddr`: `%s`,"
          "`transport`: `st2110-30`,"
          "`payloadType`: %d"
        "}"
      "},"
      "`options`: {"
        "`rdma`: {"
          "`provider`: `%s`,"
          "`num_endpoints`: %d"
        "}"
      "},"
      "`payload`: {"
        "`audio`: {"
          "`channels`: %d,"
          "`sampleRate`: %d,"
          "`format`: `%s`,"
          "`packetTime`: `%s`"
        "}"
      "}"
    "}";
