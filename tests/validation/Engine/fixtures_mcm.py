# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh
import logging
import os
import subprocess
import time

import Engine.execute

import pytest


@pytest.fixture(scope="session")
def build_TestApp(hosts, build: str) -> None:
    path = os.path.join(build, "tests", "tools", "TestApp", "build")
    for host in hosts:
        host.execute(f'rm -rf "{path}"', shell=True, timeout=10)
        host.execute(f'mkdir -p "{path}"', shell=True, timeout=10)
        host.execute("cmake ..", cwd=path, shell=True, timeout=10)
        host.execute("make", cwd=path, shell=True, timeout=10)


def kill_all_existing_media_proxies() -> None:
    # TODO: This assumes the way previous media_proxy worked will not change in the new version, which is unlikely
    "Kills all existing media_proxy processes using their PIDs found with px -aux"
    existing_mps = subprocess.run("ps -aux | awk '/media_proxy/{print($2)}'", shell=True, capture_output=True)
    mp_pids = existing_mps.stdout.decode("utf-8").split()  # returns an array of PIDs
    (subprocess.run(f"kill -9 {mp_pid}", shell=True) for mp_pid in mp_pids)


@pytest.fixture(scope="function", autouse=True)
def media_proxy_single() -> None:
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
    mesh_agent_proc = Engine.execute.call(f"mesh-agent", cwd=".")
    time.sleep(0.2) # short sleep used for mesh-agent to spin up
    if mesh_agent_proc.process.returncode:
        logging.debug(f"mesh-agent's return code: {mesh_agent_proc.returncode} of type {type(mesh_agent_proc.returncode)}")
    # single media_proxy start
    # TODO: Add parameters to media_proxy
    sender_mp_proc = Engine.execute.call(f"media_proxy", cwd=".")
    time.sleep(0.2) # short sleep used for media_proxy to spin up
    if sender_mp_proc.process.returncode:
        logging.debug(f"media_proxy's return code: {sender_mp_proc.returncode} of type {type(sender_mp_proc.returncode)}")

    yield

    sender_mp_proc.process.terminate()
    if not sender_mp_proc.process.returncode:
        logging.debug(f"media_proxy terminated properly")
    time.sleep(2) # allow media_proxy to terminate properly, before terminating mesh-agent
    mesh_agent_proc.process.terminate()
    if not mesh_agent_proc.process.returncode:
        logging.debug(f"mesh-agent terminated properly")


@pytest.fixture(scope="package")
def media_proxy_dual() -> None:
    # Run dual media proxy
    pass
