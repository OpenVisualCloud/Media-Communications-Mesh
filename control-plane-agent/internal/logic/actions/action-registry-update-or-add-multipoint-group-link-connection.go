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
	"control-plane-agent/internal/model"
	"control-plane-agent/internal/registry"
)

type Action_RegistryUpdateOrAddMultipointGroup_LinkConnection struct{}

func (a *Action_RegistryUpdateOrAddMultipointGroup_LinkConnection) ValidateModifier(modifier string) error {
	return nil
}

func (a *Action_RegistryUpdateOrAddMultipointGroup_LinkConnection) Perform(ctx context.Context, modifier string, param event.Params) (context.Context, bool, error) {
	groupId, ok := ctx.Value(event.ParamName("group_id")).(string)
	if !ok {
		return ctx, false, errors.New("multipoint group link conn: no group id in ctx")
	}
	connId, ok := ctx.Value(event.ParamName("conn_id")).(string)
	if !ok {
		return ctx, false, errors.New("multipoint group link conn: no conn id in ctx")
	}

	// Link Multipoint Group to Local Connection
	err := registry.ConnRegistry.Update_LinkGroup(ctx, connId, groupId)
	if err != nil {
		return ctx, false, fmt.Errorf("multipoint group link conn update conn err: %w", err)
	}

	// Keep Multipoint Group registry locked while updating or adding the group and linking the connection
	registry.MultipointGroupRegistry.Mx.Lock()
	defer registry.MultipointGroupRegistry.Mx.Unlock()

	// Link Local Connection to Multipoint Group
	err = registry.MultipointGroupRegistry.Update_LinkConn(ctx, groupId, connId)
	if errors.Is(err, registry.ErrResourceNotFound) {

		_, err = registry.MultipointGroupRegistry.Add(ctx, model.MultipointGroup{
			Id:      groupId,
			Status:  &model.ConnectionStatus{},
			Config:  &model.MultipointGroupConfig{},
			ConnIds: []string{connId},
		})
	}
	if err != nil {
		return ctx, false, err
	}
	return ctx, true, nil
}

func init() {
	RegisterAction("registry-update-or-add-multipoint-group-link-connection", &Action_RegistryUpdateOrAddMultipointGroup_LinkConnection{})
}
