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
#include <mcm_dp.h>

int mcm_parse_conn_param(AVFormatContext* avctx, mcm_conn_param *param,
                         transfer_type type, char *ip_addr, char *port,
                         char *protocol_type, char *payload_type,
                         char *socket_name, int interface_id);
int mcm_parse_audio_sample_rate(AVFormatContext* avctx, mcm_audio_sampling *sample_rate,
                                int value);
int mcm_parse_audio_packet_time(AVFormatContext* avctx, mcm_audio_ptime *ptime,
                                char *str);
int mcm_parse_audio_pcm_format(AVFormatContext* avctx, mcm_audio_format *fmt,
                               enum AVCodecID *codec_id, char *str);

#ifdef __cplusplus
}
#endif

#endif /* __MCM_COMMON_H__ */
