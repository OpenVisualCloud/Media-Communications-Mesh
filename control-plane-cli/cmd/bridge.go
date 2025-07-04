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

var bridgeCmd = &cobra.Command{
	Use:   "bridge",
	Short: "manage bridges",
	Long:  "Manage bridges",
}

var bridgeLsCmd = &cobra.Command{
	Use:   "ls",
	Short: "list bridges",
	Long:  "List bridges",
	RunE: func(cmd *cobra.Command, args []string) error {
		resp, err := pulse.GetBridgeList(cmd.Context(), nil, getOutputFormat())
		if err != nil {
			return err
		}
		fmt.Print(resp)
		return nil
	},
}

var bridgeInspectCmd = &cobra.Command{
	Use:   "inspect",
	Short: "inspect a bridge",
	Long:  "Inspect a bridge",
	Args:  cobra.MinimumNArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		resp, err := pulse.GetBridgeList(cmd.Context(), args, OutputFormatJson)
		if err != nil {
			return err
		}
		fmt.Print(resp)
		return nil
	},
}

func init() {
	defineFlagQuiet(bridgeLsCmd)
	defineFlagCompact(bridgeLsCmd)
	bridgeCmd.AddCommand(bridgeLsCmd, bridgeInspectCmd)
	rootCmd.AddCommand(bridgeCmd)
}
