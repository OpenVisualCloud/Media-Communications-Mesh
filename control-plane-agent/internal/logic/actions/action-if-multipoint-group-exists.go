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

type Action_IfMultipointGroupExists struct{}

func (a *Action_IfMultipointGroupExists) Perform(ctx context.Context, param event.Params) (context.Context, bool, error) {
	return ctx, false, nil
}

func init() {
	RegisterAction("if-multipoint-group-exists", &Action_IfMultipointGroupExists{})
}
