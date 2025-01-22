#include <stdio.h>
#include "Inc/input.h"
#include "Inc/mcm.h"
#include <unistd.h>
#include <stdlib.h>

const char* client_cfg;
const char* conn_cfg;

int main(int argc, char* argv[]){
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <client_cfg.json> <connection_cfg.json> <abs path>/file|frame>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char* client_cfg_file = argv[1];
    char* conn_cfg_file = argv[2];
    char* frame_file = argv[3];

    MeshConnection *connection = NULL;
    MeshClient *client = NULL;

    printf("launching RX App \n");
    printf("reading client configuration... \n");  
    client_cfg = parse_json_to_string(client_cfg_file);
    printf("reading connection configuration... \n");
    conn_cfg = parse_json_to_string(conn_cfg_file);

    /* Initialize mcm client */
    int err = mesh_create_client_json(&client, client_cfg);
    if (err) {
        printf("Failed to create mesh client: %s (%d)\n", mesh_err2str(err), err);
        return err;
    }

    /* Create mesh connection */
    err = mesh_create_rx_connection(client, &connection, conn_cfg);
    if (err) {
        printf("Failed to create connection: %s (%d)\n", mesh_err2str(err), err);
        mesh_delete_client(&client);
        return err;
    }

    FILE *out = fopen(frame_file, "wb");

    printf("waiting for frames... \n");
    int timeout = MESH_TIMEOUT_INFINITE;
    int frame = 0;
    err = 0;
    MeshBuffer *buf = NULL;

    while(1){
        /* Set loop's  error*/
        err = 0;
        // mcm_receive_video_frames(connection, client, out, frame);
        if (frame){
            timeout = 1000;
        }

        /* Receive a buffer from the mesh */
        err = mesh_get_buffer_timeout(connection, &buf, timeout);
        if (err == MESH_ERR_CONN_CLOSED) {
            printf("Connection closed\n");
            break;
        }
        if (err) {
            printf("Failed to get buffer: %s (%d)\n", mesh_err2str(err), err);
            break;
        }
        printf("Received buf of %lu B length\n", buf->payload_len);
        /* Process the received user data */
        buffer_to_file(out, buf);
        // fwrite(buf->data, buf->data_len, 1, out);
        // printf("Received 1 video frame of %li B and saved it into the file\n", buf->data_len);
        // printf("---\n");

        /* Release and put the buffer back to the mesh */
        printf("Buf addr: %p\n", &buf);
        err = mesh_put_buffer(&buf);
        if (err) {
            printf("Failed to put buffer: %s (%d)\n", mesh_err2str(err), err);
            break;
        }

        printf("Frame: %d\n", frame+1);
        frame++;
    }
    mesh_delete_connection(&connection);
    mesh_delete_client(&client);
    return 0;
}