# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh

# This file is a copy retreived from an MTL project

jxs_files = dict(
    i720p25={
        "filename": "HDR_BBC_v4_008_Penguin1_1280x720_10bit_25Hz_P422_180frames.jxs",
        "format": "YUV_422_10bit",
        "file_format": "YUV422PLANAR10LE",
        "width": 1280,
        "height": 720,
        "fps": 25,
    }
)

# Media Connection Mesh 25.03 Test plan

video_files_25_03 = {
    "FullHD_59.94": {
        "filename": "HDR_BBC_v4_008_Penguin1_1920x1080_10bit_25Hz_P422_180frames.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422PLANAR10LE",
        "width": 1920,
        "height": 1080,
        "fps": 59.94,
    },
    "FullHD_60": {
        "filename": "HDR_BBC_v4_008_Penguin1_1920x1080_10bit_25Hz_P422_180frames.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422PLANAR10LE",
        "width": 1920,
        "height": 1080,
        "fps": 60,
    },
    "4K_59.94": {
        "filename": "HDR_BBC_v4_008_Penguin1_3840x2160_10bit_25Hz_P422_180frames.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422PLANAR10LE",
        "width": 3840,
        "height": 2160,
        "fps": 59.94,
    },
    "4K_60": {
        "filename": "HDR_BBC_v4_008_Penguin1_3840x2160_10bit_25Hz_P422_180frames.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422PLANAR10LE",
        "width": 3840,
        "height": 2160,
        "fps": 60,
    },
}

audio_files_25_03 = {
    "PCM8_48000_Mono": {
        "filename": "voice_48000monoPCM8.pcm",
        "format": "pcm_s8",
        "channels": 1,
        "sample_rate": 48000,
        "packet_time": "1ms",
    },
    "PCM8_48000_Stereo": {
        "filename": "voice_48000stereoPCM8.pcm",
        "format": "pcm_s8",
        "channels": 2,
        "sample_rate": 48000,
        "packet_time": "1ms",
    },
    "PCM16_48000_Mono": {
        "filename": "voice_48000monoPCM16.pcm",
        "format": "pcm_s16be",
        "channels": 1,
        "sample_rate": 48000,
        "packet_time": "1ms",
    },
    "PCM16_48000_Stereo": {
        "filename": "voice_48000stereoPCM16.pcm",
        "format": "pcm_s16be",
        "channels": 2,
        "sample_rate": 48000,
        "packet_time": "1ms",
    },
    "PCM24_48000_Mono": {
        "filename": "voice_48000monoPCM24.pcm",
        "format": "pcm_s24be",
        "channels": 1,
        "sample_rate": 48000,
        "packet_time": "1ms",
    },
    "PCM24_48000_Stereo": {
        "filename": "voice_48000stereoPCM24.pcm",
        "format": "pcm_s24be",
        "channels": 2,
        "sample_rate": 48000,
        "packet_time": "1ms",
    },
    "PCM8_96000_Mono": {
        "filename": "voice_96000monoPCM8.pcm",
        "format": "pcm_s8",
        "channels": 1,
        "sample_rate": 96000,
        "packet_time": "1ms",
    },
    "PCM8_96000_Stereo": {
        "filename": "voice_96000stereoPCM8.pcm",
        "format": "pcm_s8",
        "channels": 2,
        "sample_rate": 96000,
        "packet_time": "1ms",
    },
    "PCM16_96000_Mono": {
        "filename": "voice_96000monoPCM16.pcm",
        "format": "pcm_s16be",
        "channels": 1,
        "sample_rate": 96000,
        "packet_time": "1ms",
    },
    "PCM16_96000_Stereo": {
        "filename": "voice_96000stereoPCM16.pcm",
        "format": "pcm_s16be",
        "channels": 2,
        "sample_rate": 96000,
        "packet_time": "1ms",
    },
    "PCM24_96000_Mono": {
        "filename": "voice_96000monoPCM24.pcm",
        "format": "pcm_s24be",
        "channels": 1,
        "sample_rate": 96000,
        "packet_time": "1ms",
    },
    "PCM24_96000_Stereo": {
        "filename": "voice_96000stereoPCM24.pcm",
        "format": "pcm_s24be",
        "channels": 2,
        "sample_rate": 96000,
        "packet_time": "1ms",
    },
}

blob_files_25_03 = dict(
    random_bin_100M={
        "filename": "random_data.bin",
        "max_payload_size": 102400,
    },
)

yuv_files = dict(
    i720p23={
        "filename": "HDR_BBC_v4_008_Penguin1_1280x720_10bit_25Hz_P422_180frames.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422PLANAR10LE",
        "width": 1280,
        "height": 720,
        "fps": 23,
    },
    i720p24={
        "filename": "HDR_BBC_v4_008_Penguin1_1280x720_10bit_25Hz_P422_180frames.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422PLANAR10LE",
        "width": 1280,
        "height": 720,
        "fps": 24,
    },
    i720p25={
        "filename": "HDR_BBC_v4_008_Penguin1_1280x720_10bit_25Hz_P422_180frames.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422PLANAR10LE",
        "width": 1280,
        "height": 720,
        "fps": 25,
    },
    i720p29={
        "filename": "Plalaedit_Pedestrian_10bit_1280x720_30Hz_P420_To_yuv422p10be_To_yuv422YCBCR10be.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",
        "width": 1280,
        "height": 720,
        "fps": 29,
    },
    i720p30={
        "filename": "Plalaedit_Pedestrian_10bit_1280x720_30Hz_P420_To_yuv422p10be_To_yuv422YCBCR10be.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",
        "width": 1280,
        "height": 720,
        "fps": 30,
    },
    i720p50={
        "filename": "ParkJoy_1280x720_10bit_50Hz_P422_To_yuv422p10be_To_yuv422YCBCR10be.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",
        "width": 1280,
        "height": 720,
        "fps": 50,
    },
    i720p59={
        "filename": "Netflix_Crosswalk_1280x720_10bit_60Hz_P422_yuv422p10be_To_yuv422YCBCR10be.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",
        "width": 1280,
        "height": 720,
        "fps": 59,
    },
    i720p60={
        "filename": "Netflix_Crosswalk_1280x720_10bit_60Hz_P422_yuv422p10be_To_yuv422YCBCR10be.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",
        "width": 1280,
        "height": 720,
        "fps": 60,
    },
    i720p119={
        "filename": "Netflix_Crosswalk_1280x720_10bit_60Hz_P422_yuv422p10be_To_yuv422YCBCR10be.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",
        "width": 1280,
        "height": 720,
        "fps": 119,
    },
    i1080p23={
        "filename": "HDR_BBC_v4_008_Penguin1_1920x1080_10bit_25Hz_180frames_yuv422p10be_To_yuv422rfc4175be10.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",
        "width": 1920,
        "height": 1080,
        "fps": 23,
    },
    i1080p24={
        "filename": "HDR_BBC_v4_008_Penguin1_1920x1080_10bit_25Hz_180frames_yuv422p10be_To_yuv422rfc4175be10.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",
        "width": 1920,
        "height": 1080,
        "fps": 24,
    },
    i1080p25={
        "filename": "HDR_BBC_v4_008_Penguin1_1920x1080_10bit_25Hz_180frames_yuv422p10be_To_yuv422rfc4175be10.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",
        "width": 1920,
        "height": 1080,
        "fps": 25,
    },
    i1080p29={
        "filename": "Plalaedit_Pedestrian_10bit_1920x1080_30Hz_P420_To_yuv422p10be_To_yuv422YCBCR10be.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",
        "width": 1920,
        "height": 1080,
        "fps": 29,
    },
    i1080p30={
        "filename": "HDR_BBC_v4_008_Penguin1_1920x1080_10bit_25Hz_180frames_yuv422p10be_To_yuv422rfc4175be10.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",
        "width": 1920,
        "height": 1080,
        "fps": 30,
    },
    i1080p50={
        "filename": "ParkJoy_1920x1080_10bit_50Hz_P422_yuv422p10be_To_yuv422YCBCR10be.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",
        "width": 1920,
        "height": 1080,
        "fps": 50,
    },
    i1080p59={
        "filename": "Netflix_Crosswalk_1920x1080_10bit_60Hz_P422_yuv422p10be_To_yuv422YCBCR10be.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",
        "width": 1920,
        "height": 1080,
        "fps": 59,
    },
    i1080p60={
        "filename": "Netflix_Crosswalk_1920x1080_10bit_60Hz_P422_yuv422p10be_To_yuv422YCBCR10be.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",
        "width": 1920,
        "height": 1080,
        "fps": 60,
    },
    i1080p100={
        "filename": "Netflix_Crosswalk_1920x1080_10bit_60Hz_P422_yuv422p10be_To_yuv422YCBCR10be.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",
        "width": 1920,
        "height": 1080,
        "fps": 100,
    },
    i1080p119={
        "filename": "Netflix_Crosswalk_1920x1080_10bit_60Hz_P422_yuv422p10be_To_yuv422YCBCR10be.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",
        "width": 1920,
        "height": 1080,
        "fps": 119,
    },
    i1080p120={
        "filename": "Netflix_Crosswalk_1920x1080_10bit_60Hz_P422_yuv422p10be_To_yuv422YCBCR10be.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",
        "width": 1920,
        "height": 1080,
        "fps": 120,
    },
    i2160p23={
        "filename": "test_3840x2160_for_25fps.yuv",  # shows up as random pixels
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",
        "width": 3840,
        "height": 2160,
        "fps": 23,
    },
    i2160p24={
        "filename": "test_3840x2160_for_25fps.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",
        "width": 3840,
        "height": 2160,
        "fps": 24,
    },
    i2160p25={
        "filename": "test_3840x2160_for_25fps.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",
        "width": 3840,
        "height": 2160,
        "fps": 25,
    },
    i2160p29={
        "filename": "Plalaedit_Pedestrian_10bit_3840x2160_30Hz_P420_To_yuv422p10be_To_yuv422YCBCR10be.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",
        "width": 3840,
        "height": 2160,
        "fps": 29,
    },
    i2160p30={
        "filename": "Plalaedit_Pedestrian_10bit_3840x2160_30Hz_P420_To_yuv422p10be_To_yuv422YCBCR10be.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",
        "width": 3840,
        "height": 2160,
        "fps": 30,
    },
    i2160p50={
        "filename": "ParkJoy_3840x2160_10bit_50Hz_P422_To_yuv422p10be_To_yuv422YCBCR10be.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",
        "width": 3840,
        "height": 2160,
        "fps": 50,
    },
    i2160p59={
        "filename": "Netflix_Crosswalk_3840x2160_10bit_60Hz_P422_To_yuv422p10be_To_yuv422YCBCR10be.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",
        "width": 3840,
        "height": 2160,
        "fps": 59,
    },
    i2160p60={
        "filename": "Netflix_Crosswalk_3840x2160_10bit_60Hz_P422_To_yuv422p10be_To_yuv422YCBCR10be.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",
        "width": 3840,
        "height": 2160,
        "fps": 60,
    },
    i2160p119={
        "filename": "Netflix_Crosswalk_3840x2160_10bit_60Hz_P422_To_yuv422p10be_To_yuv422YCBCR10be.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",
        "width": 3840,
        "height": 2160,
        "fps": 119,
    },
    i4320p23={
        "filename": "test_8k.yuv",  # single frame?
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",
        "width": 7680,
        "height": 4320,
        "fps": 23,
    },
    i4320p24={
        "filename": "test_8k.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",
        "width": 7680,
        "height": 4320,
        "fps": 24,
    },
    i4320p25={
        "filename": "test_8k.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",
        "width": 7680,
        "height": 4320,
        "fps": 25,
    },
    i4320p29={
        "filename": "test_8k.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",
        "width": 7680,
        "height": 4320,
        "fps": 29,
    },
    i4320p30={
        "filename": "test_8k.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",
        "width": 7680,
        "height": 4320,
        "fps": 30,
    },
    i4320p50={
        "filename": "test_8k.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",
        "width": 7680,
        "height": 4320,
        "fps": 50,
    },
    i4320p59={
        "filename": "test_8k.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",
        "width": 7680,
        "height": 4320,
        "fps": 59,
    },
    i4320p60={
        "filename": "test_8k.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",
        "width": 7680,
        "height": 4320,
        "fps": 60,
    },
    i4320p119={
        "filename": "test_8k.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",
        "width": 7680,
        "height": 4320,
        "fps": 119,
    },
    i480i59={
        "filename": "Netflix_Crosswalk_720x480_interlace_10bit_60Hz_P422.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",  # although the file name suggests otherwise
        "width": 720,
        "height": 480,
        "fps": 59,
    },
    i576i50={
        "filename": "ParkJoy_720x576_interlace_10bit_50Hz_P422.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",  # although the file name suggests otherwise
        "width": 720,
        "height": 576,
        "fps": 50,
    },
    i1080i50={
        "filename": "ParkJoy_1920x1080_interlace_10bit_50Hz_P422.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",  # although the file name suggests otherwise
        "width": 1920,
        "height": 1080,
        "fps": 50,
    },
    i1080i59={
        "filename": "Netflix_Crosswalk_1920x1080_interlace_10bit_60Hz_P422.yuv",
        "format": "YUV_422_10bit",
        "file_format": "YUV422RFC4175PG2BE10",  # although the file name suggests otherwise
        "width": 1920,
        "height": 1080,
        "fps": 59,
    },
)

yuv_files_422p10le = dict(
    Penguin_720p={
        "filename": "HDR_BBC_v4_008_Penguin1_1280x720_10bit_25Hz_P422_180frames.yuv",
        "file_format": "YUV422PLANAR10LE",
        "format": "YUV_422_10bit",
        "width": 1280,
        "height": 720,
    },
    Penguin_1080p={
        "filename": "HDR_BBC_v4_008_Penguin1_1920x1080_10bit_25Hz_P422_180frames.yuv",
        "file_format": "YUV422PLANAR10LE",
        "format": "YUV_422_10bit",
        "width": 1920,
        "height": 1080,
    },
)

yuv_files_interlace = dict(
    Crosswalk_480p={
        "filename": "Netflix_Crosswalk_720x480_interlace_10bit_60Hz_P422.yuv",
        "file_format": "YUV422RFC4175PG2BE10",
        "format": "YUV_422_10bit",
        "width": 720,
        "height": 480,
    },
    ParkJoy_576p={
        "filename": "ParkJoy_720x576_interlace_10bit_50Hz_P422.yuv",
        "file_format": "YUV422RFC4175PG2BE10",
        "format": "YUV_422_10bit",
        "width": 720,
        "height": 576,
    },
    Crosswalk_1080p={
        "filename": "Netflix_Crosswalk_1920x1080_interlace_10bit_60Hz_P422.yuv",
        "file_format": "YUV422RFC4175PG2BE10",
        "format": "YUV_422_10bit",
        "width": 1920,
        "height": 1080,
    },
    ParkJoy_1080p={
        "filename": "ParkJoy_1920x1080_interlace_10bit_50Hz_P422.yuv",
        "file_format": "YUV422RFC4175PG2BE10",
        "format": "YUV_422_10bit",
        "width": 1920,
        "height": 1080,
    },
)

yuv_files_422rfc10 = dict(
    Penguin_1080p={
        "filename": "HDR_BBC_v4_008_Penguin1_1920x1080_10bit_25Hz_180frames_yuv422p10be_To_yuv422rfc4175be10.yuv",
        "file_format": "YUV422RFC4175PG2BE10",
        "format": "YUV_422_10bit",
        "width": 1920,
        "height": 1080,
    },
    Penguin_4K={
        "filename": "HDR_BBC_v4_008_Penguin1_3840x2160_10bit_25Hz_P422_180frames_yuv422rfc4175be10.yuv",
        "file_format": "YUV422RFC4175PG2BE10",
        "format": "YUV_422_10bit",
        "width": 3840,
        "height": 2160,
    },
    Penguin_8K={
        "filename": "HDR_BBC_v4_008_Penguin1_7680x4320_10bit_25Hz_P422_To_yuv422rfc4175be10_180frames.yuv",
        "file_format": "YUV422RFC4175PG2BE10",
        "format": "YUV_422_10bit",
        "width": 7680,
        "height": 4320,
    },
    Crosswalk_720p={
        "filename": "Netflix_Crosswalk_1280x720_10bit_60Hz_P422_yuv422p10be_To_yuv422YCBCR10be.yuv",
        "file_format": "YUV422RFC4175PG2BE10",
        "format": "YUV_422_10bit",
        "width": 1280,
        "height": 720,
    },
    Crosswalk_1080p={
        "filename": "Netflix_Crosswalk_1920x1080_10bit_60Hz_P422_yuv422p10be_To_yuv422YCBCR10be.yuv",
        "file_format": "YUV422RFC4175PG2BE10",
        "format": "YUV_422_10bit",
        "width": 1920,
        "height": 1080,
    },
    Crosswalk_4K={
        "filename": "Netflix_Crosswalk_3840x2160_10bit_60Hz_P422_To_yuv422p10be_To_yuv422YCBCR10be.yuv",
        "file_format": "YUV422RFC4175PG2BE10",
        "format": "YUV_422_10bit",
        "width": 3840,
        "height": 2160,
    },
    ParkJoy_720p={
        "filename": "ParkJoy_1280x720_10bit_50Hz_P422_To_yuv422p10be_To_yuv422YCBCR10be.yuv",
        "file_format": "YUV422RFC4175PG2BE10",
        "format": "YUV_422_10bit",
        "width": 1280,
        "height": 720,
    },
    ParkJoy_1080p={
        "filename": "ParkJoy_1920x1080_10bit_50Hz_P422_yuv422p10be_To_yuv422YCBCR10be.yuv",
        "file_format": "YUV422RFC4175PG2BE10",
        "format": "YUV_422_10bit",
        "width": 1920,
        "height": 1080,
    },
    ParkJoy_4K={
        "filename": "ParkJoy_3840x2160_10bit_50Hz_P422_To_yuv422p10be_To_yuv422YCBCR10be.yuv",
        "file_format": "YUV422RFC4175PG2BE10",
        "format": "YUV_422_10bit",
        "width": 3840,
        "height": 2160,
    },
    Pedestrian_720p={
        "filename": "Plalaedit_Pedestrian_10bit_1280x720_30Hz_P420_To_yuv422p10be_To_yuv422YCBCR10be.yuv",
        "file_format": "YUV422RFC4175PG2BE10",
        "format": "YUV_422_10bit",
        "width": 1280,
        "height": 720,
    },
    Pedestrian_1080p={
        "filename": "Plalaedit_Pedestrian_10bit_1920x1080_30Hz_P420_To_yuv422p10be_To_yuv422YCBCR10be.yuv",
        "file_format": "YUV422RFC4175PG2BE10",
        "format": "YUV_422_10bit",
        "width": 1920,
        "height": 1080,
    },
    Pedestrian_4K={
        "filename": "Plalaedit_Pedestrian_10bit_3840x2160_30Hz_P420_To_yuv422p10be_To_yuv422YCBCR10be.yuv",
        "file_format": "YUV422RFC4175PG2BE10",
        "format": "YUV_422_10bit",
        "width": 3840,
        "height": 2160,
    },
    test_4K={
        "filename": "test_3840x2160_for_25fps.yuv",
        "file_format": "YUV422RFC4175PG2BE10",
        "format": "YUV_422_10bit",
        "width": 3840,
        "height": 2160,
    },
    test_8K={
        "filename": "test_8k.yuv",
        "file_format": "YUV422RFC4175PG2BE10",
        "format": "YUV_422_10bit",
        "width": 7680,
        "height": 4320,
    },
)

audio_files = dict(
    PCM8={
        "filename": "bits8_channels2_rate48000.pcm",
        "format": "pcm_s8",
        "channels": 2,
        "sample_rate": 48000,
    },
    PCM16={
        "filename": "bits16_channels2_rate48000.pcm",
        "format": "pcm_s16be",
        "channels": 2,
        "sample_rate": 48000,
    },
    PCM24={
        "filename": "bits24_channels2_rate48000.pcm",
        "format": "pcm_s24be",
        "channels": 2,
        "sample_rate": 48000,
    },
)

anc_files = dict(
    text_p29={
        "filename": "test.txt",
        "fps": "p29",
    },
    text_p50={
        "filename": "test.txt",
        "fps": "p50",
    },
    text_p59={
        "filename": "test.txt",
        "fps": "p59",
    },
)

blob_files = dict(
    random_bin_100M={
        "filename": "random_data.bin",
        "max_payload_size": 102400,
    },
)

st41_files = dict(
    st41_short={
        "filename": "st41_short_test.txt",
    },
    st41_p29_long_file={
        "filename": "st41_long_test.txt",
    },
)
