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

func TestConnRegistry(t *testing.T) {
	registry.ConnRegistry.Init()

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	var wg sync.WaitGroup

	wg.Add(1)
	go func() {
		defer wg.Done()
		err := registry.ConnRegistry.Run(ctx)
		require.NoError(t, err)
	}()

	// Test Add
	conn := model.Connection{
		GroupId: "Test Group Id",
		ProxyId: "Test Proxy Id",
		Config:  &model.ConnectionConfig{},
		Status:  &model.ConnectionStatus{},
	}

	id, err := registry.ConnRegistry.Add(ctx, conn)
	require.NoError(t, err)
	require.NotEmpty(t, id)

	// Test Get
	retrievedConn, err := registry.ConnRegistry.Get(ctx, id, false)
	require.NoError(t, err)
	require.Equal(t, id, retrievedConn.Id)
	require.Equal(t, conn.GroupId, retrievedConn.GroupId)
	require.Equal(t, conn.ProxyId, retrievedConn.ProxyId)
	require.NotNil(t, retrievedConn.Status)
	require.Nil(t, retrievedConn.Config)

	retrievedConn, err = registry.ConnRegistry.Get(ctx, id, true)
	require.NoError(t, err)
	require.Equal(t, id, retrievedConn.Id)
	require.Equal(t, conn.GroupId, retrievedConn.GroupId)
	require.Equal(t, conn.ProxyId, retrievedConn.ProxyId)
	require.NotNil(t, retrievedConn.Status)
	require.NotNil(t, retrievedConn.Config)

	// Test List
	conn2 := model.Connection{
		GroupId: "Test Group Id 2",
		ProxyId: "Test Proxy Id 2",
		Config:  &model.ConnectionConfig{},
		Status:  &model.ConnectionStatus{},
	}

	id2, err := registry.ConnRegistry.Add(ctx, conn2)
	require.NoError(t, err)

	conns, err := registry.ConnRegistry.List(ctx, nil, false, false)
	require.NoError(t, err)
	require.Len(t, conns, 2)
	require.Equal(t, id, conns[0].Id)
	require.Equal(t, conn.GroupId, conns[0].GroupId)
	require.Equal(t, conn.ProxyId, conns[0].ProxyId)
	require.Nil(t, conns[0].Status)
	require.Nil(t, conns[0].Config)
	require.Equal(t, id2, conns[1].Id)
	require.Equal(t, conn2.GroupId, conns[1].GroupId)
	require.Equal(t, conn2.ProxyId, conns[1].ProxyId)
	require.Nil(t, conns[1].Status)
	require.Nil(t, conns[1].Config)

	conns, err = registry.ConnRegistry.List(ctx, nil, true, true)
	require.NoError(t, err)
	require.Len(t, conns, 2)
	require.Equal(t, id, conns[0].Id)
	require.NotNil(t, conns[0].Status)
	require.NotNil(t, conns[0].Config)
	require.Equal(t, id2, conns[1].Id)
	require.NotNil(t, conns[1].Status)
	require.NotNil(t, conns[1].Config)

	// Test Update_LinkGroup
	groupId := uuid.NewString()
	err = registry.ConnRegistry.Update_LinkGroup(ctx, id, groupId)
	require.NoError(t, err)

	updatedConn, err := registry.ConnRegistry.Get(ctx, id, false)
	require.NoError(t, err)
	require.Equal(t, groupId, updatedConn.GroupId)

	// Test Update_UnlinkGroup
	err = registry.ConnRegistry.Update_UnlinkGroup(ctx, id)
	require.NoError(t, err)

	updatedConn, err = registry.ConnRegistry.Get(ctx, id, false)
	require.NoError(t, err)
	require.Empty(t, updatedConn.GroupId)

	// Test Delete
	err = registry.ConnRegistry.Delete(ctx, id)
	require.NoError(t, err)

	_, err = registry.ConnRegistry.Get(ctx, id, false)
	require.Error(t, err)

	cancel()
	wg.Wait()
}
