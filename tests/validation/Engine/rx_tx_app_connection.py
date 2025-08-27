# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh

from enum import Enum

from Engine.const import (
    DEFAULT_REMOTE_IP_ADDR,
    DEFAULT_REMOTE_PORT,
    DEFAULT_PACING,
    DEFAULT_PAYLOAD_TYPE_ST2110_20,
    DEFAULT_PIXEL_FORMAT,
    DEFAULT_MPG_URN,
    DEFAULT_PAYLOAD_TYPE_ST2110_30,
    DEFAULT_RDMA_MAX_LATENCY_NS,
)


class RxTxAppConnectionType(Enum):
    NONE = -1
    MPG = 0
    ST2110 = 1
    RDMA = 2


class RxTxAppConnection:
    """Class used to prepare connection part of connection.json config file"""

    rx_tx_app_connection_type = None

    def __init__(self, rx_tx_app_connection_type=RxTxAppConnectionType.NONE):
        self.rx_tx_app_connection_type = rx_tx_app_connection_type

    def __call__(self):
        """Make the instance callable to support AppRunnerBase usage.

        This method enables two ways of using connection classes with AppRunnerBase:
        1. Passing a class: AppRunnerBase will instantiate it with default parameters
        2. Passing an instance: AppRunnerBase will call the instance and get it back

        For new code, prefer passing fully configured instances rather than classes
        for better explicitness and maintainability.
        """
        return self


class MultipointGroup(RxTxAppConnection):
    """Prepares multipoint-group part of connection.json file"""

    def __init__(self, urn=DEFAULT_MPG_URN):
        super().__init__(rx_tx_app_connection_type=RxTxAppConnectionType.MPG)
        self.urn = urn

    def set_multipointgroup(self, edits: dict) -> None:
        self.urn = edits.get("urn", self.urn)

    def to_dict(self) -> dict:
        return {"connection": {"multipointGroup": {"urn": self.urn}}}


class TransportType(Enum):
    ST20 = "st2110-20"
    ST22 = "st2110-22"
    ST30 = "st2110-30"
    ST40 = "st2110-40"
    ST41 = "st2110-41"


class St2110(RxTxAppConnection):
    """Prepares st2110 part of connection.json file"""

    transport = None

    def __init__(self, remoteIpAddr, remotePort):
        super().__init__(rx_tx_app_connection_type=RxTxAppConnectionType.ST2110)
        self.remoteIpAddr = remoteIpAddr
        self.remotePort = remotePort

    def to_dict(self):
        return {
            "connection": {
                "st2110": {
                    "transport": self.transport.value,
                    "remoteIpAddr": self.remoteIpAddr,
                    "remotePort": self.remotePort,
                }
            }
        }


class St2110_20(St2110):
    transport = TransportType.ST20

    def __init__(
        self,
        remoteIpAddr=DEFAULT_REMOTE_IP_ADDR,
        remotePort=DEFAULT_REMOTE_PORT,
        pacing=DEFAULT_PACING,
        payloadType=DEFAULT_PAYLOAD_TYPE_ST2110_20,
        transportPixelFormat=DEFAULT_PIXEL_FORMAT,
    ):
        super().__init__(remoteIpAddr=remoteIpAddr, remotePort=remotePort)
        self.pacing = pacing
        self.payloadType = payloadType
        self.transportPixelFormat = transportPixelFormat

    def to_dict(self):
        base_dict = super().to_dict()
        base_dict["connection"]["st2110"].update(
            {
                "pacing": self.pacing,
                "payloadType": self.payloadType,
                "transportPixelFormat": self.transportPixelFormat,
            }
        )
        return base_dict


class St2110_30(St2110):
    transport = TransportType.ST30

    def __init__(
        self,
        remoteIpAddr=DEFAULT_REMOTE_IP_ADDR,
        remotePort=DEFAULT_REMOTE_PORT,
        pacing=DEFAULT_PACING,
        payloadType=DEFAULT_PAYLOAD_TYPE_ST2110_30,
    ):
        super().__init__(remoteIpAddr=remoteIpAddr, remotePort=remotePort)
        self.pacing = pacing
        self.payloadType = payloadType

    def to_dict(self):
        base_dict = super().to_dict()
        base_dict["connection"]["st2110"].update(
            {
                "pacing": self.pacing,
                "payloadType": self.payloadType,
            }
        )
        return base_dict


class ConnectionMode(Enum):
    RC = "RC"
    UC = "UC"
    UD = "UD"
    RD = "RD"


class Rdma(RxTxAppConnection):
    """Prepares RDMA part of connection.json file"""

    def __init__(self, connectionMode=ConnectionMode.RC, maxLatencyNs=DEFAULT_RDMA_MAX_LATENCY_NS):
        super().__init__(rx_tx_app_connection_type=RxTxAppConnectionType.RDMA)
        self.connectionMode = connectionMode
        self.maxLatencyNs = maxLatencyNs

    def set_rdma(self, edits: dict) -> None:
        self.connectionMode = edits.get("connectionMode", self.connectionMode)
        self.maxLatencyNs = edits.get("maxLatencyNs", self.maxLatencyNs)

    def to_dict(self) -> dict:
        return {
            "connection": {
                "rdma": {
                    "connectionMode": self.connectionMode.value,
                    "maxLatencyNs": self.maxLatencyNs,
                }
            }
        }
