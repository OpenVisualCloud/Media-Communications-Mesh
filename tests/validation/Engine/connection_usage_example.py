# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation
# Intel® Media Communications Mesh

from connection import ConnectionMode, Rdma, St2110, TransportType

connection = Rdma(connectionMode=ConnectionMode.UC, maxLatencyNs=30000)

print(
    f"""connection info:
connectionMode={connection.connectionMode}
maxLatencyNs={connection.maxLatencyNs}
"""
)


print(
    f"""Dict:
{connection.to_dict()}
"""
)

conn = St2110(transport=TransportType.ST30, pacing="wide")

print(
    f"""conn info:
transport={conn.transport}
remoteIpAddr={conn.remoteIpAddr}
remotePort={conn.remotePort}
pacing={conn.pacing}
payloadType={conn.payloadType}
"""
)


print(
    f"""Dict:
{conn.to_dict()}
"""
)
