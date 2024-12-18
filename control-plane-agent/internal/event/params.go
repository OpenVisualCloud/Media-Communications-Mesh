/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package event

import "fmt"

type ParamName string // used to pass event parameters as values in context

type Params map[string]interface{}

func (ep Params) getParam(name string) (interface{}, error) {
	p, ok := ep[name]
	if !ok {
		return nil, fmt.Errorf("param not found: %v", name)
	}
	return p, nil
}

func (ep Params) GetString(name string) (string, error) {
	v, err := ep.getParam(name)
	if err != nil {
		return "", fmt.Errorf("string %v", err)
	}
	str, ok := v.(string)
	if !ok {
		return "", fmt.Errorf("string cast failed: %v", name)
	}
	return str, nil
}

func (ep Params) GetUint32(name string) (uint32, error) {
	v, err := ep.getParam(name)
	if err != nil {
		return 0, fmt.Errorf("uint32 %v", err)
	}
	str, ok := v.(uint32)
	if !ok {
		return 0, fmt.Errorf("uint32 cast failed: %v", name)
	}
	return str, nil
}
