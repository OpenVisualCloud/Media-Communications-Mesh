/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package registry

import (
	"context"
	"fmt"
	"sync"

	"control-plane-agent/internal/event"
	"control-plane-agent/internal/model"
)

type multipointGroupRegistry struct {
	Registry
	Mx sync.Mutex // TODO: Review the mutex usage and consider if it can be removed.
}

var MultipointGroupRegistry multipointGroupRegistry

func (r *multipointGroupRegistry) Init() {
	r.handler = r
	r.Registry.Init()
}

func (r *multipointGroupRegistry) HandleReadMany(req Request) {
	out := []model.MultipointGroup{}
	for _, id := range req.Ids {
		item, ok := r.items[id]
		if !ok {
			continue
		}
		group, ok := item.(model.MultipointGroup)
		if !ok {
			req.Reply <- Reply{Err: ErrTypeCastFailed}
			return
		}
		group.Id = id
		if group.Status != nil {
			if req.Flags&FlagAddStatus != 0 {
				v := *group.Status
				group.Status = &v
			} else {
				group.Status = nil
			}
		}
		if group.Config != nil {
			if req.Flags&FlagAddConfig != 0 {
				v := *group.Config
				group.Config = &v
			} else {
				group.Config = nil
			}
		}
		sz := len(group.ConnIds)
		if sz > 0 {
			connIds := make([]string, sz)
			copy(connIds, group.ConnIds)
			group.ConnIds = connIds
		}
		out = append(out, group)
	}
	req.Reply <- Reply{Data: out}
}

func (r *multipointGroupRegistry) List(ctx context.Context, filterIds []string, addStatus, addConfig bool) ([]model.MultipointGroup, error) {
	req := NewRequest(OpReadMany)
	req.Ids = filterIds
	if addStatus {
		req.Flags |= FlagAddStatus
	}
	if addConfig {
		req.Flags |= FlagAddConfig
	}
	reply, err := r.ExecRequest(ctx, req)
	if err != nil {
		return nil, fmt.Errorf("multipoint group registry list all exec req err: %w", err)
	}
	ret, ok := reply.Data.([]model.MultipointGroup)
	if !ok {
		return nil, fmt.Errorf("multipoint group registry list all err: %w", ErrTypeCastFailed)
	}
	return ret, nil
}

func (r *multipointGroupRegistry) HandleReadOne(req Request, item interface{}) {
	group, ok := item.(model.MultipointGroup)
	if !ok {
		req.Reply <- Reply{Err: ErrTypeCastFailed}
		return
	}
	group.Id = req.Id
	if group.Status != nil {
		v := *group.Status
		group.Status = &v
	}
	if group.Config != nil {
		if req.Flags&FlagAddConfig != 0 {
			v := *group.Config
			group.Config = &v
		} else {
			group.Config = nil
		}
	}
	sz := len(group.ConnIds)
	if sz > 0 {
		connIds := make([]string, sz)
		copy(connIds, group.ConnIds)
		group.ConnIds = connIds
	}
	req.Reply <- Reply{Data: group}
}

func (r *multipointGroupRegistry) Get(ctx context.Context, id string, addConfig bool) (model.MultipointGroup, error) {
	req := NewRequest(OpReadOne)
	req.Id = id
	if addConfig {
		req.Flags |= FlagAddConfig
	}
	reply, err := r.ExecRequest(ctx, req)
	if err != nil {
		return model.MultipointGroup{}, fmt.Errorf("multipoint group registry get exec req err: %w", err)
	}
	ret, ok := reply.Data.(model.MultipointGroup)
	if !ok {
		return model.MultipointGroup{}, fmt.Errorf("multipoint group registry get err: %w", ErrTypeCastFailed)
	}
	return ret, nil
}

func (r *multipointGroupRegistry) Add(ctx context.Context, group model.MultipointGroup) (string, error) {
	req := NewRequest(OpAddOne)
	req.Id = group.Id // use the group id (URN) as a Multipoint Group id instead of a generated uuid
	req.Data = group
	reply, err := r.ExecRequest(ctx, req)
	if err != nil {
		return "", fmt.Errorf("multipoint group registry add exec req err: %w", err)
	}
	event.PostEventAsync(ctx, event.OnMultipointGroupAdded, map[string]interface{}{"group_id": reply.Id})
	return reply.Id, nil
}

func (r *multipointGroupRegistry) HandleDeleteOne(req Request, item interface{}) {
	req.Reply <- Reply{}
}

func (r *multipointGroupRegistry) Delete(ctx context.Context, id string) error {
	req := NewRequest(OpDeleteOne)
	req.Id = id
	_, err := r.ExecRequest(ctx, req)
	if err != nil {
		return fmt.Errorf("multipoint group registry del exec req err: %w", err)
	}
	event.PostEventAsync(ctx, event.OnMultipointGroupDeleted, map[string]interface{}{"group_id": id})
	return nil
}

func (r *multipointGroupRegistry) HandleUpdateOne(req Request, item interface{}) (interface{}, error) {
	group, ok := item.(model.MultipointGroup)
	if !ok {
		return nil, ErrTypeCastFailed
	}
	fn, ok := req.Data.(func(group *model.MultipointGroup) error)
	if !ok {
		return nil, ErrTypeCastFailed
	}
	err := fn(&group)
	if err != nil {
		return nil, err
	}
	return group, nil
}

func (r *multipointGroupRegistry) Update_LinkConn(ctx context.Context, id string, connId string) error {
	req := NewRequest(OpUpdateOne)
	req.Id = id
	req.Data = func(group *model.MultipointGroup) error {
		found := false
		for _, v := range group.ConnIds {
			if v == connId {
				found = true
				break
			}
		}
		if !found {
			group.ConnIds = append(group.ConnIds, connId)
		}
		return nil
	}
	_, err := r.ExecRequest(ctx, req)
	if err != nil {
		return fmt.Errorf("multipoint group registry update link conn exec req err: %w", err)
	}
	event.PostEventAsync(ctx, event.OnMultipointGroupUpdated, map[string]interface{}{"group_id": id})
	return nil
}

func (r *multipointGroupRegistry) Update_UnlinkConn(ctx context.Context, id string, connId string) error {
	req := NewRequest(OpUpdateOne)
	req.Id = id
	req.Data = func(group *model.MultipointGroup) error {
		for i, v := range group.ConnIds {
			if v == connId {
				group.ConnIds = append(group.ConnIds[:i], group.ConnIds[i+1:]...)
				return nil
			}
		}
		return ErrResourceNotFound
	}
	_, err := r.ExecRequest(ctx, req)
	if err != nil {
		return fmt.Errorf("multipoint group registry update exec unlink conn req err: %w", err)
	}
	event.PostEventAsync(ctx, event.OnMultipointGroupUpdated, map[string]interface{}{"group_id": id})
	return nil
}

func (r *multipointGroupRegistry) Update_LinkBridge(ctx context.Context, id string, bridgeId string) error {
	req := NewRequest(OpUpdateOne)
	req.Id = id
	req.Data = func(group *model.MultipointGroup) error {
		found := false
		for _, v := range group.BridgeIds {
			if v == bridgeId {
				found = true
				break
			}
		}
		if !found {
			group.BridgeIds = append(group.BridgeIds, bridgeId)
		}
		return nil
	}
	_, err := r.ExecRequest(ctx, req)
	if err != nil {
		return fmt.Errorf("multipoint group registry update link bridge exec req err: %w", err)
	}
	event.PostEventAsync(ctx, event.OnMultipointGroupUpdated, map[string]interface{}{"group_id": id})
	return nil
}

func (r *multipointGroupRegistry) Update_UnlinkBridge(ctx context.Context, id string, bridgeId string) error {
	req := NewRequest(OpUpdateOne)
	req.Id = id
	req.Data = func(group *model.MultipointGroup) error {
		for i, v := range group.BridgeIds {
			if v == bridgeId {
				group.BridgeIds = append(group.BridgeIds[:i], group.BridgeIds[i+1:]...)
				return nil
			}
		}
		return ErrResourceNotFound
	}
	_, err := r.ExecRequest(ctx, req)
	if err != nil {
		return fmt.Errorf("multipoint group registry update exec unlink bridge req err: %w", err)
	}
	event.PostEventAsync(ctx, event.OnMultipointGroupUpdated, map[string]interface{}{"group_id": id})
	return nil
}
