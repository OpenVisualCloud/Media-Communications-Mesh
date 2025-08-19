# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh

# Required ordered log phrases for Rx validation
RX_REQUIRED_LOG_PHRASES = [
    '[RX] Launching Rx App',
    '[RX] Reading client configuration',
    '[RX] Reading connection configuration',
    '[DEBU] JSON client config:',
    '[INFO] Media Communications Mesh SDK version',
    '[DEBU] JSON conn config:',
    '[RX] Waiting for packets',
    '[RX] Fetched mesh data buffer',
    '[RX] Saving buffer data to a file',
    '[RX] Frame: 1',
    '[RX] Done reading the data',
    '[RX] dropping connection to media-proxy',
    '[RX] Shutting down connection',
    'INFO - memif disconnected!',
    '[INFO] gRPC: connection deleted',
    '[RX] Shutting down client',
]
TX_REQUIRED_LOG_PHRASES = [
    '[TX] Launching Tx App',
    '[TX] Reading client configuration',
    '[TX] Reading connection configuration',
    '[DEBU] JSON client config:',
    '[INFO] Media Communications Mesh SDK version',
    '[DEBU] JSON conn config:',
    '[INFO] gRPC: connection created',
    'INFO - Create memif socket.',
    'INFO - Create memif interface.',
    'INFO - memif connected!',
    '[INFO] gRPC: connection active',
    '[TX] sending',
    '[TX] Shutting down connection',
    '[INFO] gRPC: connection deleted',
    '[TX] Shutting down client',
]

LOG_FOLDER = "logs"
DEFAULT_MEDIA_PATH = "/mnt/media/"
DEFAULT_INPUT_PATH = "/opt/intel/input_path/"
DEFAULT_OUTPUT_PATH = "/opt/intel/output_path/"

# time for establishing connection for example between TX and RX in st2110
MTL_ESTABLISH_TIMEOUT = 2
MCM_ESTABLISH_TIMEOUT = 5
DEFAULT_LOOP_COUNT = 7
MCM_RXTXAPP_RUN_TIMEOUT = MCM_ESTABLISH_TIMEOUT * DEFAULT_LOOP_COUNT
MAX_TEST_TIME_DEFAULT = 60
STOP_GRACEFULLY_PERIOD = 2  # seconds

BUILD_DIR = "_build"
INTEL_BASE_PATH = "/opt/intel"  # Base path for Intel software
MCM_PATH = f"{INTEL_BASE_PATH}/mcm"  # Path for MCM repository
MTL_PATH = f"{INTEL_BASE_PATH}/mtl"  # Path for Media Transport Library repository

# Built binaries paths
MCM_BUILD_PATH = f"{INTEL_BASE_PATH}/_build/mcm"  # Path for MCM built binaries
MTL_BUILD_PATH = f"{INTEL_BASE_PATH}/_build/mtl"  # Path for MTL built binaries
MEDIA_PROXY_PORT = 8002  # Default port for Media Proxy

DEFAULT_FFMPEG_PATH = f"{INTEL_BASE_PATH}/ffmpeg"  # Path for FFmpeg repository
DEFAULT_OPENH264_PATH = f"{INTEL_BASE_PATH}/openh264"  # Path for OpenH264 installation

OPENH264_VERSION_TAG = "openh264v2.4.0"

ALLOWED_FFMPEG_VERSIONS = ["6.1", "7.0"]
DEFAULT_MCM_FFMPEG_VERSION = "7.0"
DEFAULT_MCM_FFMPEG_PATH = f"{INTEL_BASE_PATH}/_build/ffmpeg-{DEFAULT_MCM_FFMPEG_VERSION}/ffmpeg-{DEFAULT_MCM_FFMPEG_VERSION.replace('.', '-')}_mcm_build"
DEFAULT_MCM_FFMPEG_LD_LIBRARY_PATH = f"{DEFAULT_MCM_FFMPEG_PATH}/lib"

DEFAULT_MTL_FFMPEG_VERSION = "7.0"
DEFAULT_MTL_FFMPEG_PATH = f"{INTEL_BASE_PATH}/_build/ffmpeg-{DEFAULT_MTL_FFMPEG_VERSION}/ffmpeg-{DEFAULT_MTL_FFMPEG_VERSION.replace('.', '-')}_mtl_build"
DEFAULT_MTL_FFMPEG_LD_LIBRARY_PATH = f"{DEFAULT_MTL_FFMPEG_PATH}/lib"

# Input/Output paths for media files
DEFAULT_MEDIA_PATH = "/mnt/media/"
DEFAULT_INPUT_PATH = f"{INTEL_BASE_PATH}/input_path"
DEFAULT_OUTPUT_PATH = f"{INTEL_BASE_PATH}/output_path"

MEDIA_PROXY_ERROR_KEYWORDS = ["[ERRO]"]
MESH_AGENT_ERROR_KEYWORDS = ["[ERRO]"]
RX_TX_APP_ERROR_KEYWORDS = ["[ERRO]"]

DEFAULT_MPG_URN = "ipv4:224.0.0.1"
DEFAULT_REMOTE_IP_ADDR = "239.2.39.238"
DEFAULT_REMOTE_PORT = 20000
DEFAULT_PACING = "narrow"
DEFAULT_PAYLOAD_TYPE_ST2110_20 = 112
DEFAULT_PAYLOAD_TYPE_ST2110_30 = 111
DEFAULT_PIXEL_FORMAT = "yuv422p10le"
DEFAULT_RDMA_MAX_LATENCY_NS = 10000
