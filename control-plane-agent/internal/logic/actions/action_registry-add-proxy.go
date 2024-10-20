/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package actions

import (
	"context"

	"github.com/sirupsen/logrus"

	"control-plane-agent/internal/event"
	"control-plane-agent/internal/model"
	"control-plane-agent/internal/registry"
)

type Action_RegistryAddProxy struct{}

func (a *Action_RegistryAddProxy) Perform(ctx context.Context, param event.Params) (context.Context, bool, error) {
	sdkPort, err := param.GetUint32("sdk_port")
	if err != nil {
		logrus.Errorf("registry add proxy sdk port err: %v", err)
	}

	id, err := registry.MediaProxyRegistry.Add(ctx, model.MediaProxy{Config: &model.MediaProxyConfig{SDKPort: sdkPort}})
	if err != nil {
		return ctx, false, err
	}

	return context.WithValue(ctx, event.ParamName("proxy_id"), id), true, nil
}

func init() {
	RegisterAction("registry-add-proxy", &Action_RegistryAddProxy{})
}
