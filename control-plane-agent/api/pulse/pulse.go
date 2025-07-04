/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package pulse

import (
	"context"
	"fmt"
	"net"

	"github.com/sirupsen/logrus"
	"google.golang.org/grpc"

	pb "control-plane-agent/api/pulse/proto/pulse"
)

type Config struct {
	ListenPort int
}

type API struct {
	pb.UnimplementedPulseAPIServer

	cfg Config
	srv *grpc.Server
}

func NewAPI(cfg Config) *API {
	return &API{
		cfg: cfg,
	}
}

func (a *API) Run(ctx context.Context) {
	lc := net.ListenConfig{}
	lis, err := lc.Listen(ctx, "tcp", fmt.Sprintf(":%d", a.cfg.ListenPort))
	if err != nil {
		logrus.Errorf("Pulse API listen err: %v", err)
		return
	}

	a.srv = grpc.NewServer()
	pb.RegisterPulseAPIServer(a.srv, a)

	done := make(chan interface{})
	exit := make(chan interface{})
	go func() {
		select {
		case <-ctx.Done():
		case <-exit:
		}
		a.srv.Stop()
		close(done)
	}()

	logrus.Infof("Server starts listening at :%v - Pulse API (gRPC)", a.cfg.ListenPort)

	err = a.srv.Serve(lis)
	if err != nil {
		logrus.Errorf("Pulse API server err: %v", err)
	}

	close(exit)
	<-done
}
