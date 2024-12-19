/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package actions

import (
	"context"
	"fmt"
	"time"

	"control-plane-agent/internal/event"
	"control-plane-agent/internal/logic/mesh"
	"control-plane-agent/internal/model"
	"control-plane-agent/internal/registry"

	"github.com/sirupsen/logrus"
)

type Action_RegistryAddConnection struct{}

func (a *Action_RegistryAddConnection) ValidateModifier(modifier string) error {
	return nil
}

func (a *Action_RegistryAddConnection) Perform(ctx context.Context, modifier string, param event.Params) (context.Context, bool, error) {
	// TODO: add connection parameters

	proxyId, err := param.GetString("proxy_id")
	if err != nil {
		logrus.Errorf("registry add conn proxy_id err: %v", err)
	}
	kind, err := param.GetString("kind")
	if err != nil {
		logrus.Errorf("registry add conn kind err: %v", err)
	}
	groupURN, err := param.GetString("group_urn")
	if err != nil {
		logrus.Errorf("registry add conn group id err: %v", err)
	}

	_, _, err = mesh.ParseGroupURN(groupURN)
	if err != nil {
		return ctx, false, fmt.Errorf("group urn parse err: %v", err)
	}

	// groupId := "224.0.0.1:9003"

	id, err := registry.ConnRegistry.Add(ctx,
		model.Connection{
			ProxyId: proxyId,
			Config: &model.ConnectionConfig{
				Kind:     kind,
				GroupURN: groupURN,
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

	// Add connection to Media Proxy
	err = registry.MediaProxyRegistry.Update_LinkConn(ctx, proxyId, id)
	if err != nil {
		return ctx, false, err
	}

	ctx = context.WithValue(ctx, event.ParamName("conn_id"), id)
	ctx = context.WithValue(ctx, event.ParamName("group_id"), groupURN) // Here group URN becomes Group ID
	// TODO: pass all payload config here to let the subsequent action decide
	// if the connection payload config is the same as the group payload config.

	return ctx, true, nil
}

func init() {
	RegisterAction("registry-add-connection", &Action_RegistryAddConnection{})
}
