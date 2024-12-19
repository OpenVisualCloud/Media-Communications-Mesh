/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package actions

import (
	"context"

	"control-plane-agent/internal/event"
	"control-plane-agent/internal/logic/mesh"
	"control-plane-agent/internal/registry"

	"github.com/sirupsen/logrus"
)

type Action_AllProxiesApplyConfig struct{}

func (a *Action_AllProxiesApplyConfig) ValidateModifier(modifier string) error {
	return nil
}

func (a *Action_AllProxiesApplyConfig) Perform(ctx context.Context, modifier string, param event.Params) (context.Context, bool, error) {
	proxies, err := registry.MediaProxyRegistry.List(ctx, nil, false, false)
	if err != nil {
		return ctx, false, err
	}

	groups, err := registry.MultipointGroupRegistry.List(ctx, nil, false, false)
	if err != nil {
		logrus.Errorf("all proxies apply cfg list groups err: %v", err)
		return ctx, false, err
	}

	for i := range proxies {
		err = mesh.ApplyProxyConfig(ctx, &proxies[i], groups)
		if err != nil {
			logrus.Errorf("all proxies apply cfg cmd err: %v, proxy id: %v", err, proxies[i].Id)
		}
	}

	return ctx, true, nil
}

// WARNING: This action locks mutexes in Media Proxy. Call it in asynchronous events only to avoid double locking of mutexes.
func init() {
	RegisterAction("all-proxies-apply-config", &Action_AllProxiesApplyConfig{})
}
