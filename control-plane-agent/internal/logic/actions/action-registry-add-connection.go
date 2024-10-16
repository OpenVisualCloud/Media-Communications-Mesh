/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package actions

import (
	"context"

	"control-plane-agent/internal/event"
	"control-plane-agent/internal/model"
	"control-plane-agent/internal/registry"
)

type Action_RegistryAddConnection struct{}

func (a *Action_RegistryAddConnection) Perform(ctx context.Context, param event.Params) (context.Context, bool, error) {
	// TODO: add connection parameters

	// sdkPort, err := param.GetUint32("sdk_port")
	// if err != nil {
	// 	logrus.Errorf("registry add proxy sdk port err: %v", err)
	// }

	id, err := registry.ConnRegistry.Add(ctx, model.Connection{})
	if err != nil {
		return ctx, false, err
	}

	return context.WithValue(ctx, event.ParamName("conn_id"), id), true, nil
}

func init() {
	RegisterAction("registry-add-connection", &Action_RegistryAddConnection{})
}
