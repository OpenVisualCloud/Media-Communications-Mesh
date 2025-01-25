/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package registry

import (
	"context"
	"fmt"
	"sync"
	"testing"

	"github.com/stretchr/testify/require"
)

type MockItem struct {
	Name  string
	Value int
}

type MockRegistry struct {
	Registry
}

func (r *MockRegistry) HandleReadMany(req Request) {
	var items []interface{}
	for _, id := range req.Ids {
		item, ok := r.items[id]
		if !ok {
			continue
		}
		items = append(items, item)
	}
	req.Reply <- Reply{Data: items}
}

func (h *MockRegistry) HandleReadOne(req Request, item interface{}) {
	req.Reply <- Reply{Data: item}
}

func (h *MockRegistry) HandleDeleteOne(req Request, item interface{}) {
	req.Reply <- Reply{}
}

func (h *MockRegistry) HandleUpdateOne(req Request, item interface{}) (interface{}, error) {
	return req.Data, nil
}

func TestRegistry(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	reg := &MockRegistry{}
	reg.Registry.Init()
	reg.handler = reg

	var wg sync.WaitGroup
	wg.Add(1)
	go func() {
		defer wg.Done()
		err := reg.Run(ctx)
		require.NoError(t, err)
	}()

	// Add an item
	addReq := NewRequest(OpAddOne)
	addReq.Data = MockItem{Name: "test item", Value: 42}

	addReply, err := reg.ExecRequest(ctx, addReq)
	require.NoError(t, err)
	require.NotEmpty(t, addReply.Id)

	// Read the item
	readReq := NewRequest(OpReadOne)
	readReq.Id = addReply.Id

	readReply, err := reg.ExecRequest(ctx, readReq)
	require.NoError(t, err)
	require.Equal(t, MockItem{Name: "test item", Value: 42}, readReply.Data)

	// Update the item
	updateReq := NewRequest(OpUpdateOne)
	updateReq.Id = addReply.Id
	updateReq.Data = MockItem{Name: "updated item", Value: 84}

	updateReply, err := reg.ExecRequest(ctx, updateReq)
	require.NoError(t, err)
	require.NoError(t, updateReply.Err)

	// Read the updated item
	readReq = NewRequest(OpReadOne)
	readReq.Id = addReply.Id

	readReply, err = reg.ExecRequest(ctx, readReq)
	require.NoError(t, err)
	require.Equal(t, MockItem{Name: "updated item", Value: 84}, readReply.Data)

	// Delete the item
	deleteReq := NewRequest(OpDeleteOne)
	deleteReq.Id = addReply.Id

	deleteReply, err := reg.ExecRequest(ctx, deleteReq)
	require.NoError(t, err)
	require.NoError(t, deleteReply.Err)

	// Try to read the deleted item
	readReq = NewRequest(OpReadOne)
	readReq.Id = addReply.Id

	readReply, err = reg.ExecRequest(ctx, readReq)
	require.Error(t, err)
	require.Equal(t, ErrResourceNotFound, err)
	require.Nil(t, readReply.Data)

	// Add multiple items
	for i := 0; i < 3; i++ {
		addReq := NewRequest(OpAddOne)
		addReq.Data = MockItem{Name: fmt.Sprintf("test item %d", i), Value: i}
		reply, err := reg.ExecRequest(ctx, addReq)
		require.NoError(t, err)
		require.NoError(t, reply.Err)
	}

	// Read all items
	readManyReq := NewRequest(OpReadMany)

	readManyReply, err := reg.ExecRequest(ctx, readManyReq)
	require.NoError(t, err)
	require.Len(t, readManyReply.Data.([]interface{}), 3)

	cancel()
	wg.Wait()
}
