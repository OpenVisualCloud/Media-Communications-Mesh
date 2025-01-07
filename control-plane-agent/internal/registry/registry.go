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
	OpReadMany  Operation = iota // input: id range,   output: array of struct
	OpReadOne                    // input: id,         output: struct
	OpAddOne                     // input: struct,     output: id
	OpDeleteOne                  // input: id,         output: none
	OpUpdateOne                  // input: id, struct, output: none
)

type Request struct {
	Operation Operation
	Id        string   // for OpReadOne, OpUpdateOne, OpDeleteOne
	Ids       []string // for OpReadMany
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
	HandleUpdateOne(req Request, item interface{}) (interface{}, error)
}

type Registry struct {
	queue      chan Request
	items      map[string]interface{}
	orderedIds []string
	handler    RequestHandler
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
				if len(req.Ids) == 0 {
					req.Ids = r.orderedIds
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
				if req.Id == "" {
					req.Id = uuid.NewString()
				}
				_, ok := r.items[req.Id]
				if ok {
					req.Reply <- Reply{Err: fmt.Errorf("new uuid already exists in registry (%v)", req.Id)}
					break
				}
				r.items[req.Id] = req.Data
				r.orderedIds = append(r.orderedIds, req.Id)
				req.Reply <- Reply{Id: req.Id}

			case OpDeleteOne:
				item, ok := r.items[req.Id]
				if !ok {
					req.Reply <- Reply{Err: ErrResourceNotFound}
					break
				}
				r.handler.HandleDeleteOne(req, item)
				delete(r.items, req.Id)
				for i, v := range r.orderedIds {
					if v == req.Id {
						r.orderedIds = append(r.orderedIds[:i], r.orderedIds[i+1:]...)
						break
					}
				}

			case OpUpdateOne:
				item := r.items[req.Id]
				if item == nil {
					req.Reply <- Reply{Err: ErrResourceNotFound}
					break
				}
				newItem, err := r.handler.HandleUpdateOne(req, item)
				if err == nil {
					r.items[req.Id] = newItem
				}
				req.Reply <- Reply{Err: err}
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
