/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __MCM_COMMON_H__
#define __MCM_COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "libavformat/avformat.h"
#include "libavdevice/version.h"
#include <mesh_dp.h>
#if LIBAVDEVICE_VERSION_MAJOR <= 60
#define MCM_FFMPEG_6_1
#else /* LIBAVDEVICE_VERSION_MAJOR <= 60 */
#define MCM_FFMPEG_7_0
#endif /* LIBAVDEVICE_VERSION_MAJOR <= 60 */

int mcm_get_client(MeshClient **mc);
int mcm_put_client(MeshClient **mc);
int mcm_parse_conn_param(AVFormatContext* avctx, MeshConnection *conn,
                         int kind, char *ip_addr, char *port,
                         char *protocol_type, char *payload_type,
                         char *socket_name, int interface_id);
int mcm_parse_video_pix_fmt(AVFormatContext* avctx, int *pix_fmt,
                            enum AVPixelFormat value);
int mcm_parse_audio_sample_rate(AVFormatContext* avctx, int *sample_rate,
                                int value);
int mcm_parse_audio_packet_time(AVFormatContext* avctx, int *ptime, char *str);
void mcm_replace_back_quotes(char *str);
bool mcm_shutdown_requested(void);

extern const char mcm_json_config_multipoint_group_video_format[];
extern const char mcm_json_config_st2110_video_format[];

extern const char mcm_json_config_multipoint_group_audio_format[];
extern const char mcm_json_config_st2110_audio_format[];

#ifdef __cplusplus
}
#endif

#endif /* __MCM_COMMON_H__ */
