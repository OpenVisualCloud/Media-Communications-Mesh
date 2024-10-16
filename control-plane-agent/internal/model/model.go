/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package model

import (
	"context"
	"time"
)

type CommandMessage struct {
	OpCode string
	Id     string
}

type MediaProxy struct {
	Id     string            `json:"id,omitempty"`
	Config *MediaProxyConfig `json:"config,omitempty"`
	Status *MediaProxyStatus `json:"status,omitempty"`

	queue chan CommandMessage
}

func (mp *MediaProxy) Init() {
	mp.queue = make(chan CommandMessage, 100) // TODO: capacity to be configured
}

func (mp *MediaProxy) Deinit() {
	close(mp.queue)
}

func (mp *MediaProxy) SendCommand(ctx context.Context, cmd CommandMessage) {
	select {
	case <-ctx.Done():
	case mp.queue <- cmd:
	}
}

func (mp *MediaProxy) CommandQueue() <-chan CommandMessage {
	return mp.queue
}

type MediaProxyConfig struct {
	SDKPort uint32 `json:"sdkPort"`
}

type MediaProxyStatus struct {
	Healthy       bool      `json:"healthy"`
	StartedAt     time.Time `json:"startedAt"`
	ConnectionNum int       `json:"connectionNum"`
}

type Connection struct {
	Id      string            `json:"id,omitempty"`
	ProxyId string            `json:"proxyId,omitempty"`
	Config  *ConnectionConfig `json:"config,omitempty"`
	Status  *ConnectionStatus `json:"status,omitempty"`
}

type ConnectionConfig struct {
	Kind        string      `json:"kind"`
	ConnType    string      `json:"connType"`
	Conn        interface{} `json:"conn"`
	PayloadType string      `json:"payloadType"`
	Payload     interface{} `json:"payload"`
	BufferSize  uint64      `json:"bufferSize"`
}

type ConnectionST2110 struct {
	RemoteIPAddr string `json:"remoteIpAddr"`
	RemotePort   string `json:"remotePort"`
}

// TBD ConnectionMemif
// TBD ConnectionRDMA

type PayloadVideo struct {
	Width       uint32  `json:"width"`
	Height      uint32  `json:"height"`
	FPS         float32 `json:"fps"`
	PixelFormat string  `json:"pixelFormat"`
}

// TBD PayloadAudio
// TBD PayloadAncillary

type ConnectionStatus struct {
	CreatedAt   time.Time `json:"createdAt"`
	BuffersSent int       `json:"buffersSent"`
}

type MultipointGroup struct {
	Id      string                 `json:"id,omitempty"`
	ProxyId string                 `json:"proxyId,omitempty"`
	Config  *MultipointGroupConfig `json:"config,omitempty"`
	Status  *MultipointGroupStatus `json:"status,omitempty"`
}

type MultipointGroupConfig struct {
	IPAddr string `json:"ipAddr"`
}

type MultipointGroupStatus struct {
	CreatedAt time.Time `json:"createdAt"`
}

type Bridge struct {
	Id      string        `json:"id,omitempty"`
	ProxyId string        `json:"proxyId,omitempty"`
	Config  *BridgeConfig `json:"config,omitempty"`
	Status  *BridgeStatus `json:"status,omitempty"`
}

type BridgeConfig struct {
	IPAddr string `json:"ipAddr"`
}

type BridgeStatus struct {
	CreatedAt time.Time `json:"createdAt"`
}
