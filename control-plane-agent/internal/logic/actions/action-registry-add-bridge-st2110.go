/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package actions

import (
	"context"
	"errors"
	"fmt"
	"time"

	"control-plane-agent/internal/event"
	"control-plane-agent/internal/model"
	"control-plane-agent/internal/registry"

	"github.com/sirupsen/logrus"
)

type Action_RegistryAddBridgeST2110 struct{}

func (a *Action_RegistryAddBridgeST2110) ValidateModifier(modifier string) error {
	return nil
}

func (a *Action_RegistryAddBridgeST2110) Perform(ctx context.Context, modifier string, param event.Params) (context.Context, bool, error) {
	connType, err := param.GetString("conn_type")
	if err != nil {
		return ctx, false, fmt.Errorf("registry add bridge st2110 conn type err: %v", err)
	}
	if connType != "st2110" {
		return ctx, false, fmt.Errorf("registry add bridge st2110 wrong type: '%v'", connType)
	}

	sdkConnConfig, err := param.GetSDKConnConfig("conn_config")
	if err != nil {
		logrus.Errorf("registry add bridge st2110 sdk cfg err: %v", err)
	}
	if sdkConnConfig.Conn.ST2110 == nil {
		return ctx, false, errors.New("registry add bridge st2110: sdk cfg is nil")
	}

	proxyId, err := param.GetString("proxy_id")
	if err != nil {
		logrus.Errorf("registry add bridge st2110 proxy_id err: %v", err)
	}
	proxyName, _ := param.GetString("proxy_name")
	connKind, err := param.GetString("kind")
	if err != nil {
		logrus.Errorf("registry add bridge st2110 kind err: %v", err)
	}
	groupId, err := param.GetString("group_id")
	if err != nil {
		return ctx, false, err
	}

	config := &model.BridgeConfig{
		Type: connType,
		ST2110: &model.BridgeST2110Config{
			IPAddr:       sdkConnConfig.Conn.ST2110.IPAddr,
			Port:         sdkConnConfig.Conn.ST2110.Port,
			McastSipAddr: sdkConnConfig.Conn.ST2110.McastSipAddr,
			Transport:    sdkConnConfig.Conn.ST2110.Transport,
			PayloadType:  sdkConnConfig.Conn.ST2110.PayloadType,
		},
	}

	if connKind == "tx" {
		config.Kind = "rx"
	} else {
		config.Kind = "tx"
	}

	config.CopyFrom(sdkConnConfig)

	id, err := registry.BridgeRegistry.Add(ctx,
		model.Bridge{
			ProxyId:   proxyId,
			ProxyName: proxyName,
			GroupId:   groupId,
			Config:    config,
			Status: &model.ConnectionStatus{
				RegisteredAt: model.CustomTime(time.Now()),
				State:        "active", // TODO: Rework this to use string enum?
			},
		},
	)
	if err != nil {
		return ctx, false, err
	}

	// Add bridge to Media Proxy
	err = registry.MediaProxyRegistry.Update_LinkBridge(ctx, proxyId, id)
	if err != nil {
		return ctx, false, fmt.Errorf("registry add bridge st2110 update proxy err: %w", err)
	}

	ctx = context.WithValue(ctx, event.ParamName("bridge_id"), id)
	return ctx, true, nil
}

func init() {
	RegisterAction("registry-add-bridge-st2110", &Action_RegistryAddBridgeST2110{})
}
