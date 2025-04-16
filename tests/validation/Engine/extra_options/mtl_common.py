# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2025 Intel Corporation
# Media Communications Mesh

# generates typed-ffmpeg's extra_options for Media Transport Library based on
# https://github.com/OpenVisualCloud/Media-Transport-Library/blob/main/ecosystem/ffmpeg_plugin/mtl_common.h

class MtlCommonRx:
    def __init__(
        self,
        # MTL_RX_DEV_ARGS
        p_port: str = None,
        p_sip: str = None,
        dma_dev: str = None,
        rx_queues: int = -1,
        tx_queues: int = -1,
        # MTL_RX_PORT_ARGS
        p_rx_ip: str = None,
        udp_port: int = 20000,
        payload_type: int = 112,
    ):
        self.p_port = p_port
        self.p_sip = p_sip
        self.dma_dev = dma_dev
        self.rx_queues = rx_queues
        self.tx_queues = tx_queues
        self.p_rx_ip = p_rx_ip
        self.udp_port = udp_port
        self.payload_type = payload_type

    def get_items(self):
        response = {}
        for key, value in self.__dict__.items():
            if value:
                response[key] = value
        return response


class MtlCommonTx:
    def __init__(
        self,
        # MTL_TX_DEV_ARGS
        p_port: str = None,
        p_sip: str = None,
        dma_dev: str = None,
        rx_queues: int = -1,
        tx_queues: int = -1,
        # MTL_TX_PORT_ARGS
        p_tx_ip: str = None,
        udp_port: int = 20000,
        payload_type: int = 112,
    ):
        self.p_port = p_port
        self.p_sip = p_sip
        self.dma_dev = dma_dev
        self.rx_queues = rx_queues
        self.tx_queues = tx_queues
        self.p_tx_ip = p_tx_ip
        self.udp_port = udp_port
        self.payload_type = payload_type

    def get_items(self):
        response = {}
        for key, value in self.__dict__.items():
            if value:
                response[key] = value
        return response
