/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package pulse

import (
	"context"
	"fmt"
	"strings"

	controlplane "control-plane-agent/api/control-plane"
	pb "control-plane-agent/api/pulse/proto/pulse"
	"control-plane-agent/internal/registry"

	"github.com/olekukonko/tablewriter"
)

func (a *API) BridgeList(ctx context.Context, req *pb.ListRequest) (*pb.Reply, error) {
	bridges, err := registry.BridgeRegistry.List(ctx, req.GetIds(), true, true)
	if err != nil {
		return &pb.Reply{
			Text: err.Error(),
		}, nil
	}

	for i := range bridges {
		controlplane.PopulateConnMetrics(bridges[i].Status, bridges[i].Id)
	}

	compact := req.GetOutputFormat() == "table-compact"
	out := renderOutput(req.GetOutputFormat(), bridges, func(table *tablewriter.Table) {
		row := []string{
			"BRIDGE ID",
			"KIND",
			"TYPE",
		}
		if !compact {
			row = append(row, "PAYLOAD")
		}
		row = append(row,
			"GROUP",
			"PROXY",
			"STATE",
			"ERRORS",
			"TXN",
			"TPS",
			"BYTES",
			"MBIT/S",
			"NOTES",
		)
		table.Append(row)
		for _, v := range bridges {
			var bw float64
			var cnt uint64
			var notes string
			if v.Config.Kind == "tx" {
				bw = v.Status.OutboundBandwidth
				cnt = v.Status.OutboundBytes
			} else {
				bw = v.Status.InboundBandwidth
				cnt = v.Status.InboundBytes
			}
			if v.Config.Type == "rdma" {
				if v.Config.Kind == "tx" {
					notes = fmt.Sprintf("-> %v:%v", v.Config.RDMA.RemoteIPAddr, v.Config.RDMA.Port)
				} else {
					notes = fmt.Sprintf("%v -> :%v", v.Config.RDMA.RemoteIPAddr, v.Config.RDMA.Port)
				}
			}
			if v.Config.Type == "st2110" {
				if v.Config.Kind == "tx" {
					notes = fmt.Sprintf("-> %v:%v", v.Config.ST2110.IPAddr, v.Config.ST2110.Port)
				} else {
					notes = fmt.Sprintf("%v -> :%v", v.Config.ST2110.IPAddr, v.Config.ST2110.Port)
				}
			}

			var proxy string
			if len(v.ProxyName) > 0 {
				proxy = v.ProxyName
			} else {
				proxy = v.ProxyId
			}

			row = []string{
				v.Id,
				v.Config.Kind,
				v.Config.Type,
			}
			if !compact {
				row = append(row, payloadToString(&v.Config.SDKConnectionConfig))
			}
			row = append(row,
				v.GroupId,
				proxy,
				colorizedConnState(v.Status.State),
				colorizedErrors(v.Status.Errors),
				fmt.Sprintf("%v", v.Status.TransactionsSucceeded),
				fmt.Sprintf("%v", v.Status.TransactionsPerSecond),
				fmt.Sprintf("%v", cnt),
				fmt.Sprintf("%v", bw),
				notes,
			)
			table.Append(row)
		}
	}, func() string {
		ids := []string{}
		for _, v := range bridges {
			ids = append(ids, v.Id)
		}
		return strings.Join(ids, "\n")
	})

	return &pb.Reply{
		Text: out,
	}, nil
}
