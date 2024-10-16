/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package logic

import (
	"context"
	"fmt"

	"github.com/sirupsen/logrus"

	"control-plane-agent/internal/event"
	"control-plane-agent/internal/logic/actions"
)

type logicController struct {
	manifest   manifest
	definition struct {
		events  map[string]event.Type
		actions map[string]actions.Action
	}
}

var LogicController logicController

func (lc *logicController) Init() error {
	lc.definition.events = event.GetEventDefinitions()
	lc.definition.actions = actions.Registry
	err := lc.ParseManifest(defaultManifestYaml)
	if err != nil {
		return fmt.Errorf("parse manifest err: %w", err)
	}
	return nil
}

// Business logic is applied here
func (lc *logicController) HandleEvent(ctx context.Context, e event.Event) event.Reply {

	var performActionsRecursive func(ctx context.Context, actions []manifestAction) context.Context

	performActionsRecursive = func(ctx context.Context, actions []manifestAction) context.Context {
		for _, actionDeclaration := range actions {
			action, ok := lc.definition.actions[actionDeclaration.Name]
			if !ok {
				continue
			}
			logrus.Infof("[ACT] %v", actionDeclaration.Name)

			var result bool
			var err error
			ctx, result, err = action.Perform(ctx, e.Params)
			if err != nil {
				logrus.Errorf("action err (%v): %v", actionDeclaration.Name, err)
			} else {
				getResultStr := func(result bool) string {
					if result {
						return "Success/True"
					} else {
						return "Error/False"
					}
				}
				logrus.Infof("[ACT] %v (=%v)", actionDeclaration.Name, getResultStr(result))
			}
			if result {
				if len(actionDeclaration.ResultSuccessActions) > 0 {
					logrus.Infof("[ACT] %v (-> SUCCESS branch)", actionDeclaration.Name)
					ctx = performActionsRecursive(ctx, actionDeclaration.ResultSuccessActions)
				} else if len(actionDeclaration.ResultTrueActions) > 0 {
					logrus.Infof("[ACT] %v (-> TRUE branch)", actionDeclaration.Name)
					ctx = performActionsRecursive(ctx, actionDeclaration.ResultTrueActions)
				}
			} else {
				if len(actionDeclaration.ResultErrorActions) > 0 {
					logrus.Infof("[ACT] %v (-> ERROR branch)", actionDeclaration.Name)
					ctx = performActionsRecursive(ctx, actionDeclaration.ResultErrorActions)
				} else if len(actionDeclaration.ResultFalseActions) > 0 {
					logrus.Infof("[ACT] %v (-> FALSE branch)", actionDeclaration.Name)
					ctx = performActionsRecursive(ctx, actionDeclaration.ResultFalseActions)
				}
			}
		}
		return ctx
	}

	initialContext := ctx
	var firstEventOutContext context.Context

	for _, v := range lc.manifest.Events {
		evt, ok := lc.definition.events[v.Name]
		if ok && evt == e.Type {
			outCtx := performActionsRecursive(initialContext, v.Actions)

			// Remember the context returned from the very first action flow in the YAML manifest to handle the event.
			// This context will be returned from the call to PostEventSync().
			if firstEventOutContext == nil {
				firstEventOutContext = outCtx
			}
		}
	}

	if firstEventOutContext == nil {
		firstEventOutContext = initialContext
	}

	return event.Reply{Ctx: firstEventOutContext, Err: nil}
}
