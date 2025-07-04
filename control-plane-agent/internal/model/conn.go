/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package model

type Connection struct {
	Id        string `json:"id,omitempty"`
	Name      string `json:"name,omitempty"`
	ProxyId   string `json:"proxyId,omitempty"`
	ProxyName string `json:"proxyName,omitempty"`
	GroupId   string `json:"groupId,omitempty"`

	Config *ConnectionConfig `json:"config,omitempty"`
	Status *ConnectionStatus `json:"status,omitempty"`
}

type ConnectionConfig struct {
	SDKConnectionConfig

	Kind string `json:"kind"`

	// // TODO: The following fields to be deleted
	// GroupURN1    string      `json:"-"`
	// ConnType1    string      `json:"connType_xxx"`
	// Conn1        interface{} `json:"conn_xxx"`
	// PayloadType1 string      `json:"-"` // Hide in JSON. The 'payload' field data is sufficient. TODO: Remove the field?
	// Payload1     Payload     `json:"payload_xxx"`
	// BufferSize1  uint64      `json:"bufferSize_xxx"`
}

// type ConnectionST2110 struct {
// 	RemoteIPAddr string `json:"remoteIpAddr"`
// 	RemotePort   string `json:"remotePort"`
// }

// TBD ConnectionMemif
// TBD ConnectionRDMA

type Payload struct {
	Video *PayloadVideo `json:"video,omitempty"`
	// Audio *PayloadAudio `json:"audio"`
	// Blob  *PayloadBlob  `json:"blob"`
}

type PayloadVideo struct {
	Width       uint32  `json:"width"`
	Height      uint32  `json:"height"`
	FPS         float32 `json:"fps"`
	PixelFormat string  `json:"pixelFormat"`
}

// TBD PayloadAudio
// TBD PayloadAncillary

type ConnectionStatus struct {
	RegisteredAt          CustomTime `json:"registeredAt"`
	UpdatedAt             CustomTime `json:"updatedAt,omitempty"`
	State                 string     `json:"state"`
	Linked                bool       `json:"linked"`
	InboundBytes          uint64     `json:"inbound"`
	OutboundBytes         uint64     `json:"outbound"`
	TransactionsSucceeded uint32     `json:"trnSucceeded"`
	TransactionsFailed    uint32     `json:"trnFailed"`
	TransactionsPerSecond float64    `json:"tps"`
	InboundBandwidth      float64    `json:"inBandwidthMbit"`  // One unit is 1 Mbit/s
	OutboundBandwidth     float64    `json:"outBandwidthMbit"` // One unit is 1 Mbit/s
	Errors                uint32     `json:"errors"`
	ErrorsDelta           uint32     `json:"errorsDelta"`
}
