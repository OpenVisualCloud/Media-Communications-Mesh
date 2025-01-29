# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh
import logging
import os
import subprocess
import time

import pytest


@pytest.fixture(scope="session")
def build_TestApp(build):
    path = os.path.join(build, "tests", "tools", "TestApp", "build")
    subprocess.run(f'rm -rf "{path}"', shell=True, timeout=10)
    subprocess.run(f'mkdir -p "{path}"', shell=True, timeout=10)
    subprocess.run("cmake ..", cwd=path, shell=True, timeout=10)
    subprocess.run("make", cwd=path, shell=True, timeout=10)


def kill_all_existing_media_proxies():
    # TODO: This assumes the way previous media_proxy worked will not change in the new version, which is unlikely
    "Kills all existing media_proxy processes using their PIDs found with px -aux"
    existing_mps = subprocess.run("ps -aux | awk '/media_proxy/{print($2)}'", shell=True, capture_output=True)
    mp_pids = existing_mps.stdout.decode("utf-8").split()  # returns an array of PIDs
    (subprocess.run(f"kill -9 {mp_pid}", shell=True) for mp_pid in mp_pids)


@pytest.fixture(scope="package", autouse=True)
def media_proxy_single():
    kill_existing = True
    # TODO: This assumes the way previous media_proxy worked will not change in the new version, which is unlikely
    # TODO: Re-add the parameters properly
    """Opens new media_proxies for sender and receiver.
    May optionally kill the already-running media_proxies first.

    Arguments:
        sender_mp_port(int): specifies the port number for sender
        receiver_mp_port(int): specifies the port number for receiver

    Keyword arguments:
        kill_existing(bool, optional): if use kill_all_existing_media_proxies() function to kill -9 all existing
                                       media_proxies before running new instances
    """
    if kill_existing:
        kill_all_existing_media_proxies()

    # mesh-agent start
    mesh_agent_proc = subprocess.Popen("mesh-agent")
    time.sleep(0.2)
    if mesh_agent_proc.returncode:
        logging.debug(f"mesh-agent's return code: {mesh_agent_proc.returncode} of type {type(mesh_agent_proc.returncode)}")
    # single media_proxy start
    # TODO: Add parameters to media_proxy
    sender_mp_proc = subprocess.Popen("media_proxy")
    time.sleep(0.2)
    if sender_mp_proc.returncode:
        logging.debug(f"media_proxy's return code: {sender_mp_proc.returncode} of type {type(sender_mp_proc.returncode)}")

    yield

    sender_mp_proc.terminate()
    if not sender_mp_proc.returncode:
        logging.debug(f"media_proxy terminated properly")
    time.sleep(2)
    mesh_agent_proc.terminate()
    if not mesh_agent_proc.returncode:
        logging.debug(f"mesh-agent terminated properly")


@pytest.fixture(scope="package")
def media_proxy_dual():
    # Run dual media proxy
    pass
