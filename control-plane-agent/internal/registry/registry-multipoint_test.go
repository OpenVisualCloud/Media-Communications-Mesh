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

	"control-plane-agent/internal/event"
	"control-plane-agent/internal/model"
	"control-plane-agent/internal/registry"
)

type MockEventhandler struct{}

func (h *MockEventhandler) HandleEvent(ctx context.Context, e event.Event) event.Reply {
	return event.Reply{Ctx: ctx}
}

func TestMultipointGroupRegistry(t *testing.T) {
	registry.MultipointGroupRegistry.Init()

	eventHandler := &MockEventhandler{}

	err := event.EventProcessor.Init(event.EventProcessorConfig{EventHandler: eventHandler})
	require.NoError(t, err)

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	var wg sync.WaitGroup

	wg.Add(1)
	go func() {
		defer wg.Done()
		event.EventProcessor.Run(ctx)
	}()

	wg.Add(1)
	go func() {
		defer wg.Done()
		err := registry.MultipointGroupRegistry.Run(ctx)
		require.NoError(t, err)
	}()

	// Test Add
	group := model.MultipointGroup{
		Config: &model.MultipointGroupConfig{},
		Status: &model.ConnectionStatus{},
	}

	id, err := registry.MultipointGroupRegistry.Add(ctx, group)
	require.NoError(t, err)
	require.NotEmpty(t, id)

	// Test Get
	retrievedGroup, err := registry.MultipointGroupRegistry.Get(ctx, id, false)
	require.NoError(t, err)
	require.Equal(t, id, retrievedGroup.Id)
	require.NotNil(t, retrievedGroup.Status)
	require.Nil(t, retrievedGroup.Config)
	require.Len(t, retrievedGroup.ConnIds, 0)
	require.Len(t, retrievedGroup.BridgeIds, 0)

	retrievedGroup, err = registry.MultipointGroupRegistry.Get(ctx, id, true)
	require.NoError(t, err)
	require.Equal(t, id, retrievedGroup.Id)
	require.NotNil(t, retrievedGroup.Status)
	require.NotNil(t, retrievedGroup.Config)
	require.Len(t, retrievedGroup.ConnIds, 0)
	require.Len(t, retrievedGroup.BridgeIds, 0)

	// Test List
	group2 := model.MultipointGroup{
		Config: &model.MultipointGroupConfig{},
		Status: &model.ConnectionStatus{},
	}

	id2, err := registry.MultipointGroupRegistry.Add(ctx, group2)
	require.NoError(t, err)

	groups, err := registry.MultipointGroupRegistry.List(ctx, nil, false, false)
	require.NoError(t, err)
	require.Len(t, groups, 2)
	require.Equal(t, id, groups[0].Id)
	require.Nil(t, groups[0].Status)
	require.Nil(t, groups[0].Config)
	require.Len(t, groups[0].ConnIds, 0)
	require.Len(t, groups[0].BridgeIds, 0)
	require.Equal(t, id2, groups[1].Id)
	require.Nil(t, groups[1].Status)
	require.Nil(t, groups[1].Config)
	require.Len(t, groups[1].ConnIds, 0)
	require.Len(t, groups[1].BridgeIds, 0)

	groups, err = registry.MultipointGroupRegistry.List(ctx, nil, true, true)
	require.NoError(t, err)
	require.Len(t, groups, 2)
	require.Equal(t, id, groups[0].Id)
	require.NotNil(t, groups[0].Status)
	require.NotNil(t, groups[0].Config)
	require.Len(t, groups[0].ConnIds, 0)
	require.Len(t, groups[0].BridgeIds, 0)
	require.Equal(t, id2, groups[1].Id)
	require.NotNil(t, groups[1].Status)
	require.NotNil(t, groups[1].Config)
	require.Len(t, groups[1].ConnIds, 0)
	require.Len(t, groups[1].BridgeIds, 0)

	// Test Update_LinkConn
	connId := uuid.NewString()
	err = registry.MultipointGroupRegistry.Update_LinkConn(ctx, id, connId)
	require.NoError(t, err)

	updatedGroup, err := registry.MultipointGroupRegistry.Get(ctx, id, false)
	require.NoError(t, err)
	require.Len(t, updatedGroup.ConnIds, 1)
	require.Equal(t, connId, updatedGroup.ConnIds[0])
	require.Len(t, updatedGroup.BridgeIds, 0)

	// Test Update_UnlinkGroup
	err = registry.MultipointGroupRegistry.Update_UnlinkConn(ctx, id, connId)
	require.NoError(t, err)

	updatedGroup, err = registry.MultipointGroupRegistry.Get(ctx, id, false)
	require.NoError(t, err)
	require.Len(t, updatedGroup.ConnIds, 0)
	require.Len(t, updatedGroup.BridgeIds, 0)

	// Test Update_LinkBridge
	bridgeId := uuid.NewString()
	err = registry.MultipointGroupRegistry.Update_LinkBridge(ctx, id, bridgeId)
	require.NoError(t, err)

	updatedGroup, err = registry.MultipointGroupRegistry.Get(ctx, id, false)
	require.NoError(t, err)
	require.Len(t, updatedGroup.ConnIds, 0)
	require.Len(t, updatedGroup.BridgeIds, 1)
	require.Equal(t, bridgeId, updatedGroup.BridgeIds[0])

	// Test Update_UnlinkBridge
	err = registry.MultipointGroupRegistry.Update_UnlinkBridge(ctx, id, bridgeId)
	require.NoError(t, err)

	updatedGroup, err = registry.MultipointGroupRegistry.Get(ctx, id, false)
	require.NoError(t, err)
	require.Len(t, updatedGroup.ConnIds, 0)
	require.Len(t, updatedGroup.BridgeIds, 0)

	// Test Delete
	err = registry.MultipointGroupRegistry.Delete(ctx, id)
	require.NoError(t, err)

	_, err = registry.MultipointGroupRegistry.Get(ctx, id, false)
	require.Error(t, err)

	cancel()
	wg.Wait()
}
