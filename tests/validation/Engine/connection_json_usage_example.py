# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation
# Intel® Media Communications Mesh

from connection import ConnectionMode, Rdma, TransportType, St2110
from payload import Audio, Video

from connection_json import ConnectionJson

connection = Rdma(
    connectionMode=ConnectionMode.UC,
    maxLatencyNs=30000
)

print(f"""connection info:
connectionMode={connection.connectionMode}
maxLatencyNs={connection.maxLatencyNs}
""")


print(f"""Dict:
{connection.toDict()}
""")

payload = Video(
    width = 3840,
    height = 2160
)

print(f"""payload info:
width={payload.width}
height={payload.height}
fps={payload.fps}
pixelFormat={payload.pixelFormat}
type={payload.payload_type}
""")

print(f"""Dict:
{payload.toDict()}
""")

print("ConnectionJson:")
cj = ConnectionJson(
    bufferQueueCapacity=32,
    payload=payload,
    connection=connection
)

print(f"""Dict:
{cj.toDict()}
""")

print(f"""JSON:
{cj.toJson()}
""")

print("-------------------------------")

conn = St2110(
    transport=TransportType.ST30,
    pacing="wide"
)

print(f"""conn info:
transport={conn.transport}
remoteIpAddr={conn.remoteIpAddr}
remotePort={conn.remotePort}
pacing={conn.pacing}
payloadType={conn.payloadType}
""")


print(f"""Dict:
{conn.toDict()}
""")



pl = Audio(
    channels=4
)

print(f"""pl info:
channels={pl.channels}
type={pl.payload_type}
""")

print(f"""Dict:
{pl.toDict()}
""")

print("ConnectionJson:")
cj = ConnectionJson(
    maxPayloadSize=1024,
    payload=pl,
    connection=conn
)

print(f"""Dict:
{cj.toDict()}
""")

print(f"""JSON:
{cj.toJson()}
""")