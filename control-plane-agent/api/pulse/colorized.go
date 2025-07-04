/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package pulse

import (
	"fmt"

	"github.com/fatih/color"
)

var ColorRed func(a ...interface{}) string = color.New(color.FgHiRed).SprintFunc()
var ColorYellow func(a ...interface{}) string = color.New(color.FgHiYellow).SprintFunc()
var ColorGreen func(a ...interface{}) string = color.New(color.FgGreen).SprintFunc()
var ColorMagenta func(a ...interface{}) string = color.New(color.FgHiMagenta).SprintFunc()

func colorizedErrors(errors uint32) string {
	out := fmt.Sprintf("%v", errors)
	if errors > 0 {
		return ColorRed(out)
	}
	return out
}

func colorizedConnState(state string) string {
	switch state {
	case "active":
		return ColorGreen(state)
	case "closed":
		return ColorRed(state)
	case "suspended":
		return ColorMagenta(state)
	default:
		return ColorYellow(state)
	}
}
