# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh

LOG_FOLDER = "logs"
DEFAULT_OUTPUT_PATH = "/dev/null"

# time for establishing connection for example between TX and RX in st2110
MTL_ESTABLISH_TIMEOUT = 2
MCM_ESTABLISH_TIMEOUT = 5
DEFAULT_LOOP_COUNT = 7
MCM_RXTXAPP_RUN_TIMEOUT = MCM_ESTABLISH_TIMEOUT * DEFAULT_LOOP_COUNT
MAX_TEST_TIME_DEFAULT = 60
STOP_GRACEFULLY_PERIOD = 2  # seconds

MCM_PATH = "/opt/intel/mcm"  # Default path for MCM installation
MTL_PATH = f"{MCM_PATH}/_build/mtl"  # Default path for Media Transport Library
MEDIA_PROXY_PORT = 8002  # Default port for Media Proxy

DEFAULT_FFMPEG_PATH = "/opt/intel/ffmpeg"  # Default path for FFmpeg installation
DEFAULT_MTL_PATH = "/opt/intel/mtl"  # Default path for Media Transport Library
DEFAULT_OPENH264_PATH = "/opt/intel/openh264"  # Default path for OpenH264 installation

DEFAULT_MCM_FFMPEG_VERSION = "7.0"
DEFAULT_MCM_FFMPEG_PATH = f"{MCM_PATH}/{DEFAULT_MCM_FFMPEG_VERSION.replace('.', '_')}_mcm_build"
DEFAULT_MCM_FFMPEG_LD_LIBRARY_PATH = f"{DEFAULT_MCM_FFMPEG_PATH}/lib"

DEFAULT_MTL_FFMPEG_VERSION = "7.0"
DEFAULT_MTL_FFMPEG_PATH = f"{DEFAULT_FFMPEG_PATH}/{DEFAULT_MTL_FFMPEG_VERSION.replace('.', '_')}_mtl_build"
DEFAULT_MTL_FFMPEG_LD_LIBRARY_PATH = f"{DEFAULT_MTL_FFMPEG_PATH}/lib"

MEDIA_PROXY_ERROR_KEYWORDS = ["[ERRO]"]
MESH_AGENT_ERROR_KEYWORDS = ["[ERRO]"]
RX_TX_APP_ERROR_KEYWORDS = ["[ERRO]"]

DEFAULT_MPG_URN = "ipv4:224.0.0.1:9003"
DEFAULT_REMOTE_IP_ADDR = "239.2.39.238"
DEFAULT_REMOTE_PORT = 20000
DEFAULT_PACING = "narrow"
DEFAULT_PAYLOAD_TYPE_ST2110_20 = 112
DEFAULT_PAYLOAD_TYPE_ST2110_30 = 111
DEFAULT_PIXEL_FORMAT = "yuv422p10le"
DEFAULT_RDMA_MAX_LATENCY_NS = 10000
