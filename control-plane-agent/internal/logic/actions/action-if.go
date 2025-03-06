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
	"control-plane-agent/internal/model"
	"control-plane-agent/internal/registry"

	"github.com/expr-lang/expr"
)

type Action_If struct{}

func (a *Action_If) ValidateModifier(modifier string) error {
	_, err := expr.Compile(modifier, expr.Env(map[string]interface{}{
		"conn_kind":        "",
		"conn_type":        "",
		"conn_config":      (*model.SDKConnectionConfig)(nil),
		"group_id":         "",
		"group_exists":     func(id string) bool { return true },
		"group_compatible": func(id string, kind string, cfg *model.SDKConnectionConfig) bool { return true },
	}))
	return err
}

func (a *Action_If) Perform(ctx context.Context, modifier string, param event.Params) (context.Context, bool, error) {
	env := map[string]interface{}{}
	//---------------------------------------------------------------------------------------------
	connKind, _ := param.GetString("kind")
	env["conn_kind"] = connKind
	//---------------------------------------------------------------------------------------------
	connType, _ := param.GetString("conn_type")
	env["conn_type"] = connType
	//---------------------------------------------------------------------------------------------
	sdkConnConfig, _ := param.GetSDKConnConfig("conn_config")
	env["conn_config"] = sdkConnConfig
	//---------------------------------------------------------------------------------------------
	groupId, _ := param.GetString("group_id")
	env["group_id"] = groupId
	//---------------------------------------------------------------------------------------------
	env["group_exists"] = func(id string) bool {
		if len(id) > 0 {
			// Keep Multipoint Group registry locked while updating or adding the group and adding the connection
			registry.MultipointGroupRegistry.Mx.Lock()
			defer registry.MultipointGroupRegistry.Mx.Unlock()
			_, err := registry.MultipointGroupRegistry.Get(ctx, id, false)
			if err == nil {
				// ctx = context.WithValue(ctx, event.ParamName("group_id_xxx"), group.Id) // TODO: check if this operation can be avoided
				return true
			}
		}
		return false
	}
	//---------------------------------------------------------------------------------------------
	env["group_compatible"] = func(id string, kind string, cfg *model.SDKConnectionConfig) bool {
		if len(id) > 0 {
			// Keep Multipoint Group registry locked while updating or adding the group and adding the connection
			registry.MultipointGroupRegistry.Mx.Lock()
			defer registry.MultipointGroupRegistry.Mx.Unlock()
			group, err := registry.MultipointGroupRegistry.Get(ctx, id, true)
			if err == nil {
				err = mesh.CheckIfGroupAcceptsConnectionKind(ctx, &group, kind)
				if err == nil {
					err = group.Config.CheckPayloadCompatibility(cfg)
					if err == nil {
						return true
					}
				}
				// logrus.Errorf("Conn incompatible with the multipoint group: %v", err)
				ctx = context.WithValue(ctx, event.ParamName("err_incompatible"), err)
			}
		}
		return false
	}
	//---------------------------------------------------------------------------------------------

	output, err := expr.Eval(modifier, env)
	if err != nil {
		return ctx, false, err
	}
	res, ok := output.(bool)
	if !ok {
		return ctx, false, nil
	}
	return ctx, res, nil
}

func init() {
	RegisterAction("if", &Action_If{})
}
