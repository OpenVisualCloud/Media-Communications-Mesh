/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package proxy

import (
	"context"
	"errors"
	"fmt"
	"net"

	"github.com/sirupsen/logrus"
	"google.golang.org/grpc"

	pb "control-plane-agent/api/proxy/proto/mediaproxy"
	"control-plane-agent/internal/event"
	"control-plane-agent/internal/registry"
)

type Config struct {
	ListenPort int
}

type API struct {
	pb.UnimplementedControlAPIServer

	cfg Config
	srv *grpc.Server
}

func NewAPI(cfg Config) *API {
	return &API{
		cfg: cfg,
	}
}

func (a *API) RegisterMediaProxy(ctx context.Context, in *pb.RegisterMediaProxyRequest) (*pb.RegisterMediaProxyReply, error) {
	if in == nil {
		return nil, errors.New("proxy register request")
	}

	reply := event.PostEventSync(ctx, event.OnRegisterMediaProxy, map[string]interface{}{"sdk_port": in.SdkPort})
	if reply.Err != nil {
		logrus.Errorf("Proxy register req err: %v", reply.Err)
	}

	id, ok := reply.Ctx.Value(event.ParamName("proxy_id")).(string)
	if !ok {
		return nil, errors.New("proxy register request: registry reported no proxy id")
	}

	event.PostEventAsync(reply.Ctx, event.OnRegisterMediaProxyOk, nil)
	return &pb.RegisterMediaProxyReply{ProxyId: id}, nil
}

func (a *API) RegisterConnection(ctx context.Context, in *pb.RegisterConnectionRequest) (*pb.RegisterConnectionReply, error) {
	if in == nil {
		return nil, errors.New("nil register conn request")
	}

	reply := event.PostEventSync(ctx, event.OnRegisterConnection, map[string]interface{}{"kind": in.Kind})
	if reply.Err != nil {
		logrus.Errorf("Proxy register req err: %v", reply.Err)
	}

	// TODO: pass the conn params as a model in the context

	// id, err := registry.ConnRegistry.Add(ctx, model.Connection{
	// 	ProxyId: in.ProxyId,
	// 	Config: &model.ConnectionConfig{
	// 		Kind:        strconv.Itoa(int(in.Kind)),
	// 		ConnType:    strconv.Itoa(int(in.ConnType)),
	// 		PayloadType: strconv.Itoa(int(in.PayloadType)),
	// 		BufferSize:  in.GetBufferSize(),
	// 	}})
	// if err != nil {
	// 	return nil, err
	// }

	id, ok := reply.Ctx.Value(event.ParamName("conn_id")).(string)
	if !ok {
		return nil, errors.New("conn register request: registry reported no conn id")
	}

	event.PostEventAsync(reply.Ctx, event.OnRegisterMediaProxyOk, nil)
	return &pb.RegisterConnectionReply{ConnId: id}, nil
}

func (a *API) UnregisterConnection(ctx context.Context, in *pb.UnregisterConnectionRequest) (*pb.UnregisterConnectionReply, error) {
	if in == nil {
		return nil, errors.New("nil unregister conn request")
	}
	err := registry.ConnRegistry.Delete(ctx, in.ConnId)
	if err != nil {
		logrus.Errorf("Can't delete connection %v", in.ConnId)
		return nil, err
	}

	return &pb.UnregisterConnectionReply{}, nil
}

func (a *API) StartCommandQueue(in *pb.StartCommandQueueRequest, stream pb.ControlAPI_StartCommandQueueServer) error {
	if in == nil {
		return errors.New("nil start command queue request")
	}
	id := in.ProxyId
	proxy, err := registry.MediaProxyRegistry.Get(stream.Context(), id, false)
	if err != nil {
		logrus.Errorf("Start command queue failed, id: %v, %v", id, err)
		return err
	}

	// Running a command queue for the given Media Proxy
	for {
		select {
		case <-stream.Context().Done():
			return nil
		case cmd, ok := <-proxy.CommandQueue():
			if !ok {
				return errors.New("command queue closed")
			}
			err := stream.Send(&pb.CommandMessage{Opcode: cmd.OpCode, Id: cmd.Id})
			if err != nil {
				logrus.Errorf("Error sending command: %v, id: %v, err: %v", cmd.OpCode, cmd.Id, err)
				return err
			}
		}
	}
}

func (a *API) Run(ctx context.Context) {
	lc := net.ListenConfig{}
	lis, err := lc.Listen(ctx, "tcp", fmt.Sprintf(":%d", a.cfg.ListenPort))
	if err != nil {
		logrus.Errorf("Media Proxy API listen err: %v", err)
		return
	}

	a.srv = grpc.NewServer()
	pb.RegisterControlAPIServer(a.srv, a)

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

	logrus.Infof("Server starts listening at :%v - Media Proxy API (gRPC)", a.cfg.ListenPort)

	err = a.srv.Serve(lis)
	if err != nil {
		logrus.Errorf("Media Proxy API server err: %v", err)
	}

	close(exit)
	<-done
}
