# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh

# Required ordered log phrases for Rx validation
RX_REQUIRED_LOG_PHRASES = [
    '[RX] Launching RX App',
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
    '[RX] Shuting down connection',
    'INFO - memif disconnected!',
    '[INFO] gRPC: connection deleted',
    '[RX] Shuting down client',
]
TX_REQUIRED_LOG_PHRASES = [
    '[TX] Launching TX app',
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
    '[TX] sending video frames',
    '[TX] sending blob packets',
    '[TX] Sending packet: 1',
    '[TX] Graceful shutdown requested',
    '[TX] Shuting down connection',
    '[INFO] gRPC: connection deleted',
    '[TX] Shuting down client',
]

LOG_FOLDER = "logs"
DEFAULT_OUTPUT_PATH = "/dev/null"

# time for establishing connection for example between TX and RX in st2110
MTL_ESTABLISH_TIMEOUT = 2
MCM_ESTABLISH_TIMEOUT = 5
DEFAULT_LOOP_COUNT = 7
MCM_RXTXAPP_RUN_TIMEOUT = MCM_ESTABLISH_TIMEOUT * DEFAULT_LOOP_COUNT
MAX_TEST_TIME_DEFAULT = 60

MEDIA_PROXY_ERROR_KEYWORDS = ["[ERRO]"]
MESH_AGENT_ERROR_KEYWORDS = ["[ERRO]"]
RX_TX_APP_ERROR_KEYWORDS = ["[ERRO]"]

DEFAULT_MPG_URN = "ipv4:224.1.1.1:9003"
DEFAULT_REMOTE_IP_ADDR = "239.2.39.238"
DEFAULT_REMOTE_PORT = 20000
DEFAULT_PACING = "narrow"
DEFAULT_PAYLOAD_TYPE_ST2110_20 = 112
DEFAULT_PAYLOAD_TYPE_ST2110_30 = 111
DEFAULT_PIXEL_FORMAT = "yuv422p10le"
DEFAULT_RDMA_MAX_LATENCY_NS = 10000
