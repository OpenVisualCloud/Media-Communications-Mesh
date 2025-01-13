# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation
# Intel® Media Communications Mesh

from connection import ConnectionMode, Rdma, St2110, TransportType
from connection_json import ConnectionJson
from payload import Audio, Video

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

payload = Video(width=3840, height=2160)

print(
    f"""payload info:
width={payload.width}
height={payload.height}
fps={payload.fps}
pixelFormat={payload.pixelFormat}
type={payload.payload_type}
"""
)

print(
    f"""Dict:
{payload.to_dict()}
"""
)

print("ConnectionJson:")
cj = ConnectionJson(bufferQueueCapacity=32, payload=payload, connection=connection)

print(
    f"""Dict:
{cj.to_dict()}
"""
)

print(
    f"""JSON:
{cj.to_json()}
"""
)

print("-------------------------------")

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


pl = Audio(channels=4)

print(
    f"""pl info:
channels={pl.channels}
type={pl.payload_type}
"""
)

print(
    f"""Dict:
{pl.to_dict()}
"""
)

print("ConnectionJson:")
cj = ConnectionJson(maxPayloadSize=1024, payload=pl, connection=conn)

print(
    f"""Dict:
{cj.to_dict()}
"""
)

print(
    f"""JSON:
{cj.to_json()}
"""
)
