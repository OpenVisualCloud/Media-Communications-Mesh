# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation
# Intel® Media Communications Mesh

from enum import Enum


class ConnectionType(Enum):
    MPG = (0,)
    ST2110 = (1,)
    RDMA = 2


class Connection:
    """Class used to prepare connection part of connection.json config file"""

    connection_type = None

    def __init__(self, connection_type=ConnectionType.MPG):
        self.connection_type = connection_type

    def toDict(self) -> dict:
        if self.connection_type == ConnectionType.MPG:
            return {"connection": {"multipoint-group": {"urn": self.urn}}}
        elif self.connection_type == ConnectionType.ST2110:
            return {
                "connection": {
                    "st2110": {
                        "transport": self.transport.value[0],
                        "remoteIpAddr": self.remoteIpAddr,
                        "remotePort": self.remotePort,
                        "pacing": self.pacing,
                        "payloadType": self.payloadType,
                    }
                }
            }
        elif self.connection_type == ConnectionType.RDMA:
            return {
                "connection": {
                    "rdma": {
                        "connectionMode": self.connectionMode.value,
                        "maxLatencyNs": self.maxLatencyNs,
                    }
                }
            }


class MultipointGroup(Connection):
    """Prepares multipoint-group part of connection.json file"""

    def __init__(self, urn="ipv4:224.0.0.1:9003"):
        self.urn = urn
        self.connection_type = ConnectionType.MPG

    def set_multipointgroup(self, edits: dict):
        self.urn = edits.get("urn", self.urn)


class TransportType(Enum):
    ST20 = ("st2110-20",)
    ST22 = ("st2110-22",)
    ST30 = ("st2110-30",)
    ST40 = ("st2110-40",)
    ST41 = "st2110-41"


class St2110(Connection):
    """Prepares st2110 part of connection.json file"""

    def __init__(
        self,
        transport=TransportType.ST20,
        remoteIpAddr="192.168.95.2",
        remotePort="9002",
        pacing="narrow",
        payloadType=112,
    ):
        self.transport = transport
        self.remoteIpAddr = remoteIpAddr
        self.remotePort = remotePort
        self.pacing = pacing
        self.payloadType = payloadType
        self.connection_type = ConnectionType.ST2110

    def set_st2110(self, edits: dict):
        self.transport = edits.get("transport", self.transport)
        self.remoteIpAddr = edits.get("remoteIpAddr", self.remoteIpAddr)
        self.remotePort = edits.get("remotePort", self.remotePort)
        self.pacing = edits.get("pacing", self.pacing)
        self.payloadType = edits.get("payloadType", self.payloadType)


class ConnectionMode(Enum):
    RC = "RC"
    UC = "UC"
    UD = "UD"
    RD = "RD"


class Rdma(Connection):
    """Prepares RDMA part of connection.json file"""

    def __init__(self, connectionMode=ConnectionMode.RC, maxLatencyNs=10000):
        self.connectionMode = connectionMode
        self.maxLatencyNs = maxLatencyNs
        self.connection_type = ConnectionType.RDMA

    def set_rdma(self, edits: dict):
        self.connectionMode = edits.get("connectionMode", self.connectionMode)
        self.maxLatencyNs = edits.get("maxLatencyNs", self.maxLatencyNs)
