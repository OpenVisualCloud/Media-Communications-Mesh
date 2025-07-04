/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package cmd

import (
	"fmt"

	"github.com/spf13/cobra"
)

var agent string

var configureCmd = &cobra.Command{
	Use:   "configure",
	Short: "configure",
	Long:  "Configure",
	Run: func(cmd *cobra.Command, args []string) {
		fmt.Println("Configure!", agent)
	},
}

func init() {
	configureCmd.Flags().StringVarP(&agent, "agent", "a", "", "Specify the agent")
	rootCmd.AddCommand(configureCmd)
}
