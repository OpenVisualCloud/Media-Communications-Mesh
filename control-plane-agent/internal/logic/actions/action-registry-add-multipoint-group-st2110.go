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

type Action_RegistryAddMultipointGroupST2110 struct{}

func (a *Action_RegistryAddMultipointGroupST2110) ValidateModifier(modifier string) error {
	return nil
}

func (a *Action_RegistryAddMultipointGroupST2110) Perform(ctx context.Context, modifier string, param event.Params) (context.Context, bool, error) {
	connType, err := param.GetString("conn_type")
	if err != nil {
		return ctx, false, fmt.Errorf("registry add multipoint group st2110 conn type err: %v", err)
	}
	if connType != "st2110" {
		return ctx, false, fmt.Errorf("registry add multipoint group st2110 wrong type: '%v'", connType)
	}

	groupId, err := param.GetString("group_id")
	if err != nil {
		return ctx, false, fmt.Errorf("registry add multipoint group st2110 group id err: %v", err)
	}
	sdkConnConfig, err := param.GetSDKConnConfig("conn_config")
	if err != nil {
		return ctx, false, err
	}

	// Keep Multipoint Group registry locked while updating or adding the group and adding the connection
	registry.MultipointGroupRegistry.Mx.Lock()
	defer registry.MultipointGroupRegistry.Mx.Unlock()

	// Add connection to Media Proxy
	_, err = registry.MultipointGroupRegistry.Get(ctx, groupId, false)
	if err == nil {
		return ctx, false, errors.New("registry add multipoint group st2110: group exists")
	}
	if errors.Is(err, registry.ErrResourceNotFound) {

		config := &model.MultipointGroupConfig{}
		config.CopyFrom(sdkConnConfig)

		_, err := registry.MultipointGroupRegistry.Add(ctx, model.MultipointGroup{
			Id:     groupId,
			Status: &model.ConnectionStatus{},
			Config: config,
		})
		if err != nil {
			return ctx, false, fmt.Errorf("registry add multipoint group st2110 err: %w", err)
		}

		// ctx = context.WithValue(ctx, event.ParamName("group_id_xxx"), groupId) // TODO: check if this operation can be avoided
		return ctx, true, nil
	}
	return ctx, false, err
}

func init() {
	RegisterAction("registry-add-multipoint-group-st2110", &Action_RegistryAddMultipointGroupST2110{})
}
