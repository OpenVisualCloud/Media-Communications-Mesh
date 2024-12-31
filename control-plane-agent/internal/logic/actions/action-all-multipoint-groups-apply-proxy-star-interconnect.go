/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package actions

import (
	"context"
	"time"

	"control-plane-agent/internal/event"
	"control-plane-agent/internal/model"
	"control-plane-agent/internal/registry"

	"github.com/sirupsen/logrus"
)

type Action_AllMultipointGroupsApplyProxyStarInterconnect struct{}

func (a *Action_AllMultipointGroupsApplyProxyStarInterconnect) ValidateModifier(modifier string) error {
	return nil
}

func (a *Action_AllMultipointGroupsApplyProxyStarInterconnect) Perform(ctx context.Context, modifier string, param event.Params) (context.Context, bool, error) {

	groups, err := registry.MultipointGroupRegistry.List(ctx, nil, false, false)
	if err != nil {
		return ctx, false, nil
	}
	if len(groups) == 0 {
		return ctx, true, nil
	}
	conns, err := registry.ConnRegistry.List(ctx, nil, false, true)
	if err != nil {
		return ctx, false, nil
	}
	connsNum := len(conns)
	if connsNum == 0 {
		return ctx, true, nil
	}
	proxies, err := registry.MediaProxyRegistry.List(ctx, nil, false, true)
	if err != nil {
		return ctx, false, nil
	}
	proxiesNum := len(proxies)
	if proxiesNum == 0 {
		return ctx, true, nil
	}
	bridges, err := registry.BridgeRegistry.List(ctx, nil, false, true)
	if err != nil {
		return ctx, false, nil
	}
	bridgesNum := len(bridges)

	connsMap := make(map[string]*model.Connection, connsNum)
	for i := range conns {
		if conns[i].Config == nil {
			logrus.Errorf("Star interconnect: conn cfg is nil, id: '%s'", conns[i].Id)
			return ctx, false, nil
		}
		connsMap[conns[i].Id] = &conns[i]
	}
	proxiesMap := make(map[string]*model.MediaProxy, proxiesNum)
	for i := range proxies {
		if proxies[i].Config == nil {
			logrus.Errorf("Star interconnect: proxy cfg is nil, id: '%s'", proxies[i].Id)
			return ctx, false, nil
		}
		proxiesMap[proxies[i].Id] = &proxies[i]
	}
	bridgesMap := make(map[string]*model.Bridge, bridgesNum)
	for i := range bridges {
		if bridges[i].Config == nil {
			logrus.Errorf("Star interconnect: bridge cfg is nil, id: '%s'", bridges[i].Id)
			return ctx, false, nil
		}
		bridgesMap[bridges[i].Id] = &bridges[i]
	}
	proxyRDMAPortsMap := make(map[string]*model.PortMask, proxiesNum)
	for proxyId, proxy := range proxiesMap {

		// Build the mask of RDMA listening ports in use
		var portMask model.PortMask
		for _, bridgeId := range proxy.BridgeIds {
			bridge, ok := bridgesMap[bridgeId]
			if !ok {
				logrus.Warnf("Star interconnect: proxy bridge id not found: '%v', id: '%s'", bridgeId, proxyId)
				continue
			}
			if bridge.Config.Kind == "rx" && bridge.Config.Type == "rdma" && bridge.Config.RDMA != nil {
				portMask.SetBit(bridge.Config.RDMA.Port)
			}
		}
		proxyRDMAPortsMap[proxyId] = &portMask
	}

	type groupStarConfig struct {
		SourceProxyId string
		DestProxyIds  []string
	}
	groupStarConfigs := make(map[string]groupStarConfig)

	for _, group := range groups {

		// Looking for an active sourceConnId in the group
		var sourceConnId string
		var sourceProxyId string
		for _, connId := range group.ConnIds {
			conn, ok := connsMap[connId]
			if !ok {
				logrus.Errorf("Star interconnect: conn not found, id: '%s'", connId)
				return ctx, false, nil
			}
			if conn.Config.Kind == "rx" {
				if len(sourceConnId) == 0 {
					sourceConnId = connId
					sourceProxyId = conn.ProxyId
				} else {
					logrus.Errorf("Star interconnect: multiple sources, conn id: '%s', first: '%s'", connId, sourceConnId)
					return ctx, false, nil
				}
			}
		}
		if len(sourceConnId) == 0 {
			for _, bridgeId := range group.BridgeIds {
				bridge, ok := bridgesMap[bridgeId]
				if !ok {
					logrus.Errorf("Star interconnect: bridge not found, id: '%s'", bridgeId)
					return ctx, false, nil
				}
				if bridge.Config.Type == "st2110" && bridge.Config.Kind == "rx" {
					if len(sourceConnId) == 0 {
						sourceConnId = bridgeId
						sourceProxyId = bridge.ProxyId
					} else {
						logrus.Errorf("Star interconnect: multiple sources, bridge id: '%s', first: '%s'", bridgeId, sourceConnId)
						return ctx, false, nil
					}
				}
			}
			if len(sourceConnId) == 0 {
				continue
			}
		}

		// Looking for destination points in the group
		destProxyIds := []string{}
		for _, connId := range group.ConnIds {
			conn := connsMap[connId]
			if conn.Config.Kind == "tx" && conn.ProxyId != sourceProxyId {
				destProxyIds = append(destProxyIds, conn.ProxyId)
			}
		}
		if len(destProxyIds) == 0 {
			continue
		}

		groupStarConfigs[group.Id] = groupStarConfig{SourceProxyId: sourceProxyId, DestProxyIds: destProxyIds}
	}

	// for groupId, group := range groupStarConfigs {
	// 	logrus.Debugf("SOURCE PROXY %v, group id %v", group.SourceProxyId, groupId)
	// 	logrus.Debugf("DEST PROXIES %v", group.DestProxyIds)
	// }

	type bridgeConfig struct {
		GroupId  string
		ProxyId  string
		Kind     string
		RemoteIP string
		Port     uint16

		Exists bool
	}

	newBridges := []bridgeConfig{}
	for groupId, group := range groupStarConfigs {
		sourceProxy, ok := proxiesMap[group.SourceProxyId]
		if !ok {
			logrus.Errorf("Star interconnect: src proxy not found, id: '%s'", group.SourceProxyId)
			continue
		}

		for _, destProxyId := range group.DestProxyIds {
			destProxy, ok := proxiesMap[destProxyId]
			if !ok {
				logrus.Errorf("Star interconnect: dest proxy not found, id: '%s'", destProxyId)
				continue
			}

			destBridge := bridgeConfig{
				GroupId:  groupId,
				ProxyId:  destProxyId,
				Kind:     "rx",
				RemoteIP: sourceProxy.Config.RDMA.DataplaneIPAddr,
				// Port:     port, // 9100, // DEBUG
			}
			sourceBridge := bridgeConfig{
				GroupId:  groupId,
				ProxyId:  group.SourceProxyId,
				Kind:     "tx",
				RemoteIP: destProxy.Config.RDMA.DataplaneIPAddr,
				// Port:     port, // 9100, // DEBUG
			}
			newBridges = append(newBridges, destBridge, sourceBridge)
		}
	}

	// for _, v := range newBridges {
	// 	logrus.Debugf("BRIDGE %+v", v)
	// }

	deleteBridges := []string{}

	for i := range bridges {
		bridge := &bridges[i]
		if bridge.Config == nil || bridge.Config.Type != "rdma" || bridge.Config.RDMA == nil {
			continue
		}
		found := false
		for j := range newBridges {
			newBridge := &newBridges[j]
			if bridge.GroupId == newBridge.GroupId &&
				bridge.ProxyId == newBridge.ProxyId &&
				bridge.Config.Kind == newBridge.Kind &&
				bridge.Config.RDMA.RemoteIP == newBridge.RemoteIP {
				// bridge.Config.RDMA.Port == newBridge.Port {
				newBridge.Exists = true
				found = true
			}
		}

		if !found {
			deleteBridges = append(deleteBridges, bridge.Id)
		}
	}

	// logrus.Debugf("DELETE BRIDGES %v", deleteBridges)

	for _, id := range deleteBridges {
		err = registry.BridgeRegistry.Delete(ctx, id)
		if err != nil {
			logrus.Errorf("Star interconnect: error deleting bridge: %v, id: %v", err, id)
		}

		bridge, ok := bridgesMap[id]
		if !ok {
			logrus.Errorf("Star interconnect: bridge id not found: '%v'", id)
			continue
		}

		// Remove bridge from Media Proxy
		err = registry.MediaProxyRegistry.Update_UnlinkBridge(ctx, bridge.ProxyId, id)
		if err != nil {
			logrus.Errorf("Star interconnect: proxy update unlink bridge err: %v, id: '%v'", err, id)
		}

		// Unlink bridge from Multipoint Group
		err = registry.MultipointGroupRegistry.Update_UnlinkBridge(ctx, bridge.GroupId, id)
		if err != nil {
			logrus.Errorf("Star interconnect: groups update unlink bridge err: %v, id: '%v'", err, id)
		}
	}

	for i := range newBridges {
		newBridge := &newBridges[i]
		if newBridge.Exists {
			continue
		}
		// logrus.Debugf("ADD BRIDGE %+v", newBridge)

		// Allocate a new RDMA port number
		if newBridge.Kind == "rx" {
			if i+1 >= len(newBridges) {
				logrus.Errorf("Star interconnect: new bridge tx index out of range, proxy id: '%v', group id: '%v'", newBridge.ProxyId, newBridge.GroupId)
				continue
			}

			proxy, ok := proxiesMap[newBridge.ProxyId]
			if !ok {
				logrus.Errorf("Star interconnect: dest proxy id not found: '%s'", newBridge.ProxyId)
				continue
			}
			portMask, ok := proxyRDMAPortsMap[newBridge.ProxyId]
			if !ok {
				logrus.Errorf("Star interconnect: dest proxy port mask not found, id: '%s'", newBridge.ProxyId)
				continue
			}
			port, err := portMask.AllocateFirstAvailablePort(&proxy.RDMAPortsAllowed)
			if err != nil {
				logrus.Errorf("Star interconnect: dest proxy rdma port err: %v, id: '%s'", err, newBridge.ProxyId)
				continue
			}

			// logrus.Debugf("Star interconnect: rdma port found: %v", port)

			newBridge.Port = port       // assign to ingress bridge
			newBridges[i+1].Port = port // assigne to the corresponding egress bridge
		}

		bridgeConfig := &model.BridgeConfig{
			Kind: newBridge.Kind,
			Type: "rdma",
			RDMA: &model.BridgeRDMAConfig{
				RemoteIP: newBridge.RemoteIP,
				Port:     newBridge.Port,
			},
			Payload: model.Payload{
				Video: &model.PayloadVideo{
					Width:       640,
					Height:      480,
					FPS:         30,
					PixelFormat: "yuv422p10le",
				},
			},
		}

		id, err := registry.BridgeRegistry.Add(ctx,
			model.Bridge{
				ProxyId: newBridge.ProxyId,
				GroupId: newBridge.GroupId,
				Config:  bridgeConfig,
				Status: &model.ConnectionStatus{
					RegisteredAt: model.CustomTime(time.Now()),
					State:        "active", // TODO: Rework this to use string enum?
				},
			},
		)
		if err != nil {
			logrus.Errorf("Star interconnect: error adding new bridge: %v, %+v", err, newBridge)
			continue
		}

		// Add bridge to Media Proxy
		err = registry.MediaProxyRegistry.Update_LinkBridge(ctx, newBridge.ProxyId, id)
		if err != nil {
			logrus.Errorf("Star interconnect: error updating proxy link bridge: %v, %+v", err, newBridge)
			continue
		}

		// Link Bridge to Multipoint Group
		err = registry.MultipointGroupRegistry.Update_LinkBridge(ctx, newBridge.GroupId, id)
		if err != nil {
			logrus.Errorf("Star interconnect: error updating group link bridge: %v, %+v", err, newBridge)
			continue
		}
	}

	return ctx, true, nil
}

func init() {
	RegisterAction("all-multipoint-groups-apply-proxy-star-interconnect", &Action_AllMultipointGroupsApplyProxyStarInterconnect{})
}
