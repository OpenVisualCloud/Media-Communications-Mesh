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
}

var MediaProxyRegistry mediaProxyRegistry

func (r *mediaProxyRegistry) Init() {
	r.handler = r
}

func (r *mediaProxyRegistry) HandleReadMany(req Request) {
	out := []model.MediaProxy{}
	for id, item := range r.items {
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
		out = append(out, proxy)
	}
	req.Reply <- Reply{Data: out}
}

func (r *mediaProxyRegistry) ListAll(ctx context.Context, addStatus, addConfig bool) ([]model.MediaProxy, error) {
	req := NewRequest(OpReadMany)
	if addStatus {
		req.Flags |= FlagAddStatus
	}
	if addConfig {
		req.Flags |= FlagAddConfig
	}
	reply, err := r.ExecRequest(ctx, req)
	if err != nil {
		return nil, fmt.Errorf("media proxy registry list all exec req err: %w", err)
	}
	ret, ok := reply.Data.([]model.MediaProxy)
	if !ok {
		return nil, fmt.Errorf("media proxy registry list all err: %w", ErrTypeCastFailed)
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
	proxy.Init()
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
