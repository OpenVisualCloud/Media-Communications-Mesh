# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh
import logging
import sys
import os
import shutil
from ipaddress import IPv4Interface
from pathlib import Path

from mfd_host import Host
from mfd_connect.exceptions import (
    ConnectionCalledProcessError,
    RemoteProcessTimeoutExpired,
)
import pytest

from common.mtl_manager.mtlManager import MtlManager
from common.nicctl import Nicctl
from Engine.const import (
    ALLOWED_FFMPEG_VERSIONS,
    DEFAULT_FFMPEG_PATH,
    DEFAULT_INPUT_PATH,
    DEFAULT_MCM_FFMPEG_PATH,
    DEFAULT_MCM_FFMPEG_VERSION,
    DEFAULT_MEDIA_PATH,
    DEFAULT_MTL_FFMPEG_PATH,
    DEFAULT_MTL_FFMPEG_VERSION,
    DEFAULT_OPENH264_PATH,
    DEFAULT_OUTPUT_PATH,
    INTEL_BASE_PATH,
    MCM_BUILD_PATH,
    MTL_BUILD_PATH,
    OPENH264_VERSION_TAG,
)
from Engine.mcm_apps import (
    MediaProxy,
    MeshAgent,
    get_mcm_path,
    get_mtl_path,
    get_log_folder_path,
)
from datetime import datetime
import re

logger = logging.getLogger(__name__)


@pytest.fixture(scope="function")
def media_proxy(hosts, mesh_agent, mtl_manager, media_config, log_path):
    """
    Fixture initializes MediaProxy instances for the provided hosts,
    yields them for use in tests, and ensures proper cleanup by stopping the
    media_proxy processes after the test function ends.

    :param hosts: The hosts to be used for initializing the MediaProxy instances.
    :type hosts: dict
    :yield: A dictionary of MediaProxy instances keyed by host name.
    :rtype: dict[str, MediaProxy]
    """
    media_dct = {}
    for host in hosts.values():
        media_params = getattr(host.topology.extra_info, "media_proxy", {})
        if not media_params:
            logger.warning(
                f"Not starting a media_proxy process on {host.name} - no extra_info.media_proxy in topology_config!"
            )
            continue
        if mesh_agent.host is host:
            a_ip = None
            a_port = None
            no_proxy_ip = "localhost"
        else:
            a_ip = mesh_agent.mesh_ip
            a_port = mesh_agent.p
            # Get no_proxy from the host's extra_info
            no_proxy_config = getattr(host.topology.extra_info, "no_proxy", None)

            # Handle no_proxy parameter based on its type
            if no_proxy_config is None:
                # If no_proxy is not specified, use mesh_agent.mesh_ip
                no_proxy_ip = str(mesh_agent.mesh_ip)
            elif isinstance(no_proxy_config, bool) and no_proxy_config:
                # If no_proxy is a boolean True, keep current settings
                no_proxy_ip = str(mesh_agent.mesh_ip)
            elif isinstance(no_proxy_config, str):
                # If no_proxy is a string, pass it as is
                no_proxy_ip = no_proxy_config
            else:
                # For any other case, use mesh_agent.mesh_ip
                no_proxy_ip = str(mesh_agent.mesh_ip)

        media_proxy = MediaProxy(
            host,
            no_proxy=no_proxy_ip,
            t=media_params.get("sdk_port", None),
            a_ip=a_ip,
            a_port=a_port,
            d=getattr(host, "st2110_dev", None),
            i=getattr(host, "st2110_ip", None),
            r=getattr(host, "rdma_ip", None),
            p=media_params.get("rdma_ports", None),
            use_sudo=True,
            log_path=log_path,
        )
        media_dct[host.name] = media_proxy
        media_proxy.start()
    yield media_dct
    for media_proxy in media_dct.values():
        media_proxy.stop()
        if not media_proxy.is_pass:
            raise Exception("MediaProxy process did not pass")


@pytest.fixture(scope="function")
def mtl_manager(hosts, test_config, log_path):
    """
    Fixture initializes MtlManager instances for the provided hosts,
    yields them for use in tests, and ensures proper cleanup by stopping the
    mtl_manager processes after the test function ends.

    :param hosts: The hosts to be used for initializing the MtlManager instances.
    :type hosts: dict
    :param test_config: Test configuration parameters.
    :param log_path: The log directory path from the log_path fixture.
    :type log_path: str
    :yield: A dictionary of MtlManager instances keyed by host name.
    :rtype: dict[str, MtlManager]
    """
    # Force mtl_manager configuration to be present for all hosts
    mtl_managers = {}
    for host in hosts.values():
        # Create a new MtlManager instance for each test with the test-specific log path
        mtl_manager = MtlManager(host, log_path=log_path)
        mtl_managers[host.name] = mtl_manager
        mtl_manager.start()
    yield mtl_managers
    for mtl_manager in mtl_managers.values():
        mtl_manager.stop()


@pytest.fixture(scope="function")
def mesh_agent(hosts, test_config, log_path):
    """
    Fixture for creating and managing a MeshAgent instance.

    This fixture initializes a MeshAgent instance with the provided host,
    yields it for use in tests, and ensures proper cleanup by stopping the
    agent after the test function ends.

    :param hosts: The hosts to be used for initializing the MeshAgent.
    :type hosts: dict
    :param test_config: The test configuration containing parameters for the MeshAgent.
    :param log_path: The log directory path from the log_path fixture.
    :type log_path: str
    :yield: A MeshAgent instance.
    :rtype: MeshAgent
    """
    mesh_agent = MeshAgent(None, log_path=log_path)
    mesh_agent_name = "mesh-agent"

    mesh_agent_name = test_config.get("mesh_agent_name", "mesh-agent")

    if mesh_agent_name not in hosts:
        mesh_ip = test_config.get("mesh_ip", None)

        if not mesh_ip:
            logger.error(
                f"Host '{mesh_agent_name}' not found in topology.yaml and no mesh_ip provided."
            )
            raise RuntimeError(
                f"No mesh-agent name '{mesh_agent_name}' found in hosts and no mesh_ip provided in test_config."
            )
        else:
            logger.info(
                f"Assumed that mesh agent is running, getting IP from topology config: {mesh_ip}"
            )
            mesh_agent.external = True
            mesh_agent.mesh_ip = mesh_ip
            mesh_agent.p = test_config.get("mesh_port", mesh_agent.p)
            mesh_agent.c = test_config.get("control_port", mesh_agent.c)
    else:
        agent = hosts[mesh_agent_name]
        mesh_agent.host = agent
        c, p = (None, None)
        if hasattr(agent.topology.extra_info, "mesh_agent"):
            agent_topology = agent.topology.extra_info.mesh_agent
            c, p = (
                agent_topology.get("control_port", None),
                agent_topology.get("proxy_port", None),
            )
        mesh_agent.start(c, p)
    yield mesh_agent
    mesh_agent.stop()
    if not mesh_agent.external and not mesh_agent.is_pass:
        raise Exception("MeshAgent process did not pass")


@pytest.fixture(scope="session")
def media_path(test_config: dict) -> str:
    """
    Returns the path to the media files used for testing.

    :param test_config: The test configuration dictionary.
    :return: Path to the media files.
    """
    return test_config.get("media_path", DEFAULT_MEDIA_PATH)


@pytest.fixture(scope="session")
def log_path_dir(test_config: dict) -> str:
    """
    Creates and returns the main log directory path for the test session.

    The directory is created under the path provided by get_log_folder_path.
    If keep_logs is False, the existing log directory is removed before creating a new one.

    :param test_config: Dictionary containing test configuration.
    :return: Path to the main log directory for the session.
    :rtype: str
    """
    keep_logs = test_config.get("keep_logs", True)
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    log_dir_name = f"log_{timestamp}"
    log_dir = get_log_folder_path(test_config)

    if log_dir.exists() and not keep_logs:
        shutil.rmtree(log_dir)
    log_dir = Path(log_dir, log_dir_name)
    log_dir.mkdir(parents=True, exist_ok=True)
    return str(log_dir)


def sanitize_name(test_name):
    """
    Sanitizes a test name by replacing invalid characters with underscores,
    collapsing multiple underscores into one, and removing leading/trailing underscores.
    """
    sanitized_name = re.sub(r'[|<>:"*?\r\n\[\]]| = |_{2,}', "_", test_name)
    sanitized_name = re.sub(r"_{2,}", "_", sanitized_name).strip("_")
    return sanitized_name


@pytest.fixture(scope="function")
def log_path(log_path_dir: str, request) -> Path:
    """
    Create a test-specific subdirectory within the main log directory.
    Sanitizes test names to ensure valid directory names by replacing invalid characters.

    :param log_path_dir: The main log directory path from the log_path_dir fixture.
    :type log_path_dir: str
    :param request: Pytest request object to get test name.
    :return: Path to test-specific log subdirectory.
    :rtype: str
    """
    test_name = request.node.name
    sanitized_name = sanitize_name(test_name)
    test_log_path = Path(log_path_dir, sanitized_name)
    test_log_path.mkdir(parents=True, exist_ok=True)
    return Path(test_log_path)


@pytest.fixture(scope="session")
def media_config(hosts: dict) -> None:
    """
    Prepare configuration for media_proxy like PCI addresses, VFs and RDMA IP.
    """
    for ids, host in enumerate(hosts.values()):
        if_idx = 0
        try:
            nicctl = Nicctl(get_mtl_path(host), host)
            last_oct = str(host.connection.ip).split(".")[-1]
            if host.topology.extra_info.media_proxy.get("st2110", False):
                pf_addr = host.network_interfaces[if_idx].pci_address.lspci
                vfs = nicctl.vfio_list(pf_addr)
                host.st2110_dev = host.topology.extra_info.media_proxy.get(
                    "st2110_dev", None
                )
                if not host.st2110_dev and not vfs:
                    nicctl.create_vfs(pf_addr)
                    vfs = nicctl.vfio_list(pf_addr)
                if host.st2110_dev and vfs and host.st2110_dev not in vfs:
                    logger.warning(
                        f"st2110_dev {host.st2110_dev} not found in VFs list of network interface {host.network_interfaces[if_idx].name}."
                    )
                elif vfs:
                    host.st2110_dev = vfs[0]
                else:
                    logger.error(
                        f"Still no VFs on interface {host.network_interfaces[if_idx].pci_address_lspci} even after creating VFs!"
                    )
                host.vfs = vfs
                host.st2110_ip = host.topology.extra_info.media_proxy.get(
                    "st2110_ip", f"192.168.0.{last_oct}"
                )
                if_idx += 1
            if host.topology.extra_info.media_proxy.get("rdma", False):
                if (
                    int(
                        host.network_interfaces[if_idx].virtualization.get_current_vfs()
                    )
                    > 0
                ):
                    nicctl.disable_vf(
                        str(host.network_interfaces[if_idx].pci_address.lspci)
                    )
                net_adap_ips = host.network_interfaces[if_idx].ip.get_ips().v4
                rdma_ip = host.topology.extra_info.media_proxy.get("rdma_ip", False)
                if rdma_ip:
                    rdma_ip = IPv4Interface(
                        f"{rdma_ip}" if "/" in rdma_ip else f"{rdma_ip}/24"
                    )
                elif net_adap_ips and not rdma_ip:
                    rdma_ip = net_adap_ips[0]
                if not rdma_ip or (rdma_ip not in net_adap_ips):
                    rdma_ip = (
                        IPv4Interface(f"192.168.1.{last_oct}/24")
                        if not rdma_ip
                        else rdma_ip
                    )
                    logger.info(
                        f"IP {rdma_ip} not found on RDMA network interface, setting: {rdma_ip}"
                    )
                    host.network_interfaces[if_idx].ip.add_ip(rdma_ip)
                host.rdma_ip = str(rdma_ip.ip)
            logger.info(f"VFs on {host.name} are: {host.vfs}")
        except IndexError:
            raise IndexError(
                f"Not enough network adapters available for tests! Expected: {if_idx+1}"
            )
        except AttributeError:
            logger.warning(
                f"Extra info media proxy in topology config for {host.name} is not set, skipping media config setup for this host."
            )


@pytest.fixture(scope="session", autouse=True)
def cleanup_processes(hosts: dict) -> None:
    """
    Kill mesh-agent, media_proxy, ffmpeg, and all Rx*App/Tx*App processes on all hosts before running tests.
    """
    for host in hosts.values():
        connection = host.connection
        for proc in ["mesh-agent", "media_proxy", "ffmpeg"]:
            try:
                connection.execute_command(f"pgrep {proc}", stderr_to_stdout=True)
                connection.execute_command(f"pkill -9 {proc}", stderr_to_stdout=True)
            except Exception as e:
                # Process not found returns exit code 1, which is expected
                # We only want to log unexpected errors
                is_expected_error = False
                if hasattr(e, "returncode"):
                    # Use getattr to avoid linting errors
                    returncode = getattr(e, "returncode")
                    if returncode == 1:
                        is_expected_error = True

                if not is_expected_error:
                    logger.warning(f"Failed to check/kill {proc} on {host.name}: {e}")
        for pattern in ["^Rx[A-Za-z]+App$", "^Tx[A-Za-z]+App$"]:
            try:
                connection.execute_command(
                    f"pgrep -f '{pattern}'", stderr_to_stdout=True
                )
                connection.execute_command(
                    f"pkill -9 -f '{pattern}'", stderr_to_stdout=True
                )
            except Exception as e:
                # Process not found returns exit code 1, which is expected
                # We only want to log unexpected errors
                is_expected_error = False
                if hasattr(e, "returncode"):
                    # Use getattr to avoid linting errors
                    returncode = getattr(e, "returncode")
                    if returncode == 1:
                        is_expected_error = True

                if not is_expected_error:
                    logger.warning(
                        f"Failed to check/kill processes matching {pattern} on {host.name}: {e}"
                    )
    logger.info("Cleanup of processes completed.")


@pytest.fixture(scope="session")
def build_TestApp(hosts, test_config: dict) -> None:
    if not test_config.get("rx_tx_app_rebuild", False):
        return
    for host in hosts.values():
        mcm_path = get_mcm_path(host)
        path = host.connection.path(mcm_path, "tests", "tools", "TestApp", "build")
        if path.exists():
            path.rmdir()
        path.mkdir(parents=True)
        host.connection.execute_command("cmake ..", cwd=path, shell=True, timeout=10)
        host.connection.execute_command("make", cwd=path, shell=True, timeout=10)


@pytest.fixture(scope="session")
def build_mcm_ffmpeg(hosts, test_config: dict):
    """
    Build and install FFmpeg with the Media Communications Mesh plugin.

    Steps:
    1. Clone and patch FFmpeg (default version 7.0, can be overridden by test_config).
    2. Run FFmpeg configuration tool.
    3. Build and install FFmpeg with the plugin.

    The FFmpeg binaries and libraries will be installed to:
    /opt/intel/_build/ffmpeg-{version}/ffmpeg-{version}_mcm_build/
    """
    if not test_config.get("mcm_ffmpeg_rebuild", False):
        return True

    # Use constants from Engine/const.py
    mcm_ffmpeg_version = test_config.get(
        "mcm_ffmpeg_version", DEFAULT_MCM_FFMPEG_VERSION
    )
    if not mcm_ffmpeg_version in ALLOWED_FFMPEG_VERSIONS:
        logger.error(f"Invalid mcm_ffmpeg_version: {mcm_ffmpeg_version}")
        return False

    for host in hosts.values():
        mcm_path = get_mcm_path(host)
        ffmpeg_tools_path = f"{mcm_path}/ffmpeg-plugin"

        # Ensure the directory structure exists
        build_dir = Path(DEFAULT_MCM_FFMPEG_PATH).parent
        host.connection.execute_command(f"mkdir -p {build_dir}", shell=True, timeout=10)

        # Check and remove mcm_ffmpeg_build_path if it exists
        check_dir = host.connection.execute_command(
            f"[ -d {DEFAULT_MCM_FFMPEG_PATH} ] && echo 'exists' || echo 'not exists'",
            shell=True,
        ).stdout.strip()

        if check_dir == "exists":
            host.connection.execute_command(
                f"rm -rf {DEFAULT_MCM_FFMPEG_PATH}", shell=True
            )

        logger.debug("Step 1: Clone and patch FFmpeg")
        clone_patch_script = f"{ffmpeg_tools_path}/clone-and-patch-ffmpeg.sh"
        cmd = f"bash {clone_patch_script} {mcm_ffmpeg_version}"
        res = host.connection.execute_command(
            cmd, cwd=ffmpeg_tools_path, shell=True, timeout=60, stderr_to_stdout=True
        )
        logger.debug(f"Command {cmd} output: {res.stdout}")
        if res.return_code != 0:
            logger.error(f"Command {cmd} failed with return code {res.return_code}.")
            return False

        logger.debug("Step 2: Run FFmpeg configuration tool")
        configure_script = f"{ffmpeg_tools_path}/configure-ffmpeg.sh"
        cmd = f"{configure_script} {mcm_ffmpeg_version} --prefix={DEFAULT_MCM_FFMPEG_PATH}"
        res = host.connection.execute_command(
            cmd, cwd=ffmpeg_tools_path, shell=True, timeout=60, stderr_to_stdout=True
        )
        logger.debug(f"Command {cmd} output: {res.stdout}")
        if res.return_code != 0:
            logger.error(f"Command {cmd} failed with return code {res.return_code}.")
            return False

        logger.debug("Step 3: Build and install FFmpeg with the plugin")
        build_script = f"{ffmpeg_tools_path}/build-ffmpeg.sh"
        cmd = f"{build_script} {mcm_ffmpeg_version}"
        res = host.connection.execute_command(
            cmd, cwd=ffmpeg_tools_path, shell=True, timeout=120, stderr_to_stdout=True
        )
        logger.debug(f"Command {cmd} output: {res.stdout}")
        if res.return_code != 0:
            logger.error(f"Command {cmd} failed with return code {res.return_code}.")
            return False

        logger.info(
            f"Successfully built MCM FFmpeg {mcm_ffmpeg_version} at {DEFAULT_MCM_FFMPEG_PATH}"
        )
    return True


def build_openh264(host: Host, work_dir: str) -> bool:
    """
    Build and install openh264 library on the specified host.

    :param host: The host where openh264 will be built.
    :type host: Host
    :param work_dir: The working directory for building openh264.
    :type work_dir: str
    :return: True if the build was successful, False otherwise.
    :rtype: bool
    """
    openh264_repo_dir = f"{INTEL_BASE_PATH}/openh264"  # Repository directory
    openh264_install_dir = DEFAULT_OPENH264_PATH  # Installation directory

    # Check if openh264 is already installed
    check_install = host.connection.execute_command(
        f"[ -d {openh264_install_dir}/lib ] && echo 'exists' || echo 'not exists'",
        shell=True,
    ).stdout.strip()

    if check_install == "exists":
        # Check if the library is already in the system
        lib_check = host.connection.execute_command(
            "ldconfig -p | grep -q libopenh264 && echo 'installed' || echo 'not installed'",
            shell=True,
            timeout=10,
        ).stdout.strip()

        if lib_check == "installed":
            logger.info(f"OpenH264 is already installed at {openh264_install_dir}")
            return True

    # Check if repository directory exists
    check_dir = host.connection.execute_command(
        f"[ -d {openh264_repo_dir} ] && echo 'exists' || echo 'not exists'", shell=True
    ).stdout.strip()

    if check_dir == "not exists":
        # Try to clone the repository
        logger.info(
            f"OpenH264 repository not found at {openh264_repo_dir}. Attempting to clone..."
        )
        res = host.connection.execute_command(
            f"git clone https://github.com/cisco/openh264.git {openh264_repo_dir}",
            shell=True,
            timeout=60,
            stderr_to_stdout=True,
        )
        if res.return_code != 0:
            logger.error(f"Failed to clone OpenH264 repository to {openh264_repo_dir}")
            return False

    # Check if we need to checkout the specified version
    tag_check = host.connection.execute_command(
        "git describe --exact-match --tags 2>/dev/null || echo 'wrong tag'",
        cwd=openh264_repo_dir,
        shell=True,
        timeout=10,
    ).stdout.strip()

    if tag_check != OPENH264_VERSION_TAG:
        # Reset any changes and checkout the specific tag
        logger.info(f"Checking out {OPENH264_VERSION_TAG} tag")
        res = host.connection.execute_command(
            f"git reset --hard && git checkout -f {OPENH264_VERSION_TAG}",
            cwd=openh264_repo_dir,
            shell=True,
            timeout=30,
            stderr_to_stdout=True,
        )
        if res.return_code != 0:
            logger.error(f"Failed to checkout {OPENH264_VERSION_TAG} tag.")
            return False
    else:
        logger.info(f"Already on {OPENH264_VERSION_TAG} tag")

    # Build and install to openh264_install_dir
    logger.info(f"Building and installing openh264 to {openh264_install_dir}")

    # Create install directory if it doesn't exist
    host.connection.execute_command(
        f"sudo mkdir -p {openh264_install_dir}", shell=True, timeout=10
    )

    # Build with custom prefix
    res = host.connection.execute_command(
        f"make -j $(nproc) PREFIX={openh264_install_dir} && "
        f"sudo make install PREFIX={openh264_install_dir} && "
        "sudo ldconfig",
        cwd=openh264_repo_dir,
        shell=True,
        timeout=300,
        stderr_to_stdout=True,
    )
    logger.info(f"openh264 build output: {res.stdout}")
    if res.return_code != 0:
        logger.error("Failed to build and install openh264.")
        return False

    return True


@pytest.fixture(scope="session")
def build_mtl_ffmpeg(hosts, test_config: dict):
    """
    Build and install FFmpeg with the Media Transport Library (MTL) plugin.
    """
    if not test_config.get("mtl_ffmpeg_rebuild", False):
        return True

    mtl_ffmpeg_version = test_config.get(
        "mtl_ffmpeg_version", DEFAULT_MTL_FFMPEG_VERSION
    )
    if not mtl_ffmpeg_version in ALLOWED_FFMPEG_VERSIONS:
        logger.error(f"Invalid mtl_ffmpeg_version: {mtl_ffmpeg_version}")
        return False

    enable_gpu_direct = test_config.get("mtl_ffmpeg_gpu_direct", False)

    for host in hosts.values():
        mtl_path = get_mtl_path(host)
        if not mtl_path:
            logger.error(f"MTL path not found on host {host.name}.")
            return False

        # Ensure the directory structure exists
        build_dir = Path(DEFAULT_MTL_FFMPEG_PATH).parent
        host.connection.execute_command(f"mkdir -p {build_dir}", shell=True, timeout=10)

        check_dir = host.connection.execute_command(
            f"[ -d {DEFAULT_MTL_FFMPEG_PATH} ] && echo 'exists' || echo 'not exists'",
            shell=True,
        ).stdout.strip()

        if check_dir == "exists":
            host.connection.execute_command(
                f"rm -rf {DEFAULT_MTL_FFMPEG_PATH}", shell=True
            )

        # Step 1: Build openh264
        logger.info("Step 1: Building openh264 dependency")
        if not build_openh264(host, DEFAULT_OPENH264_PATH):
            return False

        # Step 2: Clone and patch FFmpeg
        logger.info(f"Step 2: Clone and patch FFmpeg {mtl_ffmpeg_version}")
        ffmpeg_dir = DEFAULT_FFMPEG_PATH

        # Make sure the FFmpeg repo directory exists
        check_ffmpeg_dir = host.connection.execute_command(
            f"[ -d {ffmpeg_dir} ] && echo 'exists' || echo 'not exists'",
            shell=True,
        ).stdout.strip()

        if check_ffmpeg_dir != "exists":
            logger.error(f"FFmpeg repository not found at {ffmpeg_dir}")
            return False

        # Clean all previous changes
        host.connection.execute_command(
            f"rm -rf .git/rebase-apply && git clean -fd && git reset --hard origin/release/{mtl_ffmpeg_version}",
            cwd=ffmpeg_dir,
            shell=True,
            timeout=30,
            stderr_to_stdout=True,
        )

        # Apply MTL patches
        res = host.connection.execute_command(
            f"git am {mtl_path}/ecosystem/ffmpeg_plugin/{mtl_ffmpeg_version}/*.patch",
            cwd=ffmpeg_dir,
            shell=True,
            timeout=30,
            stderr_to_stdout=True,
        )
        if res.return_code == 0:
            logger.info("Successfully applied MTL patches to FFmpeg")
        else:
            logger.error(f"Failed to apply MTL patches: {res.stderr}")
            return False

        # Copy MTL implementation files regardless (they might be updated)
        res = host.connection.execute_command(
            f"cp {mtl_path}/ecosystem/ffmpeg_plugin/mtl_*.c -rf libavdevice/ && "
            f"cp {mtl_path}/ecosystem/ffmpeg_plugin/mtl_*.h -rf libavdevice/",
            cwd=ffmpeg_dir,
            shell=True,
            timeout=30,
            stderr_to_stdout=True,
        )
        if res.return_code != 0:
            logger.error("Failed to copy MTL implementation files.")
            return False

        # Step 3: Configure and build FFmpeg with MTL support
        logger.info("Step 3: Configure and build FFmpeg with MTL support")

        # Base configure options
        configure_options = [
            f"--prefix={DEFAULT_MTL_FFMPEG_PATH}",
            "--enable-shared",
            "--disable-static",
            "--enable-nonfree",
            "--enable-pic",
            "--enable-gpl",
            "--enable-libopenh264",
            "--enable-encoder=libopenh264",
            "--enable-mtl",
            f'--extra-ldflags="-L{DEFAULT_OPENH264_PATH}/lib"',
            f'--extra-cflags="-I{DEFAULT_OPENH264_PATH}/include -I{mtl_path}/include"',
        ]

        # Add GPU direct support only if explicitly enabled and headers exist
        if enable_gpu_direct:
            # Check both possible GPU header locations
            gpu_header_paths = [
                f"{mtl_path}/include/mtl_gpu_direct",
                f"{mtl_path}/include/gpu",
            ]

            gpu_headers_found = False
            for gpu_path in gpu_header_paths:
                try:
                    res = host.connection.execute_command(
                        f"test -d {gpu_path}",
                        shell=True,
                        stderr_to_stdout=True,
                        expected_return_codes={0, 1},  # Allow both success and failure
                    )
                    if res.return_code == 0:
                        logger.info(f"Found GPU headers at {gpu_path}")
                        configure_options.append(
                            f'--extra-cflags="-DMTL_GPU_DIRECT_ENABLED -I{gpu_path}"'
                        )
                        gpu_headers_found = True
                        break
                except Exception as e:
                    logger.warning(f"Error checking GPU headers at {gpu_path}: {e}")
                    continue

            if not gpu_headers_found:
                logger.warning(
                    "GPU direct support requested but headers not found. Continuing without GPU support."
                )
                enable_gpu_direct = False

        # Join configure options and continue with build
        configure_command = "./configure " + " ".join(configure_options)

        # Run configure
        res = host.connection.execute_command(
            configure_command,
            cwd=DEFAULT_FFMPEG_PATH,
            shell=True,
            timeout=120,
            stderr_to_stdout=True,
        )
        if res.return_code != 0:
            logger.error("Failed to configure FFmpeg with MTL support.")
            return False

        # Build and install with better error handling
        build_commands = [
            "make clean",
            f"make -j$(nproc)",
            "sudo make install",
            "sudo ldconfig",
        ]

        for cmd in build_commands:
            res = host.connection.execute_command(
                cmd,
                cwd=DEFAULT_FFMPEG_PATH,
                shell=True,
                timeout=300,
                stderr_to_stdout=True,
            )
            if res.return_code != 0:
                logger.error(f"Failed to execute '{cmd}'")
                return False

        logger.info(f"Successfully built FFmpeg {mtl_ffmpeg_version} with MTL support")

    return True


@pytest.fixture(scope="session", autouse=True)
def check_iommu(hosts: dict[str, Host]) -> None:
    """
    Check if IOMMU is enabled on the hosts.
    """
    iommu_not_enabled_hosts = []
    for host in hosts.values():
        try:
            output = host.connection.execute_command(
                "ls -1 /sys/kernel/iommu_groups | wc -l", shell=True, timeout=10
            )
            if int(output.stdout.strip()) == 0:
                logger.error(f"IOMMU is not enabled on host {host.name}.")
                iommu_not_enabled_hosts.append(host.name)
        except Exception as e:
            logger.exception(f"Failed to check IOMMU status on host {host.name}.")
            iommu_not_enabled_hosts.append(host.name)
    if iommu_not_enabled_hosts:
        pytest.exit(
            f"IOMMU is not enabled on hosts: {', '.join(iommu_not_enabled_hosts)}. Aborting test session."
        )
    else:
        logger.info("IOMMU is enabled on all hosts.")


@pytest.fixture(scope="session", autouse=True)
def enable_hugepages(hosts: dict[str, Host]) -> None:
    """
    Enable hugepages on the hosts if they are not already enabled.
    """
    for host in hosts.values():
        if not _check_hugepages(host):
            try:
                host.connection.execute_command(
                    "sudo sysctl -w vm.nr_hugepages=2048", shell=True, timeout=10
                )
                logger.info(f"Hugepages enabled on host {host.name}.")
            except (RemoteProcessTimeoutExpired, ConnectionCalledProcessError):
                logger.exception(f"Failed to enable hugepages on host {host.name}.")
                pytest.exit(
                    f"Failed to enable hugepages on host {host.name}. Aborting test session."
                )
            if not _check_hugepages(host):
                logger.error(f"Hugepages could not be enabled on host {host.name}.")
                pytest.exit(
                    f"Hugepages could not be enabled on host {host.name}. Aborting test session."
                )
        else:
            logger.info(f"Hugepages are already enabled on host {host.name}.")


def _check_hugepages(host: Host) -> bool:
    """
    Check if hugepages are enabled on the host.
    """
    try:
        output = host.connection.execute_command(
            "cat /proc/sys/vm/nr_hugepages", shell=True, timeout=10
        )
        return int(output.stdout.strip()) > 0
    except (RemoteProcessTimeoutExpired, ConnectionCalledProcessError):
        logger.exception(f"Failed to check hugepages status on host {host.name}.")
        return False


@pytest.fixture(scope="session", autouse=True)
def log_interface_driver_info(hosts: dict[str, Host]) -> None:
    """
    Log driver information for each network interface on the hosts.
    """
    for host in hosts.values():
        for interface in host.network_interfaces:
            driver_info = interface.driver.get_driver_info()
            logger.info(
                f"Interface {interface.name} on host {host.name} uses driver: {driver_info.driver_name} ({driver_info.driver_version})"
            )
