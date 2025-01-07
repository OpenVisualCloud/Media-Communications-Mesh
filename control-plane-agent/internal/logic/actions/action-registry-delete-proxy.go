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

type Action_RegistryDeleteProxy struct{}

func (a *Action_RegistryDeleteProxy) ValidateModifier(modifier string) error {
	return nil
}

func (a *Action_RegistryDeleteProxy) Perform(ctx context.Context, modifier string, param event.Params) (context.Context, bool, error) {
	id, err := param.GetString("proxy_id")
	if err != nil {
		logrus.Errorf("registry del proxy id err: %v", err)
	}

	err = registry.MediaProxyRegistry.Delete(ctx, id)
	if err != nil {
		return ctx, false, err
	}

	return ctx, true, nil
}

func init() {
	RegisterAction("registry-delete-proxy", &Action_RegistryDeleteProxy{})
}
