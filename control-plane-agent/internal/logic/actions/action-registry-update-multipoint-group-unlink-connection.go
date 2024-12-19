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

type Action_RegistryUpdateMultipointGroup_UnlinkConnection struct{}

func (a *Action_RegistryUpdateMultipointGroup_UnlinkConnection) ValidateModifier(modifier string) error {
	return nil
}

func (a *Action_RegistryUpdateMultipointGroup_UnlinkConnection) Perform(ctx context.Context, modifier string, param event.Params) (context.Context, bool, error) {
	groupId, err := param.GetString("group_id")
	if err != nil {
		logrus.Errorf("multipoint group unlink conn group_id err: %v", err)
	}
	connId, err := param.GetString("conn_id")
	if err != nil {
		logrus.Errorf("multipoint group unlink conn id err: %v", err)
	}

	// Unlink Multipoint Group from Local Connection.
	// Error is not handled because the connection can be already deleted from the registry.
	_ = registry.ConnRegistry.Update_UnlinkGroup(ctx, connId)

	// Keep Multipoint Group registry locked while updating the group and unlinking the connection
	registry.MultipointGroupRegistry.Mx.Lock()
	defer registry.MultipointGroupRegistry.Mx.Unlock()

	// Unlink Local Connection from Multipoint Group
	err = registry.MultipointGroupRegistry.Update_UnlinkConn(ctx, groupId, connId)
	if err != nil {
		return ctx, false, err
	}
	return ctx, true, nil
}

func init() {
	RegisterAction("registry-update-multipoint-group-unlink-connection", &Action_RegistryUpdateMultipointGroup_UnlinkConnection{})
}
