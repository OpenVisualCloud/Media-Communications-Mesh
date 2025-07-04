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

func (a *API) MultipointGroupList(ctx context.Context, req *pb.ListRequest) (*pb.Reply, error) {
	conns, err := registry.MultipointGroupRegistry.List(ctx, req.GetIds(), true, true)
	if err != nil {
		return &pb.Reply{
			Text: err.Error(),
		}, nil
	}

	for i := range conns {
		controlplane.PopulateConnMetrics(conns[i].Status, conns[i].Id)
	}

	out := renderOutput(req.GetOutputFormat(), conns, func(table *tablewriter.Table) {
		table.Append([]string{
			"GROUP ID",
			"TYPE",
			"PAYLOAD",
			"ENGINE",
			"CONNS",
			"BRIDGES",
		})
		for _, v := range conns {
			table.Append([]string{
				v.Id,
				v.Config.ConnType(),
				payloadToString(&v.Config.SDKConnectionConfig),
				v.Config.Options.Engine,
				fmt.Sprintf("%v", len(v.ConnIds)),
				fmt.Sprintf("%v", len(v.BridgeIds)),
			})
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
