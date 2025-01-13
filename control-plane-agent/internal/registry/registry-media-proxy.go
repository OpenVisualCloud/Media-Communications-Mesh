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

type mediaProxyRegistry struct {
	Registry

	cfg MediaProxyRegistryConfig
}

type MediaProxyRegistryConfig struct {
	CancelCommandRequestFunc func(reqId string) // this enables proxies to cancel a command request
}

var MediaProxyRegistry mediaProxyRegistry

func (r *mediaProxyRegistry) Init(cfg MediaProxyRegistryConfig) {
	r.handler = r
	r.cfg = cfg
	r.Registry.Init()
}

func (r *mediaProxyRegistry) HandleReadMany(req Request) {
	out := []model.MediaProxy{}
	for _, id := range req.Ids {
		item, ok := r.items[id]
		if !ok {
			continue
		}
		proxy, ok := item.(model.MediaProxy)
		if !ok {
			req.Reply <- Reply{Err: ErrTypeCastFailed}
			return
		}
		proxy.Id = id
		if proxy.Status != nil {
			if req.Flags&FlagAddStatus != 0 {
				v := *proxy.Status
				proxy.Status = &v
			} else {
				proxy.Status = nil
			}
		}
		if proxy.Config != nil {
			if req.Flags&FlagAddConfig != 0 {
				v := *proxy.Config
				proxy.Config = &v
			} else {
				proxy.Config = nil
			}
		}
		sz := len(proxy.ConnIds)
		if sz > 0 {
			connIds := make([]string, sz)
			copy(connIds, proxy.ConnIds)
			proxy.ConnIds = connIds
		}
		out = append(out, proxy)
	}
	req.Reply <- Reply{Data: out}
}

func (r *mediaProxyRegistry) List(ctx context.Context, filterIds []string, addStatus, addConfig bool) ([]model.MediaProxy, error) {
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
		return nil, fmt.Errorf("media proxy registry list many exec req err: %w", err)
	}
	ret, ok := reply.Data.([]model.MediaProxy)
	if !ok {
		return nil, fmt.Errorf("media proxy registry list many err: %w", ErrTypeCastFailed)
	}
	return ret, nil
}

func (r *mediaProxyRegistry) HandleReadOne(req Request, item interface{}) {
	proxy, ok := item.(model.MediaProxy)
	if !ok {
		req.Reply <- Reply{Err: ErrTypeCastFailed}
		return
	}
	proxy.Id = req.Id
	if proxy.Status != nil {
		v := *proxy.Status
		proxy.Status = &v
	}
	if proxy.Config != nil {
		if req.Flags&FlagAddConfig != 0 {
			v := *proxy.Config
			proxy.Config = &v
		} else {
			proxy.Config = nil
		}
	}
	sz := len(proxy.ConnIds)
	if sz > 0 {
		connIds := make([]string, sz)
		copy(connIds, proxy.ConnIds)
		proxy.ConnIds = connIds
	}
	req.Reply <- Reply{Data: proxy}
}

func (r *mediaProxyRegistry) Get(ctx context.Context, id string, addConfig bool) (model.MediaProxy, error) {
	req := NewRequest(OpReadOne)
	req.Id = id
	if addConfig {
		req.Flags |= FlagAddConfig
	}
	reply, err := r.ExecRequest(ctx, req)
	if err != nil {
		return model.MediaProxy{}, fmt.Errorf("media proxy registry get exec req err: %w", err)
	}
	ret, ok := reply.Data.(model.MediaProxy)
	if !ok {
		return model.MediaProxy{}, fmt.Errorf("media proxy registry get err: %w", ErrTypeCastFailed)
	}
	return ret, nil
}

func (r *mediaProxyRegistry) Add(ctx context.Context, proxy model.MediaProxy) (string, error) {
	req := NewRequest(OpAddOne)
	proxy.Init(r.cfg.CancelCommandRequestFunc)

	req.Data = proxy
	reply, err := r.ExecRequest(ctx, req)
	if err != nil {
		proxy.Deinit()
		return "", fmt.Errorf("media proxy registry add exec req err: %w", err)
	}
	return reply.Id, nil
}

func (r *mediaProxyRegistry) HandleDeleteOne(req Request, item interface{}) {
	proxy, ok := item.(model.MediaProxy)
	if !ok {
		req.Reply <- Reply{Err: ErrTypeCastFailed}
		return
	}
	proxy.Deinit()
	req.Reply <- Reply{}
}

func (r *mediaProxyRegistry) Delete(ctx context.Context, id string) error {
	req := NewRequest(OpDeleteOne)
	req.Id = id
	_, err := r.ExecRequest(ctx, req)
	if err != nil {
		return fmt.Errorf("media proxy registry del exec req err: %w", err)
	}
	return nil
}

func (r *mediaProxyRegistry) HandleUpdateOne(req Request, item interface{}) (interface{}, error) {
	proxy, ok := item.(model.MediaProxy)
	if !ok {
		return nil, ErrTypeCastFailed
	}
	fn, ok := req.Data.(func(proxy *model.MediaProxy) error)
	if !ok {
		return nil, ErrTypeCastFailed
	}
	err := fn(&proxy)
	if err != nil {
		return nil, err
	}
	return proxy, nil
}

func (r *mediaProxyRegistry) Update_LinkConn(ctx context.Context, id string, connId string) error {
	req := NewRequest(OpUpdateOne)
	req.Id = id
	req.Data = func(proxy *model.MediaProxy) error {
		found := false
		for _, v := range proxy.ConnIds {
			if v == connId {
				found = true
				break
			}
		}
		if !found {
			proxy.ConnIds = append(proxy.ConnIds, connId)
		}
		if proxy.Status != nil {
			proxy.Status.ConnsNum = len(proxy.ConnIds)
		}
		return nil
	}
	_, err := r.ExecRequest(ctx, req)
	if err != nil {
		return fmt.Errorf("media proxy registry update link conn exec req err: %w", err)
	}
	return nil
}

func (r *mediaProxyRegistry) Update_UnlinkConn(ctx context.Context, id string, connId string) error {
	req := NewRequest(OpUpdateOne)
	req.Id = id
	req.Data = func(proxy *model.MediaProxy) error {
		for i, v := range proxy.ConnIds {
			if v == connId {
				proxy.ConnIds = append(proxy.ConnIds[:i], proxy.ConnIds[i+1:]...)
				if proxy.Status != nil {
					proxy.Status.ConnsNum = len(proxy.ConnIds)
				}
				return nil
			}
		}
		return ErrResourceNotFound
	}
	_, err := r.ExecRequest(ctx, req)
	if err != nil {
		return fmt.Errorf("media proxy registry update exec unlink conn req err: %w", err)
	}
	return nil
}

func (r *mediaProxyRegistry) Update_LinkBridge(ctx context.Context, id string, bridgeId string) error {
	req := NewRequest(OpUpdateOne)
	req.Id = id
	req.Data = func(proxy *model.MediaProxy) error {
		found := false
		for _, v := range proxy.BridgeIds {
			if v == bridgeId {
				found = true
				break
			}
		}
		if !found {
			proxy.BridgeIds = append(proxy.BridgeIds, bridgeId)
		}
		if proxy.Status != nil {
			proxy.Status.BridgesNum = len(proxy.BridgeIds)
		}
		return nil
	}
	_, err := r.ExecRequest(ctx, req)
	if err != nil {
		return fmt.Errorf("media proxy registry update link bridge exec req err: %w", err)
	}
	return nil
}

func (r *mediaProxyRegistry) Update_UnlinkBridge(ctx context.Context, id string, bridgeId string) error {
	req := NewRequest(OpUpdateOne)
	req.Id = id
	req.Data = func(proxy *model.MediaProxy) error {
		for i, v := range proxy.BridgeIds {
			if v == bridgeId {
				proxy.BridgeIds = append(proxy.BridgeIds[:i], proxy.BridgeIds[i+1:]...)
				if proxy.Status != nil {
					proxy.Status.BridgesNum = len(proxy.BridgeIds)
				}
				return nil
			}
		}
		return ErrResourceNotFound
	}
	_, err := r.ExecRequest(ctx, req)
	if err != nil {
		return fmt.Errorf("media proxy registry update exec unlink bridge req err: %w", err)
	}
	return nil
}
