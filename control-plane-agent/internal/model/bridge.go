/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package model

import (
	"errors"
	"fmt"

	"control-plane-agent/api/proxy/proto/sdk"
)

type Bridge struct {
	Id      string            `json:"id,omitempty"`
	ProxyId string            `json:"proxyId,omitempty"`
	GroupId string            `json:"groupId,omitempty"`
	Config  *BridgeConfig     `json:"config,omitempty"`
	Status  *ConnectionStatus `json:"status,omitempty"`
}

type BridgeST2110Config struct {
	RemoteIP  string              `json:"remoteIp"`
	Port      uint16              `json:"port"`
	Transport sdk.ST2110Transport `json:"-"`
}

type BridgeRDMAConfig struct {
	RemoteIP string `json:"remoteIp"`
	Port     uint16 `json:"port"`
}

type BridgeConfig struct {
	SDKConnectionConfig

	Type   string              `json:"type"`
	Kind   string              `json:"kind"`
	ST2110 *BridgeST2110Config `json:"st2110,omitempty"`
	RDMA   *BridgeRDMAConfig   `json:"rdma,omitempty"`

	// Payload Payload `json:"payload"`
}

func (b *Bridge) ValidateConfig() error {
	if b.Config == nil {
		return errors.New("bridge config is nil")
	}
	switch b.Config.Kind {
	case "tx", "rx":
	default:
		return fmt.Errorf("bad bridge kind: '%s'", b.Config.Kind)
	}
	switch b.Config.Type {
	case "st2110":
		if b.Config.ST2110 == nil {
			return errors.New("st2110 bridge config is nil")
		}
		if b.Config.ST2110.RemoteIP == "" {
			return errors.New("bad st2110 bridge remote ip")
		}
		if b.Config.ST2110.Port == 0 {
			return errors.New("bad st2110 bridge port")
		}
		switch b.Config.ST2110.Transport {
		case sdk.ST2110Transport_CONN_TRANSPORT_ST2110_20:
		case sdk.ST2110Transport_CONN_TRANSPORT_ST2110_22:
		case sdk.ST2110Transport_CONN_TRANSPORT_ST2110_30:
		default:
			return fmt.Errorf("bad st2110 bridge transport: '%s'", b.Config.ST2110.Transport)
		}
	case "rdma":
		if b.Config.RDMA == nil {
			return errors.New("rdma bridge config is nil")
		}
		if b.Config.RDMA.RemoteIP == "" {
			return errors.New("bad rdma bridge remote ip")
		}
		if b.Config.RDMA.Port == 0 {
			return errors.New("bad rdma bridge port")
		}
	default:
		return fmt.Errorf("bad bridge type: '%s'", b.Config.Type)
	}

	return nil
}
