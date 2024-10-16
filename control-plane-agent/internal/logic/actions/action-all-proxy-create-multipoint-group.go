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

type Action_AllProxyCreateMultipointGroup struct{}

func (a *Action_AllProxyCreateMultipointGroup) Perform(ctx context.Context, param event.Params) (context.Context, bool, error) {
	return ctx, true, nil
}

func init() {
	RegisterAction("all-proxy-create-multipoint-group", &Action_AllProxyCreateMultipointGroup{})
}
