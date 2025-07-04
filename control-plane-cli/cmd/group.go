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

var groupCmd = &cobra.Command{
	Use:   "group",
	Short: "manage multipoint groups",
	Long:  "Manage multipoint groups",
}

var groupLsCmd = &cobra.Command{
	Use:   "ls",
	Short: "list multipoint groups",
	Long:  "List multipoint groups",
	RunE: func(cmd *cobra.Command, args []string) error {
		resp, err := pulse.GetMultipointGroupList(cmd.Context(), nil, getOutputFormat())
		if err != nil {
			return err
		}
		fmt.Print(resp)
		return nil
	},
}

var groupInspectCmd = &cobra.Command{
	Use:   "inspect",
	Short: "inspect a multipoint group",
	Long:  "Inspect a multipoint group",
	Args:  cobra.MinimumNArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		resp, err := pulse.GetMultipointGroupList(cmd.Context(), args, OutputFormatJson)
		if err != nil {
			return err
		}
		fmt.Print(resp)
		return nil
	},
}

func init() {
	defineFlagQuiet(groupLsCmd)
	groupCmd.AddCommand(groupLsCmd, groupInspectCmd)
	rootCmd.AddCommand(groupCmd)
}
