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

type Action_RegistryDeleteMultipointGroupIfEmpty struct{}

func (a *Action_RegistryDeleteMultipointGroupIfEmpty) ValidateModifier(modifier string) error {
	return nil
}

func (a *Action_RegistryDeleteMultipointGroupIfEmpty) Perform(ctx context.Context, modifier string, param event.Params) (context.Context, bool, error) {
	groupId, err := param.GetString("group_id")
	if err != nil {
		logrus.Errorf("delete multipoint group if empty group_id err: %v", err)
	}

	// Keep Multipoint Group registry locked while updating the group
	registry.MultipointGroupRegistry.Mx.Lock()
	defer registry.MultipointGroupRegistry.Mx.Unlock()

	group, err := registry.MultipointGroupRegistry.Get(ctx, groupId, false)
	if err != nil {
		return ctx, false, err
	}
	if len(group.ConnIds) == 0 {
		for _, id := range group.BridgeIds {
			bridge, err := registry.BridgeRegistry.Get(ctx, id, false)
			if err != nil {
				logrus.Errorf("delete multipoint group if empty: bridge get err: %v, id: '%v'", err, id)
				continue
			}

			_ = registry.BridgeRegistry.Delete(ctx, id)

			// Remove bridge from Media Proxy
			err = registry.MediaProxyRegistry.Update_UnlinkBridge(ctx, bridge.ProxyId, id)
			if err != nil {
				logrus.Errorf("delete multipoint group if empty: proxy update unlink bridge err: %v, id: '%v'", err, id)
			}
		}

		err = registry.MultipointGroupRegistry.Delete(ctx, groupId)
		if err != nil {
			return ctx, false, err
		}
	}
	return ctx, true, nil
}

func init() {
	RegisterAction("registry-delete-multipoint-group-if-empty", &Action_RegistryDeleteMultipointGroupIfEmpty{})
}
