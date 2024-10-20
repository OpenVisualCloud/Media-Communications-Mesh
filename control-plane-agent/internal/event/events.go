/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package event

import (
	"context"
	"errors"
	"fmt"
	"strings"
	"time"

	"github.com/sirupsen/logrus"
)

type Type int

const (
	OnRegisterMediaProxy Type = iota
	OnRegisterMediaProxyOk
	OnUnregisterMediaProxy
	OnUnregisterMediaProxyOk
	OnRegisterConnection
	OnRegisterConnectionOk
	OnUnregisterConnection
	OnUnregisterConnectionOk
	OnExternalCreateMultipointGroup
	OnExternalCreateMultipointGroupOk
	OnExternalDeleteMultipointGroup
	OnExternalDeleteMultipointGroupOk
	OnExternalCreateBridge
	OnExternalCreateBridgeOk
	OnExternalDeleteBridge
	OnExternalDeleteBridgeOk
	OnLogicRulesUpdated
	OnProxyDisconnected
)

func GetEventDefinitions() map[string]Type {
	return map[string]Type{

		"on-request-register-proxy":    OnRegisterMediaProxy,   // on request to register Media Proxy
		"on-request-register-proxy-ok": OnRegisterMediaProxyOk, // on request to register Media Proxy succeeded

		"on-request-unregister-proxy":    OnUnregisterMediaProxy,   // on request to unregister Media Proxy
		"on-request-unregister-proxy-ok": OnUnregisterMediaProxyOk, // on request to unregister Media Proxy succeeded

		"on-request-register-connection":    OnRegisterConnection,   // on request to register connection
		"on-request-register-connection-ok": OnRegisterConnectionOk, // on request to register connection succeeded

		"on-request-unregister-connection":    OnUnregisterConnection,   // on request to unregister connection
		"on-request-unregister-connection-ok": OnUnregisterConnectionOk, // on request to unregister connection succeeded

		"on-external-request-create-multipoint-group":    OnExternalCreateMultipointGroup,   // on external request to create multipoint group
		"on-external-request-create-multipoint-group-ok": OnExternalCreateMultipointGroupOk, // on external request to create multipoint group succeeded

		"on-external-request-delete-multipoint-group":    OnExternalDeleteMultipointGroup,   // on external request to delete multipoint group
		"on-external-request-delete-multipoint-group-ok": OnExternalDeleteMultipointGroupOk, // on external request to delete multipoint group succeeded

		"on-external-request-create-bridge":    OnExternalCreateBridge,   // on external request to create bridge
		"on-external-request-create-bridge-ok": OnExternalCreateBridgeOk, // on external request to create bridge succeeded

		"on-external-request-delete-bridge":    OnExternalDeleteBridge,   // on external request to delete bridge
		"on-external-request-delete-bridge-ok": OnExternalDeleteBridgeOk, // on external request to delete bridge succeeded

		"on-proxy-disconnected": OnProxyDisconnected, // on proxy disconnected
	}
}

type Event struct {
	Type      Type
	Params    Params
	SyncReply chan Reply // used in PostEventSync
}

type Reply struct {
	Ctx context.Context // output context with values added in actions
	Err error
}

type EventProcessorConfig struct {
	EventHandler EventHandler
}

type EventHandler interface {
	HandleEvent(ctx context.Context, e Event) Reply
}

type eventProcessor struct {
	cfg        EventProcessorConfig
	queue      chan Event
	definition struct {
		events map[Type]string
	}
}

var EventProcessor eventProcessor

func (ep *eventProcessor) Init(cfg EventProcessorConfig) error {
	if cfg.EventHandler == nil {
		return errors.New("event handler must be specified")
	}
	ep.cfg = cfg

	events := GetEventDefinitions()
	ep.definition.events = make(map[Type]string, len(events))
	for k, v := range events {
		ep.definition.events[v] = k
	}

	return nil
}

func (ep *eventProcessor) Run(ctx context.Context) {
	ep.queue = make(chan Event, 1000) // TODO: capacity to be configured

	for {
		select {
		case <-ctx.Done():
			return

		case e, ok := <-ep.queue:
			if !ok {
				return
			}
			ep.logEvent(e)

			// Business logic applies here
			reply := ep.cfg.EventHandler.HandleEvent(ctx, e)
			if reply.Err != nil {
				logrus.Errorf("Event handler err: %v", reply.Err)
			}
			if e.SyncReply != nil {
				e.SyncReply <- reply
			}
		}
	}
}

func PostEventAsync(ctx context.Context, typ Type, params map[string]interface{}) error {
	e := Event{Type: typ, Params: params}
	select {
	case <-ctx.Done():
		return ctx.Err()
	case EventProcessor.queue <- e:
		return nil
	}
}

func PostEventSync(ctx context.Context, typ Type, params map[string]interface{}) Reply {
	e := Event{Type: typ, Params: params, SyncReply: make(chan Reply, 1)}
	select {
	case <-ctx.Done():
		return Reply{Err: ctx.Err()}
	case EventProcessor.queue <- e:
	}

	cctx, ccancel := context.WithTimeout(ctx, 5*time.Second)
	defer ccancel()

	select {
	case <-cctx.Done():
		return Reply{Err: cctx.Err()}
	case reply := <-e.SyncReply:
		return reply
	}
}

func (ep *eventProcessor) logEvent(e Event) {
	out := ""
	for k := range ep.definition.events {
		if k == e.Type {
			lines := []string{}
			for k, v := range e.Params {
				lines = append(lines, fmt.Sprintf("%v=%v", k, v))
			}
			str := strings.Join(lines, ", ")
			if str != "" {
				str = "{" + str + "}"
			}
			out = fmt.Sprintf("%v %v", ep.definition.events[k], str)
		}
	}
	if out == "" {
		out = fmt.Sprintf("unknown event type (%v)", e.Type)
	}
	logrus.Infof("[EVT] %v", out)
}
