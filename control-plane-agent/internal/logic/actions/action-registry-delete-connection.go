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

type Action_RegistryDeleteConnection struct{}

func (a *Action_RegistryDeleteConnection) Perform(ctx context.Context, param event.Params) (context.Context, bool, error) {
	return ctx, true, nil
}

func init() {
	RegisterAction("registry-delete-connection", &Action_RegistryDeleteConnection{})
}
