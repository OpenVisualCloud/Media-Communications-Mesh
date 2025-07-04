/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

package main

import (
	"context"
	"control-plane-cli/api/pulse"
	"control-plane-cli/cmd"
	"log"
	"os"
	"os/signal"
	"syscall"
)

func main() {
	ctx, cancel := context.WithCancel(context.Background())

	sigs := make(chan os.Signal, 1)
	signal.Notify(sigs, os.Interrupt, syscall.SIGTERM)
	go func() {
		select {
		case <-ctx.Done():
		case <-sigs:
			cancel()
		}
	}()

	err := pulse.InitClient()
	if err != nil {
		log.Fatal(err)
	}

	err = cmd.Execute(ctx)

	cancel()
	pulse.CloseClient()

	if err != nil {
		os.Exit(1)
	}
}
