/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package proxy

import (
	"context"
	"fmt"
	"sync"
	"testing"

	"github.com/stretchr/testify/require"

	pb "control-plane-agent/api/proxy/proto/mediaproxy"
	"control-plane-agent/api/proxy/proto/sdk"
	"control-plane-agent/internal/event"
	"control-plane-agent/internal/model"
	"control-plane-agent/internal/registry"
	"control-plane-agent/internal/telemetry"
)

type MockEventhandler struct {
	fn func(ctx context.Context, e event.Event) event.Reply
}

func (h *MockEventhandler) HandleEvent(ctx context.Context, e event.Event) event.Reply {
	if h.fn != nil {
		return h.fn(ctx, e)
	}
	return event.Reply{Ctx: ctx}
}

func TestProxyAPI_RegisterMediaProxy(t *testing.T) {
	cases := []struct {
		req                 *pb.RegisterMediaProxyRequest
		expectedReply       *pb.RegisterMediaProxyReply
		expectedEventParams map[string]interface{}
	}{
		{
			req: &pb.RegisterMediaProxyRequest{
				SdkApiPort: 2345,
				St2110Config: &pb.ST2110ProxyConfig{
					DevPortBdf:      "0000:32:01.0",
					DataplaneIpAddr: "192.168.96.10",
				},
				RdmaConfig: &pb.RDMAProxyConfig{
					DataplaneIpAddr:     "192.168.97.10",
					DataplaneLocalPorts: "9100-9199",
				},
			},
			expectedReply: &pb.RegisterMediaProxyReply{
				ProxyId: "1234567890",
			},
			expectedEventParams: map[string]interface{}{
				"sdk_api_port":               uint32(2345),
				"st2110.dev_port_bdf":        "0000:32:01.0",
				"st2110.dataplane_ip_addr":   "192.168.96.10",
				"rdma.dataplane_ip_addr":     "192.168.97.10",
				"rdma.dataplane_local_ports": "9100-9199",
			},
		},
		{
			req: &pb.RegisterMediaProxyRequest{
				SdkApiPort: 3456,
			},
			expectedReply: &pb.RegisterMediaProxyReply{
				ProxyId: "0123456789",
			},
			expectedEventParams: map[string]interface{}{
				"sdk_api_port": uint32(3456),
			},
		},
		{
			req: &pb.RegisterMediaProxyRequest{
				SdkApiPort: 4567,
				St2110Config: &pb.ST2110ProxyConfig{
					DevPortBdf:      "0000:41:01.0",
					DataplaneIpAddr: "192.168.56.10",
				},
			},
			expectedReply: &pb.RegisterMediaProxyReply{
				ProxyId: "2345678901",
			},
			expectedEventParams: map[string]interface{}{
				"sdk_api_port":             uint32(4567),
				"st2110.dev_port_bdf":      "0000:41:01.0",
				"st2110.dataplane_ip_addr": "192.168.56.10",
			},
		},
		{
			req: &pb.RegisterMediaProxyRequest{
				SdkApiPort: 5678,
				RdmaConfig: &pb.RDMAProxyConfig{
					DataplaneIpAddr:     "192.168.26.10",
					DataplaneLocalPorts: "8100,8105,8350-8900",
				},
			},
			expectedReply: &pb.RegisterMediaProxyReply{
				ProxyId: "3456789012",
			},
			expectedEventParams: map[string]interface{}{
				"sdk_api_port":               uint32(5678),
				"rdma.dataplane_ip_addr":     "192.168.26.10",
				"rdma.dataplane_local_ports": "8100,8105,8350-8900",
			},
		},
	}

	eventHandler := &MockEventhandler{}

	err := event.EventProcessor.Init(event.EventProcessorConfig{EventHandler: eventHandler})
	require.NoError(t, err)

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	var wg sync.WaitGroup

	wg.Add(1)
	go func() {
		defer wg.Done()
		event.EventProcessor.Run(ctx)
	}()

	for _, c := range cases {
		api := API{}

		params := map[string]interface{}{}

		eventHandler.fn = func(ctx context.Context, e event.Event) event.Reply {
			for k, v := range c.expectedEventParams {
				var err error
				switch v.(type) {
				case string:
					v, err = e.Params.GetString(k)
					if err != nil {
						return event.Reply{Ctx: ctx, Err: err}
					}
				case uint32:
					v, err = e.Params.GetUint32(k)
					if err != nil {
						return event.Reply{Ctx: ctx, Err: err}
					}
				default:
					return event.Reply{Ctx: ctx, Err: fmt.Errorf("Unsupported param type: %T, %v = %v", v, k, v)}
				}
				params[k] = v
			}

			return event.Reply{
				Ctx: context.WithValue(ctx, event.ParamName("proxy_id"), c.expectedReply.ProxyId),
			}
		}

		reply, err := api.RegisterMediaProxy(context.Background(), c.req)
		require.NoError(t, err)
		require.Equal(t, c.expectedReply, reply)
		require.Equal(t, c.expectedEventParams, params)
	}

	cancel()
	wg.Wait()
}

func TestProxyAPI_UnregisterMediaProxy(t *testing.T) {
	cases := []struct {
		req                 *pb.UnregisterMediaProxyRequest
		expectedReply       *pb.UnregisterMediaProxyReply
		expectedEventParams map[string]interface{}
	}{
		{
			req: &pb.UnregisterMediaProxyRequest{
				ProxyId: "0123456789",
			},
			expectedReply: &pb.UnregisterMediaProxyReply{},
			expectedEventParams: map[string]interface{}{
				"proxy_id": "0123456789",
			},
		},
		{
			req:           &pb.UnregisterMediaProxyRequest{},
			expectedReply: &pb.UnregisterMediaProxyReply{},
			expectedEventParams: map[string]interface{}{
				"proxy_id": "",
			},
		},
	}

	eventHandler := &MockEventhandler{}

	err := event.EventProcessor.Init(event.EventProcessorConfig{EventHandler: eventHandler})
	require.NoError(t, err)

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	var wg sync.WaitGroup

	wg.Add(1)
	go func() {
		defer wg.Done()
		event.EventProcessor.Run(ctx)
	}()

	for _, c := range cases {
		api := API{}

		params := map[string]interface{}{}

		eventHandler.fn = func(ctx context.Context, e event.Event) event.Reply {
			for k, v := range c.expectedEventParams {
				var err error
				switch v.(type) {
				case string:
					v, err = e.Params.GetString(k)
					if err != nil {
						return event.Reply{Ctx: ctx, Err: err}
					}
				default:
					return event.Reply{Ctx: ctx, Err: fmt.Errorf("Unsupported param type: %T, %v = %v", v, k, v)}
				}
				params[k] = v
			}

			return event.Reply{Ctx: ctx}
		}

		reply, err := api.UnregisterMediaProxy(context.Background(), c.req)
		require.NoError(t, err)
		require.Equal(t, c.expectedReply, reply)
		require.Equal(t, c.expectedEventParams, params)
	}

	cancel()
	wg.Wait()
}

func TestProxyAPI_RegisterConnection(t *testing.T) {
	cases := []struct {
		req                 *pb.RegisterConnectionRequest
		expectedReply       *pb.RegisterConnectionReply
		expectedEventParams map[string]interface{}
	}{
		{
			req: &pb.RegisterConnectionRequest{
				ProxyId: "123",
				Kind:    "tx",
				Config: &sdk.ConnectionConfig{
					BufParts: &sdk.BufferPartitions{
						Payload:  &sdk.BufferPartition{},
						Metadata: &sdk.BufferPartition{},
						Sysdata:  &sdk.BufferPartition{},
					},
					Conn: &sdk.ConnectionConfig_MultipointGroup{
						MultipointGroup: &sdk.ConfigMultipointGroup{
							Urn: "abc",
						},
					},
					Payload: &sdk.ConnectionConfig_Video{
						Video: &sdk.ConfigVideo{},
					},
				},
			},
			expectedReply: &pb.RegisterConnectionReply{},
			expectedEventParams: map[string]interface{}{
				"proxy_id":  "123",
				"kind":      "tx",
				"conn_type": "group",
				"group_id":  "abc",
			},
		},
		{
			req: &pb.RegisterConnectionRequest{
				ProxyId: "234",
				Kind:    "rx",
				Config: &sdk.ConnectionConfig{
					BufParts: &sdk.BufferPartitions{
						Payload:  &sdk.BufferPartition{},
						Metadata: &sdk.BufferPartition{},
						Sysdata:  &sdk.BufferPartition{},
					},
					Conn: &sdk.ConnectionConfig_MultipointGroup{
						MultipointGroup: &sdk.ConfigMultipointGroup{
							Urn: "ABC",
						},
					},
					Payload: &sdk.ConnectionConfig_Audio{
						Audio: &sdk.ConfigAudio{},
					},
				},
			},
			expectedReply: &pb.RegisterConnectionReply{},
			expectedEventParams: map[string]interface{}{
				"proxy_id":  "234",
				"kind":      "rx",
				"conn_type": "group",
				"group_id":  "ABC",
			},
		},
		{
			req: &pb.RegisterConnectionRequest{
				ProxyId: "345",
				Kind:    "tx",
				Config: &sdk.ConnectionConfig{
					BufParts: &sdk.BufferPartitions{
						Payload:  &sdk.BufferPartition{},
						Metadata: &sdk.BufferPartition{},
						Sysdata:  &sdk.BufferPartition{},
					},
					Conn: &sdk.ConnectionConfig_St2110{
						St2110: &sdk.ConfigST2110{
							IpAddr: "192.168.96.10",
							Port:   8100,
						},
					},
					Payload: &sdk.ConnectionConfig_Video{
						Video: &sdk.ConfigVideo{},
					},
				},
			},
			expectedReply: &pb.RegisterConnectionReply{},
			expectedEventParams: map[string]interface{}{
				"proxy_id":  "345",
				"kind":      "tx",
				"conn_type": "st2110",
				"group_id":  "192.168.96.10:8100",
			},
		},
		{
			req: &pb.RegisterConnectionRequest{
				ProxyId: "456",
				Kind:    "rx",
				Config: &sdk.ConnectionConfig{
					BufParts: &sdk.BufferPartitions{
						Payload:  &sdk.BufferPartition{},
						Metadata: &sdk.BufferPartition{},
						Sysdata:  &sdk.BufferPartition{},
					},
					Conn: &sdk.ConnectionConfig_St2110{
						St2110: &sdk.ConfigST2110{
							IpAddr: "192.168.97.10",
							Port:   8201,
						},
					},
					Payload: &sdk.ConnectionConfig_Audio{
						Audio: &sdk.ConfigAudio{},
					},
				},
			},
			expectedReply: &pb.RegisterConnectionReply{},
			expectedEventParams: map[string]interface{}{
				"proxy_id":  "456",
				"kind":      "rx",
				"conn_type": "st2110",
				"group_id":  "192.168.97.10:8201",
			},
		},
	}

	eventHandler := &MockEventhandler{}

	err := event.EventProcessor.Init(event.EventProcessorConfig{EventHandler: eventHandler})
	require.NoError(t, err)

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	var wg sync.WaitGroup

	wg.Add(1)
	go func() {
		defer wg.Done()
		event.EventProcessor.Run(ctx)
	}()

	for _, c := range cases {
		api := API{}

		params := map[string]interface{}{}

		eventHandler.fn = func(ctx context.Context, e event.Event) event.Reply {
			if e.Type != event.OnRegisterConnection {
				return event.Reply{Ctx: ctx}
			}

			for k, v := range c.expectedEventParams {
				var err error
				switch v.(type) {
				case string:
					v, err = e.Params.GetString(k)
					if err != nil {
						return event.Reply{Ctx: ctx, Err: err}
					}
				case uint32:
					v, err = e.Params.GetUint32(k)
					if err != nil {
						return event.Reply{Ctx: ctx, Err: err}
					}
				case *model.SDKConnectionConfig:
					v, err = e.Params.GetSDKConnConfig(k)
					if err != nil {
						return event.Reply{Ctx: ctx, Err: err}
					}
				default:
					return event.Reply{Ctx: ctx, Err: fmt.Errorf("Unsupported param type: %T, %v = %v", v, k, v)}
				}
				params[k] = v
			}

			cctx := context.WithValue(ctx, event.ParamName("conn_id"), c.expectedReply.ConnId)
			// cctx = context.WithValue(cctx, event.ParamName("group_id"), "n/a")
			return event.Reply{Ctx: cctx}
		}

		reply, err := api.RegisterConnection(context.Background(), c.req)
		require.NoError(t, err)
		require.Equal(t, c.expectedReply, reply)
		require.Equal(t, c.expectedEventParams, params)
	}

	cancel()
	wg.Wait()
}

func TestProxyAPI_UnregisterConnection(t *testing.T) {
	cases := []struct {
		req                 *pb.UnregisterConnectionRequest
		expectedReply       *pb.UnregisterConnectionReply
		expectedEventParams map[string]interface{}
	}{
		{
			req: &pb.UnregisterConnectionRequest{
				ProxyId: "123",
			},
			expectedReply: &pb.UnregisterConnectionReply{},
			expectedEventParams: map[string]interface{}{
				"proxy_id": "123",
			},
		},
		{
			req:           &pb.UnregisterConnectionRequest{},
			expectedReply: &pb.UnregisterConnectionReply{},
			expectedEventParams: map[string]interface{}{
				"proxy_id": "",
			},
		},
	}

	registry.ConnRegistry.Init()

	eventHandler := &MockEventhandler{}

	err := event.EventProcessor.Init(event.EventProcessorConfig{EventHandler: eventHandler})
	require.NoError(t, err)

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	var wg sync.WaitGroup

	wg.Add(1)
	go func() {
		defer wg.Done()
		err := registry.ConnRegistry.Run(ctx)
		require.NoError(t, err)
	}()

	wg.Add(1)
	go func() {
		defer wg.Done()
		event.EventProcessor.Run(ctx)
	}()

	for _, c := range cases {
		conn := model.Connection{
			Config: &model.ConnectionConfig{},
			Status: &model.ConnectionStatus{},
		}

		id, err := registry.ConnRegistry.Add(ctx, conn)
		require.NoError(t, err)

		c.expectedEventParams["conn_id"] = id

		api := API{}

		params := map[string]interface{}{}

		eventHandler.fn = func(ctx context.Context, e event.Event) event.Reply {
			for k, v := range c.expectedEventParams {
				var err error
				switch v.(type) {
				case string:
					v, err = e.Params.GetString(k)
					if err != nil {
						return event.Reply{Ctx: ctx, Err: err}
					}
				default:
					return event.Reply{Ctx: ctx, Err: fmt.Errorf("Unsupported param type: %T, %v = %v", v, k, v)}
				}
				params[k] = v
			}

			return event.Reply{Ctx: ctx}
		}

		c.req.ConnId = id
		reply, err := api.UnregisterConnection(context.Background(), c.req)
		require.NoError(t, err)
		require.Equal(t, c.expectedReply, reply)
		require.Equal(t, c.expectedEventParams, params)
	}

	cancel()
	wg.Wait()
}

func TestProxyAPI_SendMetrics(t *testing.T) {
	cases := []struct {
		req      *pb.SendMetricsRequest
		expected map[string]telemetry.Metric
	}{
		{
			req: &pb.SendMetricsRequest{
				Metrics: []*pb.Metric{
					{
						TimestampMs: 1234567890,
						ProviderId:  "abc",
						Fields: []*pb.MetricField{
							{
								Name:  "state",
								Value: &pb.MetricField_StrValue{StrValue: "initial"},
							},
							{
								Name:  "bandwidth",
								Value: &pb.MetricField_UintValue{UintValue: 987654321},
							},
							{
								Name:  "fps",
								Value: &pb.MetricField_DoubleValue{DoubleValue: 59.94},
							},
							{
								Name:  "linked",
								Value: &pb.MetricField_BoolValue{BoolValue: true},
							},
						},
					},
				},
			},
			expected: map[string]telemetry.Metric{
				"abc": {
					TimestampMs: 1234567890,
					Fields: map[string]interface{}{
						"state":     "initial",
						"bandwidth": uint64(987654321),
						"fps":       59.94,
						"linked":    true,
					},
				},
			},
		},
		{
			req: &pb.SendMetricsRequest{
				Metrics: []*pb.Metric{
					{
						TimestampMs: 1234567890,
						ProviderId:  "abc",
						Fields: []*pb.MetricField{
							{
								Name:  "state",
								Value: &pb.MetricField_StrValue{StrValue: "initial"},
							},
							{
								Name:  "bandwidth",
								Value: &pb.MetricField_UintValue{UintValue: 987654321},
							},
							{
								Name:  "fps",
								Value: &pb.MetricField_DoubleValue{DoubleValue: 59.94},
							},
							{
								Name:  "linked",
								Value: &pb.MetricField_BoolValue{BoolValue: true},
							},
						},
					},
					{
						TimestampMs: 1234567891,
						ProviderId:  "abc",
						Fields: []*pb.MetricField{
							{
								Name:  "state",
								Value: &pb.MetricField_StrValue{StrValue: "closed"},
							},
							{
								Name:  "bandwidth",
								Value: &pb.MetricField_UintValue{UintValue: 3000000},
							},
							{
								Name:  "fps",
								Value: &pb.MetricField_DoubleValue{DoubleValue: 60.0},
							},
							{
								Name:  "linked",
								Value: &pb.MetricField_BoolValue{BoolValue: false},
							},
						},
					},
				},
			},
			expected: map[string]telemetry.Metric{
				"abc": {
					TimestampMs: 1234567891,
					Fields: map[string]interface{}{
						"state":     "closed",
						"bandwidth": uint64(3000000),
						"fps":       60.0,
						"linked":    false,
					},
				},
			},
		},
		{
			req: &pb.SendMetricsRequest{
				Metrics: []*pb.Metric{
					{
						TimestampMs: 1234567890,
						ProviderId:  "abc-1",
						Fields: []*pb.MetricField{
							{
								Name:  "state",
								Value: &pb.MetricField_StrValue{StrValue: "active"},
							},
							{
								Name:  "bandwidth",
								Value: &pb.MetricField_UintValue{UintValue: 1000000},
							},
							{
								Name:  "fps",
								Value: &pb.MetricField_DoubleValue{DoubleValue: 30.0},
							},
						},
					},
					{
						TimestampMs: 1234567891,
						ProviderId:  "abc-2",
						Fields: []*pb.MetricField{
							{
								Name:  "bandwidth",
								Value: &pb.MetricField_UintValue{UintValue: 1122334455},
							},
							{
								Name:  "fps",
								Value: &pb.MetricField_DoubleValue{DoubleValue: 60.0},
							},
							{
								Name:  "linked",
								Value: &pb.MetricField_BoolValue{BoolValue: false},
							},
						},
					},
				},
			},
			expected: map[string]telemetry.Metric{
				"abc-1": {
					TimestampMs: 1234567890,
					Fields: map[string]interface{}{
						"state":     "active",
						"bandwidth": uint64(1000000),
						"fps":       30.0,
					},
				},
				"abc-2": {
					TimestampMs: 1234567891,
					Fields: map[string]interface{}{
						"bandwidth": uint64(1122334455),
						"fps":       60.0,
						"linked":    false,
					},
				},
			},
		},
	}

	registry.MediaProxyRegistry.Init(registry.MediaProxyRegistryConfig{})

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	var wg sync.WaitGroup

	wg.Add(1)
	go func() {
		defer wg.Done()
		err := registry.MediaProxyRegistry.Run(ctx)
		require.NoError(t, err)
	}()

	proxyId, err := registry.MediaProxyRegistry.Add(ctx, model.MediaProxy{})
	require.NoError(t, err)

	for _, v := range cases {
		api := API{}

		v.req.ProxyId = proxyId

		_, err := api.SendMetrics(context.Background(), v.req)
		require.NoError(t, err)

		for providerId, expectedMetric := range v.expected {
			m, ok := telemetry.Storage.GetMetric(providerId)
			require.True(t, ok)
			require.Equal(t, expectedMetric.TimestampMs, m.TimestampMs)
			for fieldName, expectedValue := range expectedMetric.Fields {
				value, ok := m.Fields[fieldName]
				require.True(t, ok)
				require.Equal(t, expectedValue, value)
			}
		}
	}

	cancel()
	wg.Wait()
}
