# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh
import logging
import os
import subprocess
import time

from .execute import log_fail, call

import pytest


@pytest.fixture(scope="function", autouse=False)
def test_type_setter(request) -> None:
    test_type = request.param
    os.environ['MCM_EXPECTED_TEST_TYPE'] = test_type
    os.environ['MCM_CURRENT_TEST_TYPE'] = ""
    logging.debug(f"/start/ MCM_EXPECTED_TEST_TYPE={os.environ['MCM_EXPECTED_TEST_TYPE']}")
    logging.debug(f"/start/ MCM_CURRENT_TEST_TYPE={os.environ['MCM_CURRENT_TEST_TYPE']}")
    yield


@pytest.fixture(scope="function", autouse=False)
def test_type_checker(test_type: str = "memif") -> bool:
    yield
    logging.debug(f"/end/ MCM_EXPECTED_TEST_TYPE={os.environ['MCM_EXPECTED_TEST_TYPE']}")
    logging.debug(f"/end/ MCM_CURRENT_TEST_TYPE={os.environ['MCM_CURRENT_TEST_TYPE']}")
    if os.environ['MCM_EXPECTED_TEST_TYPE'] == os.environ['MCM_CURRENT_TEST_TYPE']:
        logging.debug("Current test type matches expectations")
    else:
        log_fail("Wrong test type detected")


@pytest.fixture(scope="session")
def build_TestApp(build: str) -> None:
    path = os.path.join(build, "tests", "tools", "TestApp", "build")
    subprocess.run(f'rm -rf "{path}"', shell=True, timeout=10)
    subprocess.run(f'mkdir -p "{path}"', shell=True, timeout=10)
    subprocess.run("cmake ..", cwd=path, shell=True, timeout=10)
    subprocess.run("make", cwd=path, shell=True, timeout=10)


def kill_all_existing_media_proxies() -> None:
    "Kills all existing media_proxy processes using their PIDs found with px -aux"
    existing_mps = subprocess.run("ps -aux | awk '/media_proxy/{print($2)}'", shell=True, capture_output=True)
    mp_pids = existing_mps.stdout.decode("utf-8").split()  # returns an array of PIDs
    (subprocess.run(f"kill -9 {mp_pid}", shell=True) for mp_pid in mp_pids)


@pytest.fixture(scope="function", autouse=False)
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
    mesh_agent_proc = call(f"mesh-agent", cwd=".")
    time.sleep(0.2) # short sleep used for mesh-agent to spin up
    if mesh_agent_proc.process.returncode:
        logging.debug(f"mesh-agent's return code: {mesh_agent_proc.returncode} of type {type(mesh_agent_proc.returncode)}")
    # single media_proxy start
    # TODO: Add parameters to media_proxy
    sender_mp_proc = call(f"media_proxy", cwd=".")
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


# Run dual media proxy
@pytest.fixture(scope="function", autouse=False)
def media_proxy_cluster(
    tx_mp_port: int = 8002,
    rx_mp_port: int = 8003,
    tx_rdma_ip: str = "192.168.95.1",
    rx_rdma_ip: str = "192.168.95.2",
    tx_rdma_port_range: str = "9100-9119",
    rx_rdma_port_range: str = "9120-9139",
    ) -> None:
    # kill all existing media proxies first
    kill_existing = True # TODO: Make it parametrized
    if kill_existing:
        kill_all_existing_media_proxies()

    # start mesh-agent
    mesh_agent_proc = call(f"mesh-agent", cwd=".")
    time.sleep(0.2) # short sleep used for mesh-agent to spin up
    if mesh_agent_proc.process.returncode:
        logging.debug(f"mesh-agent return code: {mesh_agent_proc.returncode} of type {type(mesh_agent_proc.returncode)}")

    # start sender media_proxy
    sender_mp_proc = call(f"media_proxy -t {tx_mp_port} -r {tx_rdma_ip} -p {tx_rdma_port_range}", cwd=".")
    time.sleep(0.2) # short sleep used for media_proxy to spin up
    if sender_mp_proc.process.returncode:
        logging.debug(f"sender media_proxy return code: {sender_mp_proc.returncode} of type {type(sender_mp_proc.returncode)}")

    # start receiver media_proxy
    receiver_mp_proc = call(f"media_proxy -t {rx_mp_port} -r {rx_rdma_ip} -p {rx_rdma_port_range}", cwd=".")
    time.sleep(0.2) # short sleep used for media_proxy to spin up
    if receiver_mp_proc.process.returncode:
        logging.debug(f"receiver media_proxy return code: {receiver_mp_proc.returncode} of type {type(receiver_mp_proc.returncode)}")

    yield

    # stop sender media_proxy
    sender_mp_proc.process.terminate()
    if not sender_mp_proc.process.returncode:
        logging.debug(f"sender media_proxy terminated properly")

    # stop receiver media_proxy
    receiver_mp_proc.process.terminate()
    if not receiver_mp_proc.process.returncode:
        logging.debug(f"receiver media_proxy terminated properly")

    time.sleep(2) # allow media_proxy to terminate properly, before terminating mesh-agent

    # stop mesh-agent
    mesh_agent_proc.process.terminate()
    if not mesh_agent_proc.process.returncode:
        logging.debug(f"mesh-agent terminated properly")
