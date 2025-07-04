/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package pulse

import (
	"bytes"
	"encoding/json"

	"github.com/olekukonko/tablewriter"
	"github.com/olekukonko/tablewriter/renderer"
	"github.com/olekukonko/tablewriter/tw"
)

func renderTable(fn func(*tablewriter.Table)) string {
	var buf bytes.Buffer
	table := tablewriter.NewTable(&buf, tablewriter.WithRenderer(
		renderer.NewBlueprint(tw.Rendition{
			Borders: tw.BorderNone,
			Settings: tw.Settings{
				Separators: tw.Separators{BetweenColumns: tw.Off},
				Lines:      tw.Lines{ShowHeaderLine: tw.Off},
			},
		}),
	))
	table.Configure(func(config *tablewriter.Config) {
		config.Header.Alignment.Global = tw.AlignLeft
		paddingConfig := tw.Padding{Left: tw.Empty, Right: "   ", Overwrite: true}
		config.Header.Padding.Global = paddingConfig
		config.Row.Padding.Global = paddingConfig
	})
	fn(table)
	table.Render()
	return buf.String()
}

func renderJSON(v interface{}) string {
	var out []byte
	var err error
	out, err = json.MarshalIndent(v, "", "  ")
	if err != nil {
		return err.Error()
	}
	return string(out)
}

func renderOutput(outputFormat string, v interface{}, fnTable func(*tablewriter.Table), fnQuiet func() string) string {
	out := ""
	switch outputFormat {
	case "json", "j":
		out = renderJSON(v)
	case "quiet", "q":
		out = fnQuiet()
	default:
		out = renderTable(fnTable)
	}

	l := len(out)
	if l > 0 && out[len(out)-1] != '\n' {
		out += "\n"
	}
	return out
}
