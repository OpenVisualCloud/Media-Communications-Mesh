# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh

from mcm.Engine.rx_tx_app_connection import (ConnectionMode, Rdma, St2110,
                                             TransportType)
from mcm.Engine.rx_tx_app_connection_json import ConnectionJson
from mcm.Engine.rx_tx_app_payload import Audio, Video

rx_tx_app_connection_rdma = Rdma(connectionMode=ConnectionMode.UC, maxLatencyNs=30000)

print(
    f"""connection info:
connectionMode={rx_tx_app_connection_rdma.connectionMode}
maxLatencyNs={rx_tx_app_connection_rdma.maxLatencyNs}
"""
)


print(
    f"""Dict:
{rx_tx_app_connection_rdma.to_dict()}
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
cj = ConnectionJson(bufferQueueCapacity=32, payload=payload, rx_tx_app_connection=rx_tx_app_connection_rdma)

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

rx_tx_app_connection_st2110 = St2110(transport=TransportType.ST30, pacing="wide")

print(
    f"""conn info:
transport={rx_tx_app_connection_st2110.transport}
remoteIpAddr={rx_tx_app_connection_st2110.remoteIpAddr}
remotePort={rx_tx_app_connection_st2110.remotePort}
pacing={rx_tx_app_connection_st2110.pacing}
payloadType={rx_tx_app_connection_st2110.payloadType}
"""
)


print(
    f"""Dict:
{rx_tx_app_connection_st2110.to_dict()}
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
cj = ConnectionJson(maxPayloadSize=1024, payload=pl, rx_tx_app_connection=rx_tx_app_connection_st2110)

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
