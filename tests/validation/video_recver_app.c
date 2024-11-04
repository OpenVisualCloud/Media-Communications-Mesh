/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "common.c"

int main(int argc, char** argv)
{
    // kind -> receiver
    // payload_type -> video

    // conn_type -> st2110 [TBD: | memif | rdma]

    // if conn_type -> st2110 -> transport -> st2110-20/22 [/30 not here] + remote_ip_addr^ + remote_port^ + local_ip_addr^ + local_port^
    // elif conn_type -> conn.memif -> socket_path + interface_id
    // ^also with rdma
    
    // payload.video -> width + height + fps + pixel_format

    /* Video sender menu */
    struct option longopts[] = {
        { "help",           no_argument,       NULL, 'H' },
        { "output_file", required_argument, NULL, 'z' },
        // { "conn_type",      required_argument, NULL, 't' }, // memif | st2110 | rdma
        // { "socket_path",    optional_argument, NULL, 's' }, // memif only
        // { "interface_id",   optional_argument, NULL, 'i' }, // memif only
        { "remote_ip_addr", required_argument, NULL, 'a' }, // st2110 [OR rdma] only
        { "remote_port",    required_argument, NULL, 'p' }, // st2110 [OR rdma] only
        { "local_ip_addr",  required_argument, NULL, 'l' }, // st2110 [OR rdma] only
        { "local_port",     required_argument, NULL, 'o' }, // st2110 [OR rdma] only
        { "protocol",       required_argument, NULL, 'c' },
        { "type",           required_argument, NULL, 't' },
        { "width",          required_argument, NULL, 'w' },
        { "height",         required_argument, NULL, 'h' },
        { "fps",            required_argument, NULL, 'f' },
        { "pix_fmt",        required_argument, NULL, 'x' },
        // { "number",         required_argument, NULL, 'n' },
        { 0 }
    };

    char remote_ip_addr[46] = DEFAULT_SEND_IP;
    char remote_port[6] = DEFAULT_SEND_PORT;
    char local_ip_addr[46] = DEFAULT_RECV_IP;
    char local_port[6] = DEFAULT_RECV_PORT;
    char payload_type[32] = DEFAULT_PAYLOAD_TYPE;
    char protocol_type[32] = DEFAULT_PROTOCOL;

    static char output_file[128] = "";

    uint32_t width = DEFAULT_FRAME_WIDTH;
    uint32_t height = DEFAULT_FRAME_HEIGHT;
    double vid_fps = DEFAULT_FPS;
    char pix_fmt_string[32] = DEFAULT_PIX_FMT_STRING;
    video_pixel_format pix_fmt = DEFAULT_PIX_FMT;
    // uint32_t frame_size = 0; // zero is infinity (process all provided frames)
    // uint32_t total_num = DEFAULT_TOTAL_NUM;
    int transport = DEFAULT_MESH_CONN_TRANSPORT;
    FILE* dump_fp = NULL;

    /* infinite loop, to be broken when we are done parsing options */
    int opt;
    while (1) {
        opt = getopt_long(argc, argv,
                          "Hz:t:s:i:a:p:l:o:c:t:w:h:f:x",
                          longopts, 0);
        if (opt == -1) {
            break;
        }

        switch (opt) {
        case 'H':
            usage(stdout, argv[0], 1);
            return 0;
        // case 't': //conn_type
        //     break;
        // case 's': //socket_path
        //     break;
        // case 'i': //interface_id
        //     break;
        case 'z': //output_file
            strlcpy(output_file, optarg, sizeof(output_file));
            break;
        case 'a': //remote_ip_addr
            strlcpy(remote_ip_addr, optarg, sizeof(remote_ip_addr));
            break;
        case 'p': //remote_port
            strlcpy(remote_port, optarg, sizeof(remote_port));
            break;
        case 'l': //local_ip_addr
            strlcpy(local_ip_addr, optarg, sizeof(local_ip_addr));
            break;
        case 'o': //local_port
            strlcpy(local_port, optarg, sizeof(local_port));
            break;
        case 'c': //protocol
            strlcpy(protocol_type, optarg, sizeof(protocol_type));
            break;
        case 't': //type
            strlcpy(payload_type, optarg, sizeof(payload_type));
            set_video_payload_type(&transport, payload_type);
            break;
        case 'w': //width
            width = atoi(optarg);
            break;
        case 'h': //height
            height = atoi(optarg);
            break;
        case 'f': //fps
            vid_fps = atof(optarg);
            break;
        case 'x': //pix_fmt
            strlcpy(pix_fmt_string, optarg, sizeof(pix_fmt_string));
            set_video_pix_fmt(&pix_fmt, pix_fmt_string);
            break;
        // case 'n': //number
        //     total_num = atoi(optarg);
        //     break;
        default:
            break;
        }
    }
    /* END OF: Video sender menu */

    /* Check if output file is readable */
    if (strlen(output_file) > 0) {
        dump_fp = fopen(output_file, "wb");
    }

    /* Default client configuration */
    MeshClientConfig client_config = { 0 };
    
    /* ST2110-XX configuration */
    MeshConfig_ST2110 conn_config = {
        .remote_ip_addr = { *remote_ip_addr },
        .local_port = atoi(local_port),
        .transport = transport,
    };

    /* Video configuration */
    MeshConfig_Video payload_config = {
        .width = width,
        .height = height,
        .fps = vid_fps,
        .pixel_format = pix_fmt,
    };

    MeshConnection *conn;
    MeshClient *mc;
    int err;
    // int i;//, n;

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

    /* Establish a connection for receiving data */
    err = mesh_establish_connection(conn, MESH_CONN_KIND_RECEIVER);
    if (err) {
        printf("Failed to establish connection: %s (%d)\n", mesh_err2str(err), err);
        goto exit_delete_conn;
    }

    /* Receive data loop */
    do {
        MeshBuffer *buf;

        /* Receive a buffer from the mesh */
        err = mesh_get_buffer(conn, &buf);
        if (err == MESH_ERR_CONN_CLOSED) {
            printf("Connection closed\n");
            break;
        }
        if (err) {
            printf("Failed to get buffer: %s (%d)\n", mesh_err2str(err), err);
            break;
        }

        /* Process the received user data */
        // get_user_video_frames(buf->data, buf->data_len);

        /* Release and put the buffer back to the mesh */
        err = mesh_put_buffer(&buf);
        if (err) {
            printf("Failed to put buffer: %s (%d)\n", mesh_err2str(err), err);
            break;
        }
        if (dump_fp) {
            fwrite(buf->data, buf->data_len, 1, dump_fp);
        }
    } while (1);

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