/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package actions

import (
	"context"
	"time"

	"github.com/sirupsen/logrus"

	"control-plane-agent/internal/event"
	"control-plane-agent/internal/model"
	"control-plane-agent/internal/registry"
)

type Action_RegistryAddProxy struct{}

func (a *Action_RegistryAddProxy) ValidateModifier(modifier string) error {
	return nil
}

func (a *Action_RegistryAddProxy) Perform(ctx context.Context, modifier string, param event.Params) (context.Context, bool, error) {
	sdkAPIPort, err := param.GetUint32("sdk_api_port")
	if err != nil {
		logrus.Errorf("registry add proxy sdk port err: %v", err)
	}
	st2110_DevPortBDF, err := param.GetString("st2110.dev_port_bdf")
	if err != nil {
		logrus.Errorf("registry add proxy st2110 dev port err: %v", err)
	}
	st2110_DataplaneIPAddr, err := param.GetString("st2110.dataplane_ip_addr")
	if err != nil {
		logrus.Errorf("registry add proxy st2110 dataplane ip addr err: %v", err)
	}
	rdma_DataplaneIPAddr, err := param.GetString("rdma.dataplane_ip_addr")
	if err != nil {
		logrus.Errorf("registry add proxy rdma dataplane ip addr err: %v", err)
	}
	rdma_DataplaneLocalPorts, err := param.GetString("rdma.dataplane_local_ports")
	if err != nil {
		logrus.Errorf("registry add proxy rdma dataplane local ports err: %v", err)
	}

	rdmaBitMask, err := model.NewPortMask(rdma_DataplaneLocalPorts)
	if err != nil {
		logrus.Errorf("registry add proxy rdma dataplane parse ports err: %v", err)
		return ctx, false, err
	}

	proxy := model.MediaProxy{
		Config: &model.MediaProxyConfig{
			SDKAPIPort: sdkAPIPort,
			ST2110: model.ST2110ProxyConfig{
				DataplaneIPAddr: st2110_DataplaneIPAddr,
				DevPortBDF:      st2110_DevPortBDF,
			},
			RDMA: model.RDMAProxyConfig{
				DataplaneIPAddr:     rdma_DataplaneIPAddr,
				DataplaneLocalPorts: rdma_DataplaneLocalPorts,
			},
		},
		Status: &model.MediaProxyStatus{
			Healthy:      true,
			RegisteredAt: model.CustomTime(time.Now()),
		},
		RDMAPortsAllowed: rdmaBitMask,
	}
	id, err := registry.MediaProxyRegistry.Add(ctx, proxy)
	if err != nil {
		return ctx, false, err
	}

	return context.WithValue(ctx, event.ParamName("proxy_id"), id), true, nil
}

func init() {
	RegisterAction("registry-add-proxy", &Action_RegistryAddProxy{})
}
