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
var compactOutput bool

var connectionCmd = &cobra.Command{
	Use:   "connection",
	Short: "manage connections",
	Long:  "Manage connections",
}

var connectionLsCmd = &cobra.Command{
	Use:   "ls",
	Short: "list connections",
	Long:  "List connections",
	RunE: func(cmd *cobra.Command, args []string) error {
		resp, err := pulse.GetConnList(cmd.Context(), nil, getOutputFormat())
		if err != nil {
			return err
		}
		fmt.Print(resp)
		return nil
	},
}

var connectionInspectCmd = &cobra.Command{
	Use:   "inspect",
	Short: "inspect a connection",
	Long:  "Inspect a connection",
	Args:  cobra.MinimumNArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		resp, err := pulse.GetConnList(cmd.Context(), args, OutputFormatJson)
		if err != nil {
			return err
		}
		fmt.Print(resp)
		return nil
	},
}

func defineFlagCompact(cmd *cobra.Command) {
	cmd.Flags().BoolVarP(&compactOutput, "compact", "c", false, "compact output, remove redundant details")
}

func init() {
	defineFlagQuiet(connectionLsCmd)
	defineFlagCompact(connectionLsCmd)
	connectionCmd.AddCommand(connectionLsCmd, connectionInspectCmd)
	rootCmd.AddCommand(connectionCmd)
}
