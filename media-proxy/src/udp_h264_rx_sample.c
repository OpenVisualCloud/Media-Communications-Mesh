/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "mtl.h"
#include "shm_memif.h"
#include <mcm_dp.h>
#include <mtl/mtl_sch_api.h>
#include <mtl/mudp_api.h>
#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <bsd/string.h>
#include <errno.h>

static void* rx_memif_event_loop(void* arg)
{
    int err = MEMIF_ERR_SUCCESS;
    memif_socket_handle_t memif_socket = (memif_socket_handle_t)arg;

    do {
        INFO("media-proxy waiting event.");
        err = memif_poll_event(memif_socket, -1);
    } while (err == MEMIF_ERR_SUCCESS);

    INFO("MEMIF DISCONNECTED.");

    return NULL;
}

int rx_udp_h264_shm_init(rx_udp_h264_session_context_t* rx_ctx, memif_ops_t* memif_ops)
{
    int ret = 0;
    memif_ops_t default_memif_ops = { 0 };

    if (rx_ctx == NULL) {
        printf("%s, fail to initialize udp h264 share memory.\n", __func__);
        return -1;
    }

    bzero(rx_ctx->memif_socket_args.app_name, sizeof(rx_ctx->memif_socket_args.app_name));
    bzero(rx_ctx->memif_socket_args.path, sizeof(rx_ctx->memif_socket_args.path));

    if (memif_ops == NULL) {
        strncpy(default_memif_ops.app_name, "mcm_rx", sizeof(default_memif_ops.app_name));
        strncpy(default_memif_ops.interface_name, "mcm_rx", sizeof(default_memif_ops.interface_name));
        strncpy(default_memif_ops.socket_path, "/run/mcm/mcm_rx_memif.sock", sizeof(default_memif_ops.socket_path));

        memif_ops = &default_memif_ops;
    }

    /* Set application name */
    strncpy(rx_ctx->memif_socket_args.app_name, memif_ops->app_name,
        sizeof(rx_ctx->memif_socket_args.app_name) - 1);

    /* Create memif socket
     * Interfaces are internally stored in a database referenced by memif socket. */
    strncpy(rx_ctx->memif_socket_args.path, memif_ops->socket_path,
        sizeof(rx_ctx->memif_socket_args.path) - 1);

    /* unlink socket file */
    if (memif_ops->is_master && rx_ctx->memif_socket_args.path[0] != '@') {
        if (mkdir("/run/mcm", 0666) != 0) {
            if (errno != EEXIST) {
                ERROR("Fail to access or create directory (/run/mcm) for MemIF.");
                return -1;
            }
        }
        unlink(rx_ctx->memif_socket_args.path);
    }

    INFO("Create memif socket.");
    ret = memif_create_socket(&rx_ctx->memif_socket, &rx_ctx->memif_socket_args, NULL);
    if (ret != MEMIF_ERR_SUCCESS) {
        INFO("memif_create_socket: %s", memif_strerror(ret));
        return -1;
    }

    /* Create memif interfaces
     * Both interaces are assigned the same socket and same id to create a loopback. */
    rx_ctx->shm_ready = 0;
    rx_ctx->memif_conn_args.socket = rx_ctx->memif_socket;
    rx_ctx->memif_conn_args.interface_id = memif_ops->interface_id;
    rx_ctx->memif_conn_args.buffer_size = 5184000;
    rx_ctx->memif_conn_args.log2_ring_size = 8;
    memcpy((char*)rx_ctx->memif_conn_args.interface_name, memif_ops->interface_name,
        sizeof(rx_ctx->memif_conn_args.interface_name));
    rx_ctx->memif_conn_args.is_master = memif_ops->is_master;

    INFO("create memif interface.");
    ret = memif_create(&rx_ctx->memif_conn, &rx_ctx->memif_conn_args,
        rx_udp_h264_on_connect, rx_on_disconnect, rx_on_receive, rx_ctx);
    if (ret != MEMIF_ERR_SUCCESS) {
        INFO("memif_create: %s", memif_strerror(ret));
        return -1;
    }

    /* Start the MemIF event loop. */
    ret = pthread_create(&rx_ctx->memif_event_thread, NULL, rx_memif_event_loop, rx_ctx->memif_conn_args.socket);
    if (ret < 0) {
        printf("%s(%d), thread create fail\n", __func__, ret);
        return -1;
    }

    return 0;
}

static int udp_server_h264(void* arg)
{

    rx_udp_h264_session_context_t* s = arg;
    mudp_handle socket = s->socket;
    ssize_t udp_len = MUDP_MAX_BYTES;
    char buf[udp_len];

    int err = 0;
    uint16_t qid = 0;
    uint16_t buf_num = 1;
    memif_buffer_t* tx_bufs = s->shm_bufs;
    char* dst;
    char* dst_nalu_size_point;
    uint16_t tx_buf_num = 0, tx = 0;
    uint32_t buf_size = s->memif_nalu_size;

    int flagFragment = 0;
    //FILE* fp = fopen("data1.264", "wb");
    unsigned char h264_frame_start_str[4];
    h264_frame_start_str[0] = 0;
    h264_frame_start_str[1] = 0;
    h264_frame_start_str[2] = 0;
    h264_frame_start_str[3] = 1;

    mcm_buffer* rtp_header;

    bool memif_alloc = false;

    while (s->shm_ready != 1) {
        INFO("%s, wait for share memory is ready\n", __func__);
        sleep(1);
    }
    //INFO("%s, start socket %p\n", __func__, socket);
    if (s->fragments_bunch == false) {
        rtp_header = calloc(1, sizeof(mcm_buffer));
    }

    if (s->stop != true) {
        ssize_t recv = mudp_recvfrom(socket, buf, sizeof(buf), 0, NULL, NULL);
        // printf("[%s] : recv = %d\n", __FUNCTION__, (int)recv);
        if (recv < 0) {
            ERROR("%s, mudp_recvfrom fail %d\n", __func__, (int)recv);
            free(rtp_header);
            return 0;
        }
        if (s->check_first_new_NALU == true) {
            unsigned char RTP_payload_type = *((unsigned char*)buf + 1);
            unsigned char mark = RTP_payload_type & 0x80;
            if (mark > 0) {
                s->new_NALU = 1;
                // printf("First Mark = %d\n", mark);
                s->check_first_new_NALU = false;
            }
            free(rtp_header);
            return 0;
        }

        if (s->fragments_bunch == true) {
            rtp_header = s->rtp_header;

            if (s->new_NALU == 1) {
                s->new_NALU = 0;
                /* allocate memory */
                tx_bufs = s->shm_bufs;
                while (memif_alloc != true) {
                    err = memif_buffer_alloc(s->memif_conn, qid, tx_bufs, buf_num, &s->tx_buf_num, buf_size);
                    if (err != MEMIF_ERR_SUCCESS) {
                        INFO("Failed to alloc memif buffer: %s, err:%d", memif_strerror(err), err);
                        return -1;
                    } else {
                        // INFO("Success to alloc memif buffer\n");
                        memif_alloc = true;
                    }
                }
                s->dst = tx_bufs->data;
                tx = 0;
                rtp_header->metadata.seq_num = *((uint16_t*)(buf + 2));
                rtp_header->metadata.timestamp = *((uint32_t*)(buf + 4));
                mtl_memcpy(s->dst, &rtp_header->metadata.seq_num, sizeof(rtp_header->metadata.seq_num));
                s->dst += sizeof(rtp_header->metadata.seq_num);
                mtl_memcpy(s->dst, &rtp_header->metadata.timestamp, sizeof(rtp_header->metadata.timestamp));
                s->dst += sizeof(rtp_header->metadata.timestamp);
                s->dst_nalu_size_point = s->dst;
                s->dst += sizeof(size_t);
                rtp_header->len = 0;

                //fwrite(h264_frame_start_str, 1, 1, s->fp);
                mtl_memcpy(s->dst, h264_frame_start_str, sizeof(unsigned char));
                s->dst += sizeof(unsigned char);
                rtp_header->len = rtp_header->len + 1;
            }

            unsigned char payload_header = *((unsigned char*)(buf + 12));
            unsigned char fragment_header = *((unsigned char*)(buf + 13));

            typedef unsigned char u8;
            u8 payload_type = payload_header & 0x1f;
            u8 payload_flag = payload_header & 0xe0;

            u8 fragment_start = fragment_header & 0x80;
            u8 fragment_end = fragment_header & 0x40;
            u8 fragment_type = fragment_header & 0x1f;
            u8 payload_header_temp = payload_flag + fragment_type;

            if (payload_type == 0x1c) {
                flagFragment = 1;
            } else {
                flagFragment = 0;
            }

            if (flagFragment == 1) {
                // printf("fragment true!\n");
                if (fragment_start == 0x80) {
                    // printf("fragment start!\n");
                    //fwrite(h264_frame_start_str + 1, 3, 1, s->fp);//printf("001\n");
                    mtl_memcpy(s->dst, h264_frame_start_str + 1, sizeof(unsigned char) * 3);
                    s->dst += sizeof(unsigned char) * 3;
                    rtp_header->len = rtp_header->len + 3;
                    //fwrite(&payload_header_temp, 1, 1, s->fp);
                    mtl_memcpy(s->dst, &payload_header_temp, sizeof(unsigned char));
                    s->dst += sizeof(unsigned char);
                    rtp_header->len = rtp_header->len + 1;
                    //fwrite(buf + 14, (int)recv - 14, 1, s->fp);
                    mtl_memcpy(s->dst, buf + 14, (int)recv - 14);
                    s->dst += (int)recv - 14;
                    rtp_header->len = rtp_header->len + (int)recv - 14;
                    return 0;
                } else {
                    if (fragment_end == 0x40) {
                        // printf("fragment end!\n");
                        //fwrite(buf + 14, (int)recv - 14, 1, s->fp);
                        mtl_memcpy(s->dst, buf + 14, (int)recv - 14);
                        s->dst += (int)recv - 14;
                        rtp_header->len = rtp_header->len + (int)recv - 14;
                    } else {
                        // printf("fragment middle!\n");
                        //fwrite(buf + 14, (int)recv - 14, 1, s->fp);
                        mtl_memcpy(s->dst, buf + 14, (int)recv - 14);
                        s->dst += (int)recv - 14;
                        rtp_header->len = rtp_header->len + (int)recv - 14;
                        return 0;
                    }
                }
            } else {
                // printf("fragment false!\n");
                //fwrite(h264_frame_start_str + 1, 3, 1, s->fp);//printf("001\n");
                mtl_memcpy(s->dst, h264_frame_start_str + 1, sizeof(unsigned char) * 3);
                s->dst += sizeof(unsigned char) * 3;
                rtp_header->len = rtp_header->len + 3;
                //fwrite(buf + 12, (int)recv - 12, 1, s->fp);
                mtl_memcpy(s->dst, buf + 12, sizeof(unsigned char) * ((int)recv - 12));
                s->dst += sizeof(unsigned char) * ((int)recv - 12);
                rtp_header->len = rtp_header->len + (int)recv - 12;
            }

            unsigned char RTP_payload_type = *((unsigned char*)buf + 1);
            unsigned char mark = RTP_payload_type & 0x80;

            if (mark > 0) {
                s->new_NALU = 1;
                mtl_memcpy(s->dst_nalu_size_point, &rtp_header->len, sizeof(size_t));
                err = memif_tx_burst(s->memif_conn, qid, tx_bufs, s->tx_buf_num, &tx);

                if (err != MEMIF_ERR_SUCCESS) {
                    INFO("memif_tx_burst for framgment=0: %s", memif_strerror(err));
                }
                s->dst = NULL;
                s->dst_nalu_size_point = NULL;
            }

        } else {
            /* allocate memory */
            memif_alloc = false;
            tx_bufs = s->shm_bufs;
            while (memif_alloc != true) {
                err = memif_buffer_alloc(s->memif_conn, qid, tx_bufs, buf_num, &tx_buf_num, buf_size);
                if (err != MEMIF_ERR_SUCCESS) {
                    INFO("Failed to alloc memif buffer: %s, err:%d", memif_strerror(err), err);
                    //continue;
                    free(rtp_header);
                    return -1;
                } else {
                    // INFO("Success to alloc memif buffer\n");
                    memif_alloc = true;
                }
            }
            dst = tx_bufs->data;
            tx = 0;
            rtp_header->metadata.seq_num = *((uint16_t*)(buf + 2));
            rtp_header->metadata.timestamp = *((uint32_t*)(buf + 4));
            mtl_memcpy(dst, &rtp_header->metadata.seq_num, sizeof(rtp_header->metadata.seq_num));
            dst += sizeof(rtp_header->metadata.seq_num);
            mtl_memcpy(dst, &rtp_header->metadata.timestamp, sizeof(rtp_header->metadata.timestamp));
            dst += sizeof(rtp_header->metadata.timestamp);
            dst_nalu_size_point = dst;
            dst += sizeof(size_t);
            rtp_header->len = 0;

            if (s->new_NALU == 1) {
                s->new_NALU = 0;

                // fwrite(h264_frame_start_str, 1, 1, fp);
                mtl_memcpy(dst, h264_frame_start_str, sizeof(unsigned char));
                dst += sizeof(unsigned char);
                rtp_header->len = rtp_header->len + 1;
            }
            unsigned char payload_header = *((unsigned char*)(buf + 12));
            unsigned char fragment_header = *((unsigned char*)(buf + 13));

            typedef unsigned char u8;
            u8 payload_type = payload_header & 0x1f;
            u8 payload_flag = payload_header & 0xe0;

            u8 fragment_start = fragment_header & 0x80;
            u8 fragment_end = fragment_header & 0x40;
            u8 fragment_type = fragment_header & 0x1f;
            u8 payload_header_temp = payload_flag + fragment_type;

            if (payload_type == 0x1c) {
                flagFragment = 1;
            } else {
                flagFragment = 0;
            }

            if (flagFragment == 1) {
                // printf("fragment true!\n");
                if (fragment_start == 0x80) {
                    // printf("fragment start!\n");
                    // fwrite(h264_frame_start_str + 1, 3, 1, fp);//printf("001\n");
                    mtl_memcpy(dst, h264_frame_start_str + 1, sizeof(unsigned char) * 3);
                    dst += sizeof(unsigned char) * 3;
                    rtp_header->len = rtp_header->len + 3;
                    // fwrite(&payload_header_temp, 1, 1, fp);
                    mtl_memcpy(dst, &payload_header_temp, sizeof(unsigned char));
                    dst += sizeof(unsigned char);
                    rtp_header->len = rtp_header->len + 1;
                    // fwrite(buf + 14, (int)recv - 14, 1, fp);
                    mtl_memcpy(dst, buf + 14, (int)recv - 14);
                    rtp_header->len = rtp_header->len + (int)recv - 14;
                    mtl_memcpy(dst_nalu_size_point, &rtp_header->len, sizeof(size_t));
                    /*Send to microservice application.*/
                    // INFO("memif_tx_burst for framgment = start\n");
                    err = memif_tx_burst(s->memif_conn, qid, tx_bufs, tx_buf_num, &tx);
                    if (err != MEMIF_ERR_SUCCESS) {
                        INFO("memif_tx_burst for framgment=0: %s", memif_strerror(err));
                    }
                } else {
                    if (fragment_end == 0x40) {
                        // printf("fragment end!\n");
                        // fwrite(buf + 14, (int)recv - 14, 1, fp);
                        mtl_memcpy(dst, buf + 14, (int)recv - 14);
                        dst += (int)recv - 14;
                        rtp_header->len = rtp_header->len + (int)recv - 14;
                        mtl_memcpy(dst_nalu_size_point, &rtp_header->len, sizeof(size_t));
                        /*Send to microservice application.*/
                        // INFO("memif_tx_burst for framgment = end\n");
                        err = memif_tx_burst(s->memif_conn, qid, tx_bufs, tx_buf_num, &tx);
                        if (err != MEMIF_ERR_SUCCESS) {
                            INFO("memif_tx_burst for framgment=1: %s", memif_strerror(err));
                        }
                    } else {
                        // printf("fragment middle!\n");
                        // fwrite(buf + 14, (int)recv - 14, 1, fp);
                        mtl_memcpy(dst, buf + 14, (int)recv - 14);
                        dst += (int)recv - 14;
                        rtp_header->len = rtp_header->len + (int)recv - 14;
                        mtl_memcpy(dst_nalu_size_point, &rtp_header->len, sizeof(size_t));
                        /*Send to microservice application.*/
                        // INFO("memif_tx_burst for framgment = middle\n");
                        err = memif_tx_burst(s->memif_conn, qid, tx_bufs, tx_buf_num, &tx);
                        if (err != MEMIF_ERR_SUCCESS) {
                            INFO("memif_tx_burst for framgment=1: %s", memif_strerror(err));
                        }
                    }
                }
            } else {
                // printf("fragment false!\n");
                // fwrite(h264_frame_start_str + 1, 3, 1, fp);//printf("001\n");
                mtl_memcpy(dst, h264_frame_start_str + 1, sizeof(unsigned char) * 3);
                dst += sizeof(unsigned char) * 3;
                rtp_header->len = rtp_header->len + 3;
                // fwrite(buf + 12, (int)recv - 12, 1, fp);
                mtl_memcpy(dst, buf + 12, sizeof(unsigned char) * ((int)recv - 12));
                dst += sizeof(unsigned char) * ((int)recv - 12);
                rtp_header->len = rtp_header->len + (int)recv - 12;
                mtl_memcpy(dst_nalu_size_point, &rtp_header->len, sizeof(size_t));
                /*Send to microservice application.*/
                // INFO("memif_tx_burst for framgment=0\n");
                err = memif_tx_burst(s->memif_conn, qid, tx_bufs, tx_buf_num, &tx);
                if (err != MEMIF_ERR_SUCCESS) {
                    INFO("memif_tx_burst for framgment=0: %s", memif_strerror(err));
                }
            }

            unsigned char RTP_payload_type = *((unsigned char*)buf + 1);
            unsigned char mark = RTP_payload_type & 0x80;

            if (mark > 0) {
                s->new_NALU = 1;
            }
            if (rtp_header != NULL) {
                free(rtp_header);
                rtp_header = NULL;
            }
        }
    } else {
        INFO("%s, has stopped\n", __func__);
    }
    return 0;
}

static int media_proxy_mudp_poll(void* priv)
{
    int ret;
    rx_udp_h264_session_context_t* ctx = (rx_udp_h264_session_context_t*)priv;

    if (ctx->sch_start == true) {
        int ret = udp_server_h264(ctx);
        if (ret == 0) {
            return MTL_TASKLET_ALL_DONE;
        } else {
            ERROR("media_proxy_mudp_poll: udp_server_h264 has memif error.\n");
            return MTL_TASKLET_ALL_DONE;
        }
    }
    INFO("media_proxy_mudp_poll: udp_server_h264 sch_start is false.\n");
    return MTL_TASKLET_ALL_DONE;
}

static int udp_poll_tasklet_start(void* priv)
{
    rx_udp_h264_session_context_t* ctx = (rx_udp_h264_session_context_t*)priv;
    ctx->sch_start = true;
    return 0;
}

static int udp_poll_tasklet_stop(void* priv)
{
    rx_udp_h264_session_context_t* ctx = (rx_udp_h264_session_context_t*)priv;
    ctx->sch_start = false;
    return 0;
}

rx_udp_h264_session_context_t* mtl_udp_h264_rx_session_create(mtl_handle dev_handle, mcm_dp_addr* dp_addr, memif_ops_t* memif_ops, mtl_sch_handle schs[])
{
    // struct st_sample_context ctx;
    rx_udp_h264_session_context_t* ctx = NULL;
    static int idx = 0;
    int err = 0;
    int ret = 0;

    if (dev_handle == NULL) {
        printf("%s, Invalid parameter.\n", __func__);
        return NULL;
    }

    ctx = calloc(1, sizeof(rx_udp_h264_session_context_t));
    if (ctx == NULL) {
        printf("%s, RX session contex malloc fail\n", __func__);
        return NULL;
    }
    // memset(&ctx, 0, sizeof(ctx));
    // ctx->udp_port = 20000;
    ctx->udp_port = atoi(dp_addr->port);
    ctx->payload_type = 112;
    snprintf(ctx->param.port[MTL_PORT_P], sizeof(ctx->param.port[MTL_PORT_P]), "%s", "0000:4b:01.3");
    ctx->udp_mode = SAMPLE_UDP_TRANSPORT_H264;

    ctx->st = dev_handle;
    ctx->stop = false;
    ctx->fragments_bunch = true;
    //ctx->fp = fopen("data1.264", "wb");

    if (ctx->fragments_bunch == true) {
        ctx->rtp_header = calloc(1, sizeof(mcm_buffer));
    }

    if (!ctx->st) {
        printf("m20230903235050_debug01_01_01\n");
        INFO("%s, mtl_init fail\n", __func__);
        // return -EIO;
        return NULL;
    }

    /*initialize share memory*/
    ret = rx_udp_h264_shm_init(ctx, memif_ops);
    if (ret < 0) {
        printf("%s, fail to initialize udp h264 share memory.\n", __func__);
        return NULL;
    }

    ctx->socket = mudp_socket(ctx->st, AF_INET, SOCK_DGRAM, 0);

    if (!ctx->socket) {
        INFO("%s, socket create fail\n", __func__);
        return NULL;
    }
    mudp_init_sockaddr(&ctx->client_addr, ctx->rx_sip_addr[MTL_PORT_P],
        ctx->udp_port);
    mudp_init_sockaddr(&ctx->bind_addr, ctx->param.sip_addr[MTL_PORT_P],
        ctx->udp_port);
    ret = mudp_bind(ctx->socket, (const struct sockaddr*)&ctx->bind_addr,
        sizeof(ctx->bind_addr));
    if (ret < 0) {
        INFO("%s, bind fail %d\n", __func__, ret);
        return NULL;
    }

    ctx->memif_nalu_size = 5184000;

    /*udp poll*/
    for (int i = 0; i < SCH_CNT; i++) {
        mtl_tasklet_handle udp_tasklet;

        ctx->udp_tasklet_ops.name = "udp_poll";
        ctx->udp_tasklet_ops.start = udp_poll_tasklet_start;
        ctx->udp_tasklet_ops.stop = udp_poll_tasklet_stop;
        ctx->udp_tasklet_ops.handler = media_proxy_mudp_poll;
        ctx->udp_tasklet_ops.priv = ctx;
        udp_tasklet = mtl_sch_register_tasklet(schs[i], &(ctx->udp_tasklet_ops));
        if (udp_tasklet != NULL) {
            ctx->udp_tasklet = udp_tasklet;
            ctx->udp_pollfd.fd = ctx->socket;
            ctx->udp_pollfd.events = POLLIN;
            ctx->check_first_new_NALU = true;
            ctx->new_NALU = 0;
            break;
        } else {
            continue;
        }
    }

    return ctx;
}
