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

type connRegistry struct {
	Registry
}

var ConnRegistry connRegistry

func (r *connRegistry) Init() {
	r.handler = r
}

func (r *connRegistry) HandleReadMany(req Request) {
	out := []model.Connection{}
	for id, item := range r.items {
		conn, ok := item.(model.Connection)
		if !ok {
			req.Reply <- Reply{Err: ErrTypeCastFailed}
			return
		}
		conn.Id = id
		if conn.Status != nil {
			if req.Flags&FlagAddStatus != 0 {
				v := *conn.Status
				conn.Status = &v
			} else {
				conn.Status = nil
			}
		}
		if conn.Config != nil {
			if req.Flags&FlagAddConfig != 0 {
				v := *conn.Config
				conn.Config = &v
			} else {
				conn.Config = nil
			}
		}
		out = append(out, conn)
	}
	req.Reply <- Reply{Data: out}
}

func (r *connRegistry) ListAll(ctx context.Context, addStatus, addConfig bool) ([]model.Connection, error) {
	req := NewRequest(OpReadMany)
	if addStatus {
		req.Flags |= FlagAddStatus
	}
	if addConfig {
		req.Flags |= FlagAddConfig
	}
	reply, err := r.ExecRequest(ctx, req)
	if err != nil {
		return nil, fmt.Errorf("conn registry list all exec req err: %w", err)
	}
	ret, ok := reply.Data.([]model.Connection)
	if !ok {
		return nil, fmt.Errorf("conn registry list all err: %w", ErrTypeCastFailed)
	}
	return ret, nil
}

func (r *connRegistry) HandleReadOne(req Request, item interface{}) {
	conn, ok := item.(model.Connection)
	if !ok {
		req.Reply <- Reply{Err: ErrTypeCastFailed}
		return
	}
	conn.Id = req.Id
	if conn.Status != nil {
		v := *conn.Status
		conn.Status = &v
	}
	if conn.Config != nil {
		if req.Flags&FlagAddConfig != 0 {
			v := *conn.Config
			conn.Config = &v
		} else {
			conn.Config = nil
		}
	}
	req.Reply <- Reply{Data: conn}
}

func (r *connRegistry) Get(ctx context.Context, id string, addConfig bool) (model.Connection, error) {
	req := NewRequest(OpReadOne)
	req.Id = id
	if addConfig {
		req.Flags |= FlagAddConfig
	}
	reply, err := r.ExecRequest(ctx, req)
	if err != nil {
		return model.Connection{}, fmt.Errorf("conn registry get exec req err: %w", err)
	}
	ret, ok := reply.Data.(model.Connection)
	if !ok {
		return model.Connection{}, fmt.Errorf("conn registry get err: %w", ErrTypeCastFailed)
	}
	return ret, nil
}

func (r *connRegistry) Add(ctx context.Context, conn model.Connection) (string, error) {
	req := NewRequest(OpAddOne)
	req.Data = conn
	reply, err := r.ExecRequest(ctx, req)
	if err != nil {
		return "", fmt.Errorf("conn registry add exec req err: %w", err)
	}
	return reply.Id, nil
}

func (r *connRegistry) HandleDeleteOne(req Request, item interface{}) {
	req.Reply <- Reply{}
}

func (r *connRegistry) Delete(ctx context.Context, id string) error {
	req := NewRequest(OpDeleteOne)
	req.Id = id
	_, err := r.ExecRequest(ctx, req)
	if err != nil {
		return fmt.Errorf("conn registry del exec req err: %w", err)
	}
	return nil
}
