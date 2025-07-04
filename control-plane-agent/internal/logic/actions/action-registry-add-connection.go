/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package actions

import (
	"context"
	"time"

	"control-plane-agent/internal/event"
	"control-plane-agent/internal/model"
	"control-plane-agent/internal/registry"

	"github.com/sirupsen/logrus"
)

type Action_RegistryAddConnection struct{}

func (a *Action_RegistryAddConnection) ValidateModifier(modifier string) error {
	return nil
}

func (a *Action_RegistryAddConnection) Perform(ctx context.Context, modifier string, param event.Params) (context.Context, bool, error) {
	proxyId, err := param.GetString("proxy_id")
	if err != nil {
		logrus.Errorf("registry add conn proxy id err: %v", err)
	}
	proxyName, _ := param.GetString("proxy_name")
	kind, err := param.GetString("kind")
	if err != nil {
		logrus.Errorf("registry add conn kind err: %v", err)
	}
	sdkConnConfig, err := param.GetSDKConnConfig("conn_config")
	if err != nil {
		logrus.Errorf("registry add conn sdk cfg err: %v", err)
	}
	connId, err := param.GetString("conn_id")
	if err != nil {
		logrus.Errorf("registry add conn conn id err: %v", err)
	}
	connName, _ := param.GetString("conn_name")

	// _, _, err = mesh.ParseGroupURN(groupURN)
	// if err != nil {
	// 	return ctx, false, fmt.Errorf("group urn parse err: %v", err)
	// }

	// groupId := "224.0.0.1:9003"

	config := &model.ConnectionConfig{
		Kind: kind,
	}

	config.CopyFrom(sdkConnConfig)

	id, err := registry.ConnRegistry.Add(ctx,
		model.Connection{
			Id:        connId, // Normally, this should be empty. If Media Proxy lost connection to Agent, this is the id assigned by Agent at previous registration.
			Name:      connName,
			ProxyId:   proxyId,
			ProxyName: proxyName,
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

	// Add connection to Media Proxy
	err = registry.MediaProxyRegistry.Update_LinkConn(ctx, proxyId, id)
	if err != nil {
		return ctx, false, err
	}

	ctx = context.WithValue(ctx, event.ParamName("conn_id"), id)
	return ctx, true, nil
}

func init() {
	RegisterAction("registry-add-connection", &Action_RegistryAddConnection{})
}
