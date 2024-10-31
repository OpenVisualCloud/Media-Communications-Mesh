/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "common.c"

int main(int argc, char** argv)
{
    // kind -> sender
    // payload_type -> video

    // conn_type -> memif | st2110 | rdma

    // if conn_type -> conn.memif -> socket_path + interface_id
    // elif conn_type -> st2110/rdma -> remote_ip_addr + remote_port (+local_ip_addr + local_port - static)
    //     if conn_type -> st2110 -> transport -> st2110-20/22 [/30 not here]
    
    // payload.video -> width + height + fps + pixel_format

    /* Video sender menu */
    int help_flag = 0;
    struct option longopts[] = {
        { "help",               no_argument, &help_flag, 'H' },
        { "conn_type",          required_argument, NULL, 'r' }, // memif | st2110 | rdma
        { "socket_path",        optional_argument, NULL, 'r' }, // memif only
        { "interface_id",       optional_argument, NULL, 'r' }, // memif only
        { "remote_ip_addr",     optional_argument, NULL, 'r' }, // st2110 OR rdma only
        { "remote_port",        optional_argument, NULL, 'r' }, // st2110 OR rdma only
        { "local_ip_addr",      optional_argument, NULL, 'r' }, // st2110 OR rdma only
        { "local_port",         optional_argument, NULL, 'r' }, // st2110 OR rdma only
        
        { "protocol",           required_argument, NULL, 'o' },
        { "type",               required_argument, NULL, 't' },
        { "width",              required_argument, NULL, 'w' },
        { "height",             required_argument, NULL, 'h' },
        { "fps",                required_argument, NULL, 'f' },
        { "pix_fmt",            required_argument, NULL, 'x' },
        { "number",             required_argument, NULL, 'n' },
        { 0 }
    };

    char recv_addr[46] = DEFAULT_RECV_IP;
    char recv_port[6] = DEFAULT_RECV_PORT;
    char payload_type[32] = "";
    char protocol_type[32] = "";

    uint32_t width = DEFAULT_FRAME_WIDTH;
    uint32_t height = DEFAULT_FRAME_HEIGHT;
    double vid_fps = DEFAULT_FPS;
    char pix_fmt_string[32] = DEFAULT_VIDEO_FMT;
    video_pixel_format pix_fmt = PIX_FMT_YUV422P_10BIT_LE;
    uint32_t frame_size = 0;
    uint32_t total_num = DEFAULT_TOTAL_NUM;

    /* infinite loop, to be broken when we are done parsing options */
    int opt;
    while (1) {
        opt = getopt_long(argc, argv,
                          "Hr:i:o:t:w:h:f:x:n:",
                          longopts, 0);
        if (opt == -1) {
            break;
        }

        switch (opt) {
        case 'H':
            help_flag = 1;
            break;
        case 'r':
            strlcpy(recv_addr, optarg, sizeof(recv_addr));
            break;
        case 'i':
            strlcpy(recv_port, optarg, sizeof(recv_port));
            break;
        case 'o':
            strlcpy(protocol_type, optarg, sizeof(protocol_type));
            break;
        case 't':
            strlcpy(payload_type, optarg, sizeof(payload_type));
            break;
        case 'w':
            width = atoi(optarg);
            break;
        case 'h':
            height = atoi(optarg);
            break;
        case 'f':
            vid_fps = atof(optarg);
            break;
        case 'x':
            strlcpy(pix_fmt_string, optarg, sizeof(pix_fmt_string));
            set_video_pix_fmt(&pix_fmt, pix_fmt_string);
            break;
        case 'n':
            total_num = atoi(optarg);
            break;
        default:
            break;
        }
    }

    if (help_flag) {
        usage(stdout, argv[0], 1);
        return 0;
    }
    /* END OF: Video sender menu */


    // int len = 0;
    // char* video_data = open_video_file(&len);

    /* Default client configuration */
    MeshClientConfig client_config = { 0 };
    
    /* ST2110-XX configuration */
    MeshConfig_ST2110 conn_config = {
        .remote_ip_addr = recv_addr,
        .remote_port = recv_port,
        .transport = MESH_CONN_TRANSPORT_ST2110_20,
    };

    /* Video configuration */
    MeshConfig_Video payload_config = {
        .width = width,
        .height = height,
        .fps = vid_fps,
        .pixel_format = pix_fmt,
    };

    MeshConnection *conn;
    MeshBuffer *buf;
    MeshClient *mc;
    int err;
    int i, n;

    /* Create a mesh client */
    err = mesh_create_client(&mc, &client_config);
    if (err) {
        printf("Failed to create mesh client: %s (%d)\n", mesh_err2str(err), err);
        exit(1);
    }

    /* Create a mesh connection */
    err = mesh_create_connection(mc, &conn);
    if (err) {
        printf("Failed to create connection: %s (%d)\n", mesh_err2str(err), err);
        goto exit_delete_client;
    }

    /* Apply connection configuration */
    err = mesh_apply_connection_config_st2110(conn, &conn_config);
    if (err) {
        printf("Failed to apply SMPTE ST2110 configuration: %s (%d)\n", mesh_err2str(err), err);
        goto exit_delete_conn;
    }

    /* Apply video payload configuration */
    err = mesh_apply_connection_config_video(conn, &payload_config);
    if (err) {
        printf("Failed to apply video configuration: %s (%d)\n", mesh_err2str(err), err);
        goto exit_delete_conn;
    }

    /* Establish a connection for sending data */
    err = mesh_establish_connection(conn, MESH_CONN_KIND_SENDER);
    if (err) {
        printf("Failed to establish connection: %s (%d)\n", mesh_err2str(err), err);
        goto exit_delete_conn;
    }

    /* 10 video frames to be sent */
    n = 10;

    /* Send data loop */
    for (i = 0; i < n; i++) {
        /* Ask the mesh to allocate a shared memory buffer for user data */
        err = mesh_get_buffer(conn, &buf);
        if (err) {
            printf("Failed to get buffer: %s (%d)\n", mesh_err2str(err), err);
            break;
        }

        /* Fill the buffer with user data */
        // put_user_video_frames(buf->data, buf->data_len);

        /* Send the buffer */
        err = mesh_put_buffer(&buf);
        if (err) {
            printf("Failed to put buffer: %s (%d)\n", mesh_err2str(err), err);
            break;
        }
    }

    /* Shutdown the connection */
    err = mesh_shutdown_connection(conn);
    if (err)
        printf("Failed to shutdown connection: %s (%d)\n", mesh_err2str(err), err);

exit_delete_conn:
    /* Delete the media connection */
    mesh_delete_connection(&conn);

exit_delete_client:
    /* Delete the mesh client */
    mesh_delete_client(&mc);

    if (err)
        exit(1);
}