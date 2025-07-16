# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh

from Engine.rx_tx_app_connection import (ConnectionMode, Rdma, St2110,
                                             TransportType)

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
