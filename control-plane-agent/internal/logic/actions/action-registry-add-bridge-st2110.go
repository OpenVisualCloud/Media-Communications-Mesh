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
	"control-plane-agent/internal/logic/mesh"
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
		return ctx, false, fmt.Errorf("registry add bridge st2110 type err: %w", err)
	}
	if connType != "st2110" {
		return ctx, false, fmt.Errorf("registry add bridge st2110 wrong type: '%v'", connType)
	}

	// TODO: add connection parameters

	proxyId, err := param.GetString("proxy_id")
	if err != nil {
		logrus.Errorf("registry add bridge st2110 proxy_id err: %v", err)
	}
	connKind, err := param.GetString("kind")
	if err != nil {
		logrus.Errorf("registry add bridge st2110 kind err: %v", err)
	}
	groupId, ok := ctx.Value(event.ParamName("group_id")).(string)
	if !ok {
		return ctx, false, errors.New("registry add bridge st2110: no group id in ctx")
	}

	var bridgeKind string
	if connKind == "tx" {
		bridgeKind = "rx"
	} else {
		bridgeKind = "tx"
	}

	groupUrnIp, groupUrnPort, err := mesh.ParseGroupURN(groupId)
	if err != nil {
		return ctx, false, fmt.Errorf("group urn bad format: %v", err)
	}

	id, err := registry.BridgeRegistry.Add(ctx,
		model.Bridge{
			ProxyId: proxyId,
			GroupId: groupId,
			Config: &model.BridgeConfig{
				Kind: bridgeKind,
				Type: connType,
				ST2110: &model.BridgeST2110Config{
					RemoteIP:  groupUrnIp,
					Port:      groupUrnPort,
					Transport: "20",
				},
				Payload: model.Payload{
					Video: &model.PayloadVideo{
						Width:       640,
						Height:      480,
						FPS:         30,
						PixelFormat: "yuv422p10le",
					},
				},
			},
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
