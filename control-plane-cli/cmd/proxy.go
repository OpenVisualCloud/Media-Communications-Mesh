/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package cmd

import (
	"control-plane-cli/api/pulse"
	"fmt"

	"github.com/spf13/cobra"
)

// var agent string
var quietOutput bool

func getOutputFormat() string {
	if quietOutput {
		return "quiet"
	}
	if compactOutput {
		return "table-compact"
	}
	return ""
}

const OutputFormatJson = "json"

var proxyCmd = &cobra.Command{
	Use:   "proxy",
	Short: "manage media proxies",
	Long:  "Manage media proxies",
}

var proxyLsCmd = &cobra.Command{
	Use:   "ls",
	Short: "list proxies",
	Long:  "List proxies",
	RunE: func(cmd *cobra.Command, args []string) error {
		resp, err := pulse.GetProxyList(cmd.Context(), nil, getOutputFormat())
		if err != nil {
			return err
		}
		fmt.Print(resp)
		return nil
	},
}

var proxyInspectCmd = &cobra.Command{
	Use:   "inspect PROXY [PROXY...]",
	Short: "inspect a proxy",
	Long:  "Inspect a proxy",
	Args:  cobra.MinimumNArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		resp, err := pulse.GetProxyList(cmd.Context(), args, OutputFormatJson)
		if err != nil {
			return err
		}
		fmt.Print(resp)
		return nil
	},
}

func defineFlagQuiet(cmd *cobra.Command) {
	cmd.Flags().BoolVarP(&quietOutput, "quiet", "q", false, "quiet output, only display IDs")
}

func init() {
	defineFlagQuiet(proxyLsCmd)
	proxyCmd.AddCommand(proxyLsCmd, proxyInspectCmd)
	rootCmd.AddCommand(proxyCmd)
}
