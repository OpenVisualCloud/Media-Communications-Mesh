/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package registry

import "errors"

const (
	FlagAddStatus uint32 = 1 << iota
	FlagAddConfig
)

var (
	ErrResourceNotFound = errors.New("resource not found")
	ErrTypeCastFailed   = errors.New("type cast failed")
)
