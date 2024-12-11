from enum import Enum

class ConnectionType(Enum):
    MPG = 0,
    ST2110 = 1,
    RDMA = 2


class Connection:
    """Class used to prepare connection part of connection.json config file"""
    connection_type = None
    def __init__(self, connection_type=ConnectionType.MPG)
        self.connection_type = connection_type


class MultipointGroup(Connection):
    """Prepares multipoint-group part of connection.json file"""
    def __init__(
        urn="ipv4:224.0.0.1:9003"
    ):
        self.urn = urn

    def set_multipointgroup(edits: dict):
        self.urn = edits.get("urn", self.urn)


class St2110(Connection):
    """Prepares st2110 part of connection.json file"""
    def __init__(
        transport="st2110-20",
        remoteIpAddr="192.168.95.2",
        remotePort="9002",
        pacing="narrow",
        payloadType=112
    ):
        self.transport = transport
        self.remoteIpAddr = remoteIpAddr
        self.remotePort = remotePort
        self.pacing = pacing
        self.payloadType = payloadType

    def set_st2110(edits: dict):
        self.transport = edits.get("transport", self.transport)
        self.remoteIpAddr = edits.get("remoteIpAddr", self.remoteIpAddr)
        self.remotePort = edits.get("remotePort", self.remotePort)
        self.pacing = edits.get("pacing", self.pacing)
        self.payloadType = edits.get("payloadType", self.payloadType)


class Rdma(Connection):
    """Prepares RDMA part of connection.json file"""
    def __init__(
        connectionMode="RC",
        maxLatencyNs=10000
    ):
        self.connectionMode = connectionMode
        self.maxLatencyNs = maxLatencyNs

    def set_rdma(edits: dict):
        self.connectionMode = edits.get("connectionMode", self.connectionMode)
        self.maxLatencyNs = edits.get("maxLatencyNs", self.maxLatencyNs)
