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
	for _, id := range req.Ids {
		item, ok := r.items[id]
		if !ok {
			continue
		}
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

func (r *connRegistry) List(ctx context.Context, filterIds []string, addStatus, addConfig bool) ([]model.Connection, error) {
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

func (r *connRegistry) HandleUpdateOne(req Request, item interface{}) (interface{}, error) {
	conn, ok := item.(model.Connection)
	if !ok {
		return nil, ErrTypeCastFailed
	}
	fn, ok := req.Data.(func(proxy *model.Connection) error)
	if !ok {
		return nil, ErrTypeCastFailed
	}
	err := fn(&conn)
	if err != nil {
		return nil, err
	}
	return conn, nil
}

func (r *connRegistry) Update_LinkGroup(ctx context.Context, id string, groupId string) error {
	req := NewRequest(OpUpdateOne)
	req.Id = id
	req.Data = func(conn *model.Connection) error {
		conn.GroupId = groupId
		return nil
	}
	_, err := r.ExecRequest(ctx, req)
	if err != nil {
		return fmt.Errorf("conn registry update link group exec req err: %w", err)
	}
	return nil
}

func (r *connRegistry) Update_UnlinkGroup(ctx context.Context, id string) error {
	return r.Update_LinkGroup(ctx, id, "")
}
