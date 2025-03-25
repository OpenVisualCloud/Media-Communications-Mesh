/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _INPUT_H_
#define _INPUT_H_

#include <time.h>
typedef struct video_params{
  int width;
  int height;
  double fps;
  char* pixel_format;
}video_params;

typedef struct audio_params{
  int channels;
  long long sample_rate;
  char* format;
  long long packet_time;
}audio_params;


char *input_parse_file_to_string(const char *file_name);
video_params get_video_params(const char *json_string);
audio_params get_audio_params(const char *json_string);
void parse_cli_commands(int argc, char *argv[]);
extern int input_loop;

#endif /* _INPUT_H_ */
