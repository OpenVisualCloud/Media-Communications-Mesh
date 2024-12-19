/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package model

type Connection struct {
	Id      string `json:"id,omitempty"`
	ProxyId string `json:"proxyId,omitempty"`
	GroupId string `json:"groupId,omitempty"`

	Config *ConnectionConfig `json:"config,omitempty"`
	Status *ConnectionStatus `json:"status,omitempty"`
}

type ConnectionConfig struct {
	Kind        string      `json:"kind"`
	GroupURN    string      `json:"groupUrn"`
	ConnType    string      `json:"connType"`
	Conn        interface{} `json:"conn"`
	PayloadType string      `json:"-"` // Hide in JSON. The 'payload' field data is sufficient. TODO: Remove the field?
	Payload     Payload     `json:"payload"`
	BufferSize  uint64      `json:"bufferSize"`
}

type ConnectionST2110 struct {
	RemoteIPAddr string `json:"remoteIpAddr"`
	RemotePort   string `json:"remotePort"`
}

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
