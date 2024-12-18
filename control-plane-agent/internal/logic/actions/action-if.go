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

	"github.com/expr-lang/expr"
)

type Action_If struct{}

func (a *Action_If) ValidateModifier(modifier string) error {
	_, err := expr.Compile(modifier, expr.Env(map[string]interface{}{
		"conn_type":    "",
		"group_id":     "",
		"group_exists": func(id string) bool { return true },
	}))
	return err
}

func (a *Action_If) Perform(ctx context.Context, modifier string, param event.Params) (context.Context, bool, error) {
	env := map[string]interface{}{}
	//---------------------------------------------------------------------------------------------
	connType, err := param.GetString("conn_type")
	if err != nil {
		connType, _ = ctx.Value(event.ParamName("conn_type")).(string)
	}
	env["conn_type"] = connType
	//---------------------------------------------------------------------------------------------
	groupId, _ := ctx.Value(event.ParamName("group_id")).(string)
	env["group_id"] = groupId
	//---------------------------------------------------------------------------------------------
	env["group_exists"] = func(id string) bool {
		if len(id) > 0 {
			// Keep Multipoint Group registry locked while updating or adding the group and adding the connection
			registry.MultipointGroupRegistry.Mx.Lock()
			defer registry.MultipointGroupRegistry.Mx.Unlock()
			group, err := registry.MultipointGroupRegistry.Get(ctx, id, false)
			if err == nil {
				ctx = context.WithValue(ctx, event.ParamName("group_id"), group.Id)
				return true
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
