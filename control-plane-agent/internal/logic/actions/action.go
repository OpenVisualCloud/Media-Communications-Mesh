/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package actions

import (
	"context"

	"control-plane-agent/internal/event"
)

type Action interface {
	Perform(ctx context.Context, param event.Params) (context.Context, bool, error)
}

var Registry map[string]Action

func RegisterAction(name string, action Action) {
	if Registry == nil {
		Registry = make(map[string]Action)
	}
	Registry[name] = action
}
