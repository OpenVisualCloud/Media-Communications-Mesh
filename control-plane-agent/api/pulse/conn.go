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
	"control-plane-agent/internal/model"
	"control-plane-agent/internal/registry"

	"github.com/olekukonko/tablewriter"
)

func payloadToString(cfg *model.SDKConnectionConfig) string {
	if cfg.Payload.Video != nil {
		str := fmt.Sprintf("video %vx%v %vfps %v",
			cfg.Payload.Video.Width,
			cfg.Payload.Video.Height,
			cfg.Payload.Video.FPS,
			cfg.Payload.Video.PixelFormatStr,
		)
		return str
	}
	if cfg.Payload.Audio != nil {
		str := fmt.Sprintf("audio %vch %v %v %v",
			cfg.Payload.Audio.Channels,
			cfg.Payload.Audio.SampleRateStr,
			cfg.Payload.Audio.FormatStr,
			cfg.Payload.Audio.PacketTimeStr,
		)
		return str
	}
	if cfg.Payload.Blob != nil {
		str := fmt.Sprintf("blob %v",
			cfg.CalculatedPayloadSize,
		)
		return str
	}
	return "?unknown?"
}

func (a *API) ConnectionList(ctx context.Context, req *pb.ListRequest) (*pb.Reply, error) {
	conns, err := registry.ConnRegistry.List(ctx, req.GetIds(), true, true)
	if err != nil {
		return &pb.Reply{
			Text: err.Error(),
		}, nil
	}

	for i := range conns {
		controlplane.PopulateConnMetrics(conns[i].Status, conns[i].Id)
	}

	compact := req.GetOutputFormat() == "table-compact"
	out := renderOutput(req.GetOutputFormat(), conns, func(table *tablewriter.Table) {
		row := []string{
			"CONN ID",
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
			"NAME",
		)
		table.Append(row)
		for _, v := range conns {
			var bw float64
			var cnt uint64
			if v.Config.Kind == "tx" {
				bw = v.Status.OutboundBandwidth
				cnt = v.Status.OutboundBytes
			} else {
				bw = v.Status.InboundBandwidth
				cnt = v.Status.InboundBytes
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
				v.Config.ConnType(),
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
				v.Name,
			)
			table.Append(row)
		}
	}, func() string {
		ids := []string{}
		for _, v := range conns {
			ids = append(ids, v.Id)
		}
		return strings.Join(ids, "\n")
	})

	return &pb.Reply{
		Text: out,
	}, nil
}
