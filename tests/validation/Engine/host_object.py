# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2025 Intel Corporation
# Media Communications Mesh
import paramiko

from abc import ABC, abstractmethod
from typing import Optional

class Connection(ABC):
    # TODO prepare full ssh conenction class
    def __init__(self, ip: str):
        """
        Initialize  Connection

        :param ip: IP Address of host
        """
        self.ip = ip

    @abstractmethod
    def connect(self):
        """
        Establish connection
        """
        pass

    @abstractmethod
    def execute(self, cmd: str):
        """
        Execute command
        :param cmd: command to be executed
        """
        pass

    @abstractmethod
    def start_process(self, cmd: str):
        """
        Execute cmd in background
        :param cmd: command to be executed in background
        """
        pass

    @abstractmethod
    def disconnect(self):
        """
        Close connection
        """
        pass


class SSHConenction(Connection):
    # TODO prepare full ssh conenction class

    def __init__(self, ip: str):
        """
        Initialize SSH Connection

        :param ip: IP Address of host

        """
        super().__init__(self, ip)
        self.connect()

    def connect(self):
        """
        Establish connection
        """
        self.conn = paramiko.SSHClient()
        self.conn.connect()

    def execute(self, cmd: str):
        """
        Execute command
        :param cmd: command to be executed
        """
        return self.conn.exec_command(cmd)


class LocalConnection(Connection):
    # TODO move things from execute.py
    def __init__(self, ip):
        """
        Initialize local conenction class
        :param ip: managament ip of local host
        """
        super().__init__(self, ip)

    def execute(self, cmd):
        """
        Execute command
        :param cmd: command to be executed
        """
        from .execute import run
        return run(testcmd=cmd)


class Host:
    # TODO: host shall include all information about NICs, IPs for Media proxy and ffmpeg/TxRxApp execution
    def __init__(self, ip: str, connection: Connection, nics: str = "", hostname: str = ""):
        """
        Initialize Host class
        :param ip: managament IP of the host
        :param connection: created connection for to host
        :param nics: adapters will be used during test execution
        :param hostname: name of the host for identification purposes
        """
        self.nics = nics
        self.ip = ip
        self.name = hostname
        self.conn = connection

    def execute(self, cmd):
        """
        Execute command
        :param cmd: command to be executed
        """
        return self.conn.execute(cmd)


class MediaProxy:
    """
    Media proxy class, run and collect logs from MCM media proxy
    """
    def __init__(
        self,
        connection,
        sdk: Optional[int] = 8002,
        agent: Optional[str] = "localhost:50051",
        st2110_dev: Optional[str] = "0000:31:00.0",
        st2110_ip: Optional[str] = "192.168.96.1",
        rdma_ip: Optional[str] = "192.168.96.2",
        rdma_ports: Optional[str] = "9100-9999"
    ):
        """
        :param connection: connection object for executing run command
        :param sdk: Port number for SDK API server. Default is 8002
        :param agent: MCM Agent Proxy API address in the format host:port. Default is 'localhost:50051'
        :param st2110_dev: PCI device port for SMPTE 2110 media data transportation.
        :param st2110_ip: IP address for SMPTE 2110. Default is '192.168.96.1'.
        :param rdma_ip: IP address for RDMA. Default is '192.168.96.2'
        :param rdma_ports: Local port ranges for incoming RDMA connections. Default is '9100-9999'
        """
        self.conn = connection
        self.sdk = sdk
        self.agent = agent
        self.st2110_dev = st2110_dev
        self.st2110_ip = st2110_ip
        self.rdma_ip = rdma_ip
        self.rdma_ports = rdma_ports
        self._process = None

    @property
    def prepare_media_command(self):
        """
        Prepare media proxy command
        """
        return ""

    def run_media_proxy(self):
        """
        Start media proxy
        """
        self._process = self.conn.start_process(self.prepare_media_command)

    def stop_media(self):
        """
        Stop media proxy
        """
        self._process.stop()
