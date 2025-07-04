/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package cmd

import (
	"context"
	"fmt"

	"github.com/spf13/cobra"
)

// var outputFormat string

var rootCmd = &cobra.Command{
	Use: "mesh",
	Run: func(cmd *cobra.Command, args []string) {
		// Default action if no subcommand is given
		fmt.Println("Welcome to YourApp CLI!")
	},
}

// Execute runs the root command
func Execute(ctx context.Context) error {
	return rootCmd.ExecuteContext(ctx)
}

func init() {
	// You can add global (persistent) flags here, e.g.:
	// rootCmd.PersistentFlags().StringP("output", "o", "", "Output type (default, json)")
	// rootCmd.PersistentFlags().StringVarP(&outputFormat, "output", "o", "", "Specify the output format (default, json, quiet, etc.)")
	// rootCmd.PersistentFlags().BoolVarP(&quietOutput, "quiet", "q", false, "Quiet output, only display IDs")
}
