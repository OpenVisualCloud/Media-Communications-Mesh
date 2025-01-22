# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh

pytest_plugins = ["Engine.fixtures", "Engine.logging", "Engine.fixtures_mcm"]


def pytest_addoption(parser):
    parser.addoption("--keep", help="keep result media files: all, failed, none (default)")
    parser.addoption("--dmesg", help="method of dmesg gathering: clear (dmesg -C), keep (default)")
    parser.addoption("--media", help="path to media asset (default /mnt/media)")
    parser.addoption("--build", help="path to build (default ../..)")
    parser.addoption("--nic", help="list of PCI IDs of network devices")
    parser.addoption("--dma", help="list of PCI IDs of DMA devices")
    parser.addoption("--time", help="seconds to run every test (default=15)")
