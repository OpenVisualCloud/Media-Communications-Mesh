/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package registry_test

import (
	"context"
	"sync"
	"testing"

	"github.com/google/uuid"
	"github.com/stretchr/testify/require"

	"control-plane-agent/internal/model"
	"control-plane-agent/internal/registry"
)

func TestBridgeRegistry(t *testing.T) {
	registry.BridgeRegistry.Init()

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	var wg sync.WaitGroup

	wg.Add(1)
	go func() {
		defer wg.Done()
		err := registry.BridgeRegistry.Run(ctx)
		require.NoError(t, err)
	}()

	// Test Add
	bridge := model.Bridge{
		GroupId: "Test Group Id",
		ProxyId: "Test Proxy Id",
		Config:  &model.BridgeConfig{},
		Status:  &model.ConnectionStatus{},
	}

	id, err := registry.BridgeRegistry.Add(ctx, bridge)
	require.NoError(t, err)
	require.NotEmpty(t, id)

	// Test Get
	retrievedBridge, err := registry.BridgeRegistry.Get(ctx, id, false)
	require.NoError(t, err)
	require.Equal(t, id, retrievedBridge.Id)
	require.Equal(t, bridge.GroupId, retrievedBridge.GroupId)
	require.Equal(t, bridge.ProxyId, retrievedBridge.ProxyId)
	require.NotNil(t, retrievedBridge.Status)
	require.Nil(t, retrievedBridge.Config)

	retrievedBridge, err = registry.BridgeRegistry.Get(ctx, id, true)
	require.NoError(t, err)
	require.Equal(t, id, retrievedBridge.Id)
	require.Equal(t, bridge.GroupId, retrievedBridge.GroupId)
	require.Equal(t, bridge.ProxyId, retrievedBridge.ProxyId)
	require.NotNil(t, retrievedBridge.Status)
	require.NotNil(t, retrievedBridge.Config)

	// Test List
	bridge2 := model.Bridge{
		GroupId: "Test Group Id 2",
		ProxyId: "Test Proxy Id 2",
		Config:  &model.BridgeConfig{},
		Status:  &model.ConnectionStatus{},
	}

	id2, err := registry.BridgeRegistry.Add(ctx, bridge2)
	require.NoError(t, err)

	bridges, err := registry.BridgeRegistry.List(ctx, nil, false, false)
	require.NoError(t, err)
	require.Len(t, bridges, 2)
	require.Equal(t, id, bridges[0].Id)
	require.Equal(t, bridge.GroupId, bridges[0].GroupId)
	require.Equal(t, bridge.ProxyId, bridges[0].ProxyId)
	require.Nil(t, bridges[0].Status)
	require.Nil(t, bridges[0].Config)
	require.Equal(t, id2, bridges[1].Id)
	require.Equal(t, bridge2.GroupId, bridges[1].GroupId)
	require.Equal(t, bridge2.ProxyId, bridges[1].ProxyId)
	require.Nil(t, bridges[1].Status)
	require.Nil(t, bridges[1].Config)

	bridges, err = registry.BridgeRegistry.List(ctx, nil, true, true)
	require.NoError(t, err)
	require.Len(t, bridges, 2)
	require.Equal(t, id, bridges[0].Id)
	require.NotNil(t, bridges[0].Status)
	require.NotNil(t, bridges[0].Config)
	require.Equal(t, id2, bridges[1].Id)
	require.NotNil(t, bridges[1].Status)
	require.NotNil(t, bridges[1].Config)

	// Test Update_LinkGroup
	groupId := uuid.NewString()
	err = registry.BridgeRegistry.Update_LinkGroup(ctx, id, groupId)
	require.NoError(t, err)

	updatedBridge, err := registry.BridgeRegistry.Get(ctx, id, false)
	require.NoError(t, err)
	require.Equal(t, groupId, updatedBridge.GroupId)

	// Test Update_UnlinkGroup
	err = registry.BridgeRegistry.Update_UnlinkGroup(ctx, id)
	require.NoError(t, err)

	updatedBridge, err = registry.BridgeRegistry.Get(ctx, id, false)
	require.NoError(t, err)
	require.Empty(t, updatedBridge.GroupId)

	// Test Delete
	err = registry.BridgeRegistry.Delete(ctx, id)
	require.NoError(t, err)

	_, err = registry.BridgeRegistry.Get(ctx, id, false)
	require.Error(t, err)

	cancel()
	wg.Wait()
}
