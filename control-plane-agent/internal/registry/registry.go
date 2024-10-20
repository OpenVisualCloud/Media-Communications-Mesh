/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package registry

import (
	"context"
	"errors"
	"fmt"

	"github.com/google/uuid"
)

type Operation int

const (
	OpReadMany  Operation = iota // input: id range, output: array of struct
	OpReadOne                    // input: id,       output: struct
	OpAddOne                     // input: struct,   output: id
	OpDeleteOne                  // input: id,       output: none
)

type Request struct {
	Operation Operation
	Id        string
	Data      interface{}
	Flags     uint32
	Reply     chan Reply
}

type Reply struct {
	Id   string
	Data interface{}
	Err  error
}

func NewRequest(op Operation) Request {
	return Request{
		Operation: op,
		Reply:     make(chan Reply, 1),
	}
}

type RequestHandler interface {
	HandleReadMany(req Request)
	HandleReadOne(req Request, item interface{})
	HandleDeleteOne(req Request, item interface{})
}

type Registry struct {
	queue   chan Request
	items   map[string]interface{}
	handler RequestHandler
}

func (r *Registry) Run(ctx context.Context) error {
	if r.handler == nil {
		return errors.New("no request handler assigned")
	}

	r.queue = make(chan Request, 100) // TODO: capacity to be configured
	r.items = make(map[string]interface{})

	for {
		select {
		case <-ctx.Done():
			return nil

		case req, ok := <-r.queue:
			if !ok {
				return nil
			}

			switch req.Operation {

			case OpReadMany:
				if req.Id != "" {
					req.Reply <- Reply{Err: errors.New("registry doesn't support read many over id range")}
					break
				}
				r.handler.HandleReadMany(req)

			case OpReadOne:
				item := r.items[req.Id]
				if item == nil {
					req.Reply <- Reply{Err: ErrResourceNotFound}
					break
				}
				r.handler.HandleReadOne(req, item)

			case OpAddOne:
				id := uuid.NewString()
				_, ok := r.items[id]
				if ok {
					req.Reply <- Reply{Err: fmt.Errorf("new uuid already exists in registry (%v)", id)}
					break
				}
				r.items[id] = req.Data
				req.Reply <- Reply{Id: id}

			case OpDeleteOne:
				item, ok := r.items[req.Id]
				if !ok {
					req.Reply <- Reply{Err: ErrResourceNotFound}
					break
				}
				r.handler.HandleDeleteOne(req, item)
				delete(r.items, req.Id)
			}
		}
	}
}

func (r *Registry) ExecRequest(ctx context.Context, req Request) (Reply, error) {
	select {
	case <-ctx.Done():
		return Reply{}, ctx.Err()
	case r.queue <- req:
	}

	select {
	case <-ctx.Done():
		return Reply{}, ctx.Err()
	case reply, ok := <-req.Reply:
		if !ok {
			return Reply{}, errors.New("reply channel closed")
		}
		if reply.Err != nil {
			return Reply{}, reply.Err
		}
		return reply, nil
	}
}
