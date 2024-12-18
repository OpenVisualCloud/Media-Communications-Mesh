# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024 Intel Corporation
# Intel® Media Communications Mesh
import os
import subprocess

import pytest


@pytest.fixture(scope="session")
def build_TestApp(build):
    path = os.path.join(build, "tests", "tools", "TestApp", "build")
    subprocess.run(f"rm -rf {path}", shell=True, timeout=10)
    subprocess.run(f"mkdir -p {path}", shell=True, timeout=10)
    subprocess.run("cmake ..", cwd=path, shell=True, timeout=10)
    subprocess.run("make", cwd=path, shell=True, timeout=10)


def kill_all_existing_media_proxies():
    "Kills all existing media_proxy processes using their PIDs found with px -aux"
    existing_mps = subprocess.run("ps -aux | awk '/media_proxy/{print($2)}'", shell=True, capture_output=True)
    mp_pids = existing_mps.stdout.decode("utf-8").split()  # returns an array of PIDs
    (subprocess.run(f"kill -9 {mp_pid}", shell=True) for mp_pid in mp_pids)


@pytest.fixture(scope="package", autouse=False)
def media_proxy_single(sender_mp_port: int, receiver_mp_port: int, kill_existing: bool = True):
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
    # create new media_proxy processes for sender and receiver
    sender_mp_proc = subprocess.Popen(f"media_proxy -t {sender_mp_port}")  # sender's media_proxy
    receiver_mp_proc = subprocess.Popen(f"media_proxy -t {receiver_mp_port}")  # receiver media_proxy
    yield
    sender_mp_proc.terminate()
    receiver_mp_proc.terminate()


@pytest.fixture(scope="package")
def media_proxy_dual():
    # Run dual media proxy
    pass
