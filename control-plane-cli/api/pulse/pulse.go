/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package pulse

import (
	"context"
	"fmt"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"

	pb "control-plane-cli/api/pulse/proto/pulse"
)

var gcli pb.PulseAPIClient
var gcliConn *grpc.ClientConn

func InitClient() error {
	gcliConn, err := grpc.NewClient(
		"localhost:50061",
		grpc.WithTransportCredentials(insecure.NewCredentials()),
	)
	if err != nil {
		return fmt.Errorf("failed to create gRPC client: %v", err)
	}
	gcli = pb.NewPulseAPIClient(gcliConn)
	return nil
}

func CloseClient() {
	if gcliConn != nil {
		gcliConn.Close()
	}
}

func GetProxyList(ctx context.Context, ids []string, outputFormat string) (string, error) {
	resp, err := gcli.ProxyList(ctx, &pb.ListRequest{
		Ids:          ids,
		OutputFormat: outputFormat,
	})
	if err != nil {
		return "", err
	}
	return resp.GetText(), nil
}

func GetConnList(ctx context.Context, ids []string, outputFormat string) (string, error) {
	resp, err := gcli.ConnectionList(ctx, &pb.ListRequest{
		Ids:          ids,
		OutputFormat: outputFormat,
	})
	if err != nil {
		return "", err
	}
	return resp.GetText(), nil
}

func GetBridgeList(ctx context.Context, ids []string, outputFormat string) (string, error) {
	resp, err := gcli.BridgeList(ctx, &pb.ListRequest{
		Ids:          ids,
		OutputFormat: outputFormat,
	})
	if err != nil {
		return "", err
	}
	return resp.GetText(), nil
}

func GetMultipointGroupList(ctx context.Context, ids []string, outputFormat string) (string, error) {
	resp, err := gcli.MultipointGroupList(ctx, &pb.ListRequest{
		Ids:          ids,
		OutputFormat: outputFormat,
	})
	if err != nil {
		return "", err
	}
	return resp.GetText(), nil
}
