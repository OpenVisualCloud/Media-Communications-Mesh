/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package model

type MultipointGroup struct {
	Id        string                 `json:"id,omitempty"`
	Config    *MultipointGroupConfig `json:"config,omitempty"`
	Status    *ConnectionStatus      `json:"status,omitempty"`
	ConnIds   []string               `json:"connIds,omitempty"`
	BridgeIds []string               `json:"bridgeIds,omitempty"`
}

type MultipointGroupConfig struct {
	IPAddr string `json:"ipAddr"`
}
