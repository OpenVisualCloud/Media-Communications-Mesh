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
	"sync"

	"github.com/sirupsen/logrus"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	pb "control-plane-agent/api/proxy/proto/mediaproxy"
	"control-plane-agent/internal/event"
	"control-plane-agent/internal/model"
	"control-plane-agent/internal/registry"
	"control-plane-agent/internal/telemetry"
)

type Config struct {
	ListenPort int
}

type API struct {
	pb.UnimplementedProxyAPIServer

	cfg Config
	srv *grpc.Server

	requestMap   map[string]model.CommandRequest
	requestMapMx sync.Mutex
}

func NewAPI(cfg Config) *API {
	return &API{
		cfg:        cfg,
		requestMap: make(map[string]model.CommandRequest),
	}
}

func (a *API) CancelCommandRequest(reqId string) {
	a.requestMapMx.Lock()
	defer a.requestMapMx.Unlock()
	req, ok := a.requestMap[reqId]
	if ok {
		delete(a.requestMap, reqId)
		req.Reply <- model.CommandReply{Err: errors.New("command request cancelled by initiator")}
		close(req.Reply)
	}
}

// RegisterMediaProxy
func (a *API) RegisterMediaProxy(ctx context.Context, in *pb.RegisterMediaProxyRequest) (*pb.RegisterMediaProxyReply, error) {
	if in == nil {
		return nil, errors.New("proxy register request")
	}

	params := map[string]interface{}{
		"sdk_api_port": in.SdkApiPort,
	}

	if in.St2110Config != nil {
		params["st2110.dev_port_bdf"] = in.St2110Config.DevPortBdf
		params["st2110.dataplane_ip_addr"] = in.St2110Config.DataplaneIpAddr
	}

	if in.RdmaConfig != nil {
		params["rdma.dataplane_ip_addr"] = in.RdmaConfig.DataplaneIpAddr
		params["rdma.dataplane_local_ports"] = in.RdmaConfig.DataplaneLocalPorts
	}

	reply := event.PostEventSync(ctx, event.OnRegisterMediaProxy, params)
	if reply.Err != nil {
		logrus.Errorf("Proxy register req err: %v", reply.Err)
		return nil, reply.Err
	}

	id, ok := reply.Ctx.Value(event.ParamName("proxy_id")).(string)
	if !ok {
		return nil, errors.New("proxy register request: registry reported no proxy id")
	}

	event.PostEventAsync(reply.Ctx, event.OnRegisterMediaProxyOk, map[string]interface{}{
		"proxy_id": id,
	})
	return &pb.RegisterMediaProxyReply{ProxyId: id}, nil
}

// UnregisterMediaProxy
func (a *API) UnregisterMediaProxy(ctx context.Context, in *pb.UnregisterMediaProxyRequest) (*pb.UnregisterMediaProxyReply, error) {
	if in == nil {
		return nil, errors.New("proxy unregister request")
	}

	reply := event.PostEventSync(ctx, event.OnUnregisterMediaProxy, map[string]interface{}{
		"proxy_id": in.ProxyId,
	})
	if reply.Err != nil {
		logrus.Errorf("Proxy unregister req err: %v", reply.Err)
	}

	event.PostEventAsync(reply.Ctx, event.OnUnregisterMediaProxyOk, nil)
	return &pb.UnregisterMediaProxyReply{}, nil
}

// RegisterConnection
func (a *API) RegisterConnection(ctx context.Context, in *pb.RegisterConnectionRequest) (*pb.RegisterConnectionReply, error) {
	if in == nil {
		return nil, errors.New("nil register conn request")
	}

	// If conn_id is not empty, this request is sent by Media Proxy to recover after a connection loss.
	// So, if the connection exists in the registry, reply with success and exit.
	// Otherwise, return an error, which should force Media Proxy to delete the existing connection.
	if len(in.ConnId) > 0 {
		_, err := registry.ConnRegistry.Get(ctx, in.ConnId, false)
		return &pb.RegisterConnectionReply{ConnId: in.ConnId}, err
	}

	if in.Config == nil {
		return nil, errors.New("nil register conn config")
	}
	logrus.Infof("Conn register %+v", in.Config)

	config := &model.SDKConnectionConfig{}
	err := config.AssignFromPb(in.Config)
	if err != nil {
		return nil, err
	}

	groupURN, err := config.GetMultipointGroupURN()
	if err != nil {
		return nil, err
	}

	reply := event.PostEventSync(ctx, event.OnRegisterConnection, map[string]interface{}{
		"proxy_id":    in.ProxyId,
		"kind":        in.Kind,
		"conn_id":     in.ConnId, // Normally, this should be empty
		"conn_config": config,
		"conn_type":   config.ConnType(),
		"group_id":    groupURN, // Here the group URN becomes Group ID
	})
	if reply.Err != nil {
		logrus.Errorf("Conn register req err: %v", reply.Err)
	}

	errIncompat, ok := reply.Ctx.Value(event.ParamName("err_incompatible")).(error)
	if ok {
		return nil, errIncompat
	}

	id, ok := reply.Ctx.Value(event.ParamName("conn_id")).(string)
	if !ok {
		return nil, errors.New("conn register request: registry reported no conn id")
	}

	event.PostEventAsync(reply.Ctx, event.OnRegisterConnectionOk, map[string]interface{}{
		"proxy_id": in.ProxyId,
	})
	return &pb.RegisterConnectionReply{ConnId: id}, nil
}

// UnregisterConnection
func (a *API) UnregisterConnection(ctx context.Context, in *pb.UnregisterConnectionRequest) (*pb.UnregisterConnectionReply, error) {
	if in == nil {
		return nil, errors.New("nil unregister conn request")
	}

	conn, err := registry.ConnRegistry.Get(ctx, in.ConnId, false)
	if err != nil {
		logrus.Errorf("Can't find connection %v", in.ConnId)
		return nil, err
	}

	reply := event.PostEventSync(ctx, event.OnUnregisterConnection, map[string]interface{}{
		"conn_id":  in.ConnId,
		"proxy_id": in.ProxyId,
		"group_id": conn.GroupId,
	})
	if reply.Err != nil {
		logrus.Errorf("Conn unregister req err: %v", reply.Err)
	}

	event.PostEventAsync(reply.Ctx, event.OnUnregisterConnectionOk, nil)
	return &pb.UnregisterConnectionReply{}, nil
}

// StartCommandQueue
func (a *API) StartCommandQueue(in *pb.StartCommandQueueRequest, stream pb.ProxyAPI_StartCommandQueueServer) error {
	if in == nil {
		return errors.New("nil start command queue request")
	}
	proxyId := in.ProxyId
	proxy, err := registry.MediaProxyRegistry.Get(stream.Context(), proxyId, false)
	if err != nil {
		return status.Error(codes.NotFound, err.Error())
	}

	err = registry.MediaProxyRegistry.Update_SetActive(stream.Context(), proxyId, true)
	if err != nil {
		return err
	}

	event.PostEventAsync(stream.Context(), event.OnActivateMediaProxy, map[string]interface{}{
		"proxy_id": proxyId,
	})

	// Running a command queue for the given Media Proxy
	for {
		req, err := proxy.GetNextCommandRequest(stream.Context())
		if err != nil {
			// Commented this out to avoid closing the command stream after network glitches.
			// TODO: Check and revert back if needed.
			// proxy.Deinit()
			return err
		}

		a.requestMapMx.Lock()
		a.requestMap[req.PbRequest.ReqId] = req
		a.requestMapMx.Unlock()

		err = stream.Send(req.PbRequest)
		if err != nil {
			logrus.Errorf("Error sending command: proxy_id: %v, msg_id: %v, err: %v", proxyId, req.PbRequest.ReqId, err)

			a.requestMapMx.Lock()
			delete(a.requestMap, req.PbRequest.ReqId)
			a.requestMapMx.Unlock()

			req.Reply <- model.CommandReply{Err: err}
			close(req.Reply)
		}
	}
}

// SendCommandReply
func (a *API) SendCommandReply(ctx context.Context, in *pb.CommandReply) (*pb.CommandReplyReceipt, error) {
	if in == nil {
		return nil, errors.New("nil command reply request")
	}

	// logrus.Debugf("CommandReply received")

	a.requestMapMx.Lock()
	req, ok := a.requestMap[in.ReqId]
	if ok {
		delete(a.requestMap, in.ReqId)
	}
	a.requestMapMx.Unlock()

	if !ok {
		return nil, errors.New("command reply id not found")
	}
	if req.ProxyId != in.ProxyId {
		return nil, fmt.Errorf("bad proxy id: %v, %v, req id: %v", in.ProxyId, req.ProxyId, in.ReqId)
	}

	select {
	case <-ctx.Done():
		return nil, ctx.Err()
	case req.Reply <- model.CommandReply{Data: in.Reply}:
		return &pb.CommandReplyReceipt{}, nil
	}
}

func (s *API) SendMetrics(ctx context.Context, req *pb.SendMetricsRequest) (*pb.SendMetricsReply, error) {
	if req == nil {
		return nil, errors.New("nil send metrics request")
	}

	proxyId := req.ProxyId

	_, err := registry.MediaProxyRegistry.Get(ctx, proxyId, false)
	if err != nil {
		return nil, fmt.Errorf("send metrics req err: %v", err)
	}

	for _, v := range req.Metrics {
		// logrus.Debugf("Received metric [%v] %v %v", time.UnixMilli(v.TimestampMs).Format("2006-01-02T15:04:05.000Z07:00"), v.ProviderId, v.Fields)
		metric := telemetry.NewMetric(v.TimestampMs)
		for _, field := range v.Fields {
			switch value := field.Value.(type) {
			case *pb.MetricField_StrValue:
				metric.Fields[field.Name] = value.StrValue
			case *pb.MetricField_UintValue:
				metric.Fields[field.Name] = value.UintValue
			case *pb.MetricField_DoubleValue:
				metric.Fields[field.Name] = value.DoubleValue
			case *pb.MetricField_BoolValue:
				metric.Fields[field.Name] = value.BoolValue
			}
		}
		telemetry.Storage.AddMetric(v.ProviderId, metric)
	}
	return &pb.SendMetricsReply{}, nil
}

func (a *API) Run(ctx context.Context) {
	lc := net.ListenConfig{}
	lis, err := lc.Listen(ctx, "tcp", fmt.Sprintf(":%d", a.cfg.ListenPort))
	if err != nil {
		logrus.Errorf("Media Proxy API listen err: %v", err)
		return
	}

	a.srv = grpc.NewServer()
	pb.RegisterProxyAPIServer(a.srv, a)

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
