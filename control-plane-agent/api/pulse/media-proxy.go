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

	pb "control-plane-agent/api/pulse/proto/pulse"
	"control-plane-agent/internal/registry"

	"github.com/olekukonko/tablewriter"
)

func (a *API) ProxyList(ctx context.Context, req *pb.ListRequest) (*pb.Reply, error) {
	proxies, err := registry.MediaProxyRegistry.List(ctx, req.GetIds(), true, true)
	if err != nil {
		return &pb.Reply{
			Text: err.Error(),
		}, nil
	}

	out := renderOutput(req.GetOutputFormat(), proxies, func(table *tablewriter.Table) {
		table.Append([]string{
			"PROXY ID",
			"SDK ADDR",
			"ST2110 BDF",
			"ST2110 IP ADDR",
			"RDMA IP ADDR",
			"RDMA PORTS",
			"STATE",
			"CONNS",
			"BRIDGES",
			"NAME",
		})

		for _, v := range proxies {
			var state string
			if v.Active {
				state = ColorGreen("active")
			} else {
				state = ColorRed("inactive")
			}

			table.Append([]string{
				v.Id,
				fmt.Sprintf("%v:%v", v.Config.ControlplaneAddr, v.Config.SDKAPIPort),
				v.Config.ST2110.DevPortBDF,
				v.Config.ST2110.DataplaneIPAddr,
				v.Config.RDMA.DataplaneIPAddr,
				v.Config.RDMA.DataplaneLocalPorts,
				state,
				fmt.Sprintf("%v", v.Status.ConnsNum),
				fmt.Sprintf("%v", v.Status.BridgesNum),
				v.Name,
			})
		}
	}, func() string {
		ids := []string{}
		for _, v := range proxies {
			ids = append(ids, v.Id)
		}
		return strings.Join(ids, "\n")
	})

	return &pb.Reply{
		Text: out,
	}, nil
}
