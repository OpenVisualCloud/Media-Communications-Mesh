/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package actions

import (
	"context"

	"control-plane-agent/internal/event"
	"control-plane-agent/internal/registry"

	"github.com/sirupsen/logrus"
)

type Action_RegistryDeleteConnection struct{}

func (a *Action_RegistryDeleteConnection) ValidateModifier(modifier string) error {
	return nil
}

func (a *Action_RegistryDeleteConnection) Perform(ctx context.Context, modifier string, param event.Params) (context.Context, bool, error) {
	proxyId, err := param.GetString("proxy_id")
	if err != nil {
		logrus.Errorf("registry del conn proxy_id err: %v", err)
	}
	connId, err := param.GetString("conn_id")
	if err != nil {
		logrus.Errorf("registry del conn id err: %v", err)
	}

	err = registry.ConnRegistry.Delete(ctx, connId)
	if err != nil {
		return ctx, false, err
	}

	err = registry.MediaProxyRegistry.Update_UnlinkConn(ctx, proxyId, connId)
	if err != nil {
		return ctx, false, err
	}
	return ctx, true, nil
}

func init() {
	RegisterAction("registry-delete-connection", &Action_RegistryDeleteConnection{})
}
