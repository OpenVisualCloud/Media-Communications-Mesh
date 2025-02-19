/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package registry

import (
	"context"
	"fmt"

	"control-plane-agent/internal/model"
)

type bridgeRegistry struct {
	Registry
}

var BridgeRegistry bridgeRegistry

func (r *bridgeRegistry) Init() {
	r.handler = r
	r.Registry.Init()
}

func (r *bridgeRegistry) HandleReadMany(req Request) {
	out := []model.Bridge{}
	for _, id := range req.Ids {
		item, ok := r.items[id]
		if !ok {
			continue
		}
		bridge, ok := item.(model.Bridge)
		if !ok {
			req.Reply <- Reply{Err: ErrTypeCastFailed}
			return
		}
		bridge.Id = id
		if bridge.Status != nil {
			if req.Flags&FlagAddStatus != 0 {
				v := *bridge.Status
				bridge.Status = &v
			} else {
				bridge.Status = nil
			}
		}
		if bridge.Config != nil {
			if req.Flags&FlagAddConfig != 0 {
				v := *bridge.Config
				bridge.Config = &v
			} else {
				bridge.Config = nil
			}
		}
		out = append(out, bridge)
	}
	req.Reply <- Reply{Data: out}
}

func (r *bridgeRegistry) List(ctx context.Context, filterIds []string, addStatus, addConfig bool) ([]model.Bridge, error) {
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
		return nil, fmt.Errorf("bridge registry list all exec req err: %w", err)
	}
	ret, ok := reply.Data.([]model.Bridge)
	if !ok {
		return nil, fmt.Errorf("bridge registry list all err: %w", ErrTypeCastFailed)
	}
	return ret, nil
}

func (r *bridgeRegistry) HandleReadOne(req Request, item interface{}) {
	bridge, ok := item.(model.Bridge)
	if !ok {
		req.Reply <- Reply{Err: ErrTypeCastFailed}
		return
	}
	bridge.Id = req.Id
	if bridge.Status != nil {
		v := *bridge.Status
		bridge.Status = &v
	}
	if bridge.Config != nil {
		if req.Flags&FlagAddConfig != 0 {
			v := *bridge.Config
			bridge.Config = &v
		} else {
			bridge.Config = nil
		}
	}
	req.Reply <- Reply{Data: bridge}
}

func (r *bridgeRegistry) Get(ctx context.Context, id string, addConfig bool) (model.Bridge, error) {
	req := NewRequest(OpReadOne)
	req.Id = id
	if addConfig {
		req.Flags |= FlagAddConfig
	}
	reply, err := r.ExecRequest(ctx, req)
	if err != nil {
		return model.Bridge{}, fmt.Errorf("bridge registry get exec req err: %w", err)
	}
	ret, ok := reply.Data.(model.Bridge)
	if !ok {
		return model.Bridge{}, fmt.Errorf("bridge registry get err: %w", ErrTypeCastFailed)
	}
	return ret, nil
}

func (r *bridgeRegistry) Add(ctx context.Context, bridge model.Bridge) (string, error) {
	req := NewRequest(OpAddOne)
	req.Data = bridge
	reply, err := r.ExecRequest(ctx, req)
	if err != nil {
		return "", fmt.Errorf("bridge registry add exec req err: %w", err)
	}
	return reply.Id, nil
}

func (r *bridgeRegistry) HandleDeleteOne(req Request, item interface{}) {
	req.Reply <- Reply{}
}

func (r *bridgeRegistry) Delete(ctx context.Context, id string) error {
	req := NewRequest(OpDeleteOne)
	req.Id = id
	_, err := r.ExecRequest(ctx, req)
	if err != nil {
		return fmt.Errorf("bridge registry del exec req err: %w", err)
	}
	return nil
}

func (r *bridgeRegistry) HandleUpdateOne(req Request, item interface{}) (interface{}, error) {
	bridge, ok := item.(model.Bridge)
	if !ok {
		return nil, ErrTypeCastFailed
	}
	fn, ok := req.Data.(func(proxy *model.Bridge) error)
	if !ok {
		return nil, ErrTypeCastFailed
	}
	err := fn(&bridge)
	if err != nil {
		return nil, err
	}
	return bridge, nil
}

func (r *bridgeRegistry) Update_LinkGroup(ctx context.Context, id string, groupId string) error {
	req := NewRequest(OpUpdateOne)
	req.Id = id
	req.Data = func(bridge *model.Bridge) error {
		bridge.GroupId = groupId
		return nil
	}
	_, err := r.ExecRequest(ctx, req)
	if err != nil {
		return fmt.Errorf("bridge registry update link group exec req err: %w", err)
	}
	return nil
}

func (r *bridgeRegistry) Update_UnlinkGroup(ctx context.Context, id string) error {
	return r.Update_LinkGroup(ctx, id, "")
}
