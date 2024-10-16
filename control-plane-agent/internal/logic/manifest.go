/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package logic

import (
	"bytes"
	_ "embed"
	"encoding/json"
	"fmt"

	"gopkg.in/yaml.v3"
)

type ManifestYaml struct {
	Events yaml.Node `yaml:"logic"`
}

type ManifestActionYaml struct {
	ResultTrue    yaml.Node `yaml:"true"`
	ResultFalse   yaml.Node `yaml:"false"`
	ResultSuccess yaml.Node `yaml:"success"`
	ResultError   yaml.Node `yaml:"error"`
}

type manifest struct {
	Events []manifestEvent `json:"events"`
}

type manifestEvent struct {
	Name    string           `json:"name"`
	Actions []manifestAction `json:"actions,omitempty"`
}

type manifestAction struct {
	Name                 string           `json:"name"`
	ResultTrueActions    []manifestAction `json:"resultTrueActions,omitempty"`
	ResultFalseActions   []manifestAction `json:"resultFalseActions,omitempty"`
	ResultSuccessActions []manifestAction `json:"resultSuccessActions,omitempty"`
	ResultErrorActions   []manifestAction `json:"resultErrorActions,omitempty"`
}

//go:embed default.yaml
var defaultManifestYaml string

func (lc *logicController) decodeActionsRecursive(content []*yaml.Node) ([]manifestAction, error) {
	actions := []manifestAction{}

	actionLen := len(content)
	for i, node := range content {
		if node.Tag != "!!str" {
			continue
		}

		actionName := node.Value
		_, ok := lc.definition.actions[actionName]
		if !ok {
			return nil, fmt.Errorf("unknown action name (%v)", actionName)
		}

		if i+1 >= actionLen {
			return nil, fmt.Errorf("not enough items in events content (%v)", i)
		}

		var actionYaml ManifestActionYaml

		err := content[i+1].Decode(&actionYaml)
		if err != nil {
			return nil, fmt.Errorf("action decode err: %v", err)
		}

		// check if only allowed result types exist in the YAML manifest
		for _, node := range content[i+1].Content {
			switch node.Value {
			case "true", "false", "success", "error", "":
			default:
				return nil, fmt.Errorf("unknown result type (%v)", node.Value)
			}
		}

		resTrueActions, err := lc.decodeActionsRecursive(actionYaml.ResultTrue.Content)
		if err != nil {
			return nil, fmt.Errorf("result true decode err: %w", err)
		}
		resFalseActions, err := lc.decodeActionsRecursive(actionYaml.ResultFalse.Content)
		if err != nil {
			return nil, fmt.Errorf("result false decode err: %w", err)
		}
		resSuccessActions, err := lc.decodeActionsRecursive(actionYaml.ResultSuccess.Content)
		if err != nil {
			return nil, fmt.Errorf("result success decode err: %w", err)
		}
		resErrorActions, err := lc.decodeActionsRecursive(actionYaml.ResultError.Content)
		if err != nil {
			return nil, fmt.Errorf("result error decode err: %w", err)
		}

		actions = append(actions, manifestAction{
			Name:                 node.Value,
			ResultTrueActions:    resTrueActions,
			ResultFalseActions:   resFalseActions,
			ResultSuccessActions: resSuccessActions,
			ResultErrorActions:   resErrorActions,
		})
	}

	return actions, nil
}

func (lc *logicController) ParseManifest(text string) error {
	manifestYaml := ManifestYaml{}

	dec := yaml.NewDecoder(bytes.NewReader([]byte(text)))
	dec.KnownFields(true)
	err := dec.Decode(&manifestYaml)
	if err != nil {
		return fmt.Errorf("manifest decode err: %w", err)
	}

	manifest := manifest{}

	evtLen := len(manifestYaml.Events.Content)
	for i, node := range manifestYaml.Events.Content {
		if node.Tag != "!!str" {
			continue
		}

		eventName := node.Value
		_, ok := lc.definition.events[eventName]
		if !ok {
			return fmt.Errorf("unknown event name (%v)", eventName)
		}

		if i+1 >= evtLen {
			return fmt.Errorf("not enough items in events content (%v)", i)
		}

		var eventYaml yaml.Node
		err := manifestYaml.Events.Content[i+1].Decode(&eventYaml)
		if err != nil {
			return fmt.Errorf("event decode err: %w", err)
		}

		actions, err := lc.decodeActionsRecursive(eventYaml.Content)
		if err != nil {
			return fmt.Errorf("event actions decode err: %w", err)
		}

		manifest.Events = append(manifest.Events, manifestEvent{
			Name:    eventName,
			Actions: actions,
		})
	}

	lc.manifest = manifest

	lc.PrintManifestJson()

	return nil
}

func (lc *logicController) PrintManifestJson() {
	out, err := json.MarshalIndent(lc.manifest, "", "  ")
	if err != nil {
		fmt.Printf("JSON error %v\n", err)
	} else {
		fmt.Printf("JSON manifest\n%v\n", string(out))
	}
}
