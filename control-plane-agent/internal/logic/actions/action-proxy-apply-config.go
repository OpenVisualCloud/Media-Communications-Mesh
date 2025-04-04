/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package actions

import (
	"context"
	"fmt"

	"control-plane-agent/internal/event"
	"control-plane-agent/internal/logic/mesh"

	"github.com/sirupsen/logrus"
)

type Action_ProxyApplyConfig struct{}

func (a *Action_ProxyApplyConfig) ValidateModifier(modifier string) error {
	return nil
}

func (a *Action_ProxyApplyConfig) Perform(ctx context.Context, modifier string, param event.Params) (context.Context, bool, error) {
	proxyId, err := param.GetString("proxy_id")
	if err != nil {
		return ctx, false, fmt.Errorf("proxy apply config id err: %w", err)
	}

	err = mesh.ApplyProxyConfigQueue.EnqueueProxyId(ctx, proxyId)
	if err != nil {
		logrus.Errorf("proxy apply config enqueue err: %v", err)
	}

	return ctx, true, nil
}

func init() {
	RegisterAction("proxy-apply-config", &Action_ProxyApplyConfig{})
}
