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

	"control-plane-agent/internal/event"
	"control-plane-agent/internal/registry"
)

type Action_RegistryUpdateMultipointGroupLinkBridge struct{}

func (a *Action_RegistryUpdateMultipointGroupLinkBridge) ValidateModifier(modifier string) error {
	return nil
}

func (a *Action_RegistryUpdateMultipointGroupLinkBridge) Perform(ctx context.Context, modifier string, param event.Params) (context.Context, bool, error) {
	groupId, err := param.GetString("group_id")
	if err != nil {
		return ctx, false, fmt.Errorf("multipoint group link bridge group id err: %v", err)
	}
	bridgeId, ok := ctx.Value(event.ParamName("bridge_id")).(string)
	if !ok {
		return ctx, false, errors.New("multipoint group link bridge: no bridge id in ctx")
	}

	// Link Multipoint Group to Bridge
	err = registry.BridgeRegistry.Update_LinkGroup(ctx, bridgeId, groupId)
	if err != nil {
		return ctx, false, fmt.Errorf("multipoint group link bridge update bridge err: %w", err)
	}

	// Link Bridge to Multipoint Group
	err = registry.MultipointGroupRegistry.Update_LinkBridge(ctx, groupId, bridgeId)
	if err != nil {
		return ctx, false, err
	}
	return ctx, true, nil
}

func init() {
	RegisterAction("registry-update-multipoint-group-link-bridge", &Action_RegistryUpdateMultipointGroupLinkBridge{})
}
