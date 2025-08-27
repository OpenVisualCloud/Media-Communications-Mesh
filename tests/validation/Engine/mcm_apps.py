import logging
import re
import time
import threading
from pathlib import Path

from Engine.const import (
    LOG_FOLDER,
    MCM_PATH,
    MEDIA_PROXY_ERROR_KEYWORDS,
    MEDIA_PROXY_PORT,
    MESH_AGENT_ERROR_KEYWORDS,
    MTL_PATH,
    STOP_GRACEFULLY_PERIOD,
)

logger = logging.getLogger(__name__)


def get_mtl_path(host) -> str:
    """
    Returns the path to the Media Transport Library (MTL) on the given host.
    If the host has a custom MTL path set in its extra_info, it will return that.
    Otherwise, it returns the default MTL path.
    """
    return getattr(host.topology.extra_info, "mtl_path", MTL_PATH)


def get_mcm_path(host) -> str:
    """
    Returns the path to the Media Communications Mesh (MCM) on the given host.
    If the host has a custom MCM path set in its extra_info, it will return that.
    Otherwise, it returns the default MCM path.
    """
    return getattr(host.topology.extra_info, "mcm_path", MCM_PATH)


def get_media_proxy_port(host) -> int:
    """
    Returns the media proxy port for the given host.
    If the host has a custom media proxy port set in its extra_info, it will return that.
    Otherwise, it returns the default media proxy port.
    """
    media_params = getattr(host.topology.extra_info, "media_proxy", {})
    return media_params.get("sdk_port", MEDIA_PROXY_PORT)


def remove_ansi_escape_sequences(text):
    # Regular expression pattern to match ANSI escape sequences
    ansi_escape_pattern = r"\x1b\[[0-9;]*m"
    # Use re.sub to replace ANSI escape sequences with an empty string
    cleaned_text = re.sub(ansi_escape_pattern, "", text)
    return cleaned_text


def save_process_log(
    subdir: str,
    filename: str,
    text: str,
    cmd: str | None = None,
    log_dir: str | Path = LOG_FOLDER,
):
    log_dir = Path(log_dir)
    log_path = Path(log_dir, subdir)
    log_path.mkdir(parents=True, exist_ok=True)
    log_file = Path(log_path, filename)
    cleaned_text = remove_ansi_escape_sequences(text)
    write_cmd = False
    if cmd:
        if not log_file.exists() or log_file.stat().st_size == 0:
            write_cmd = True
    with open(log_file, "a") as f:
        if write_cmd:
            f.write(cmd + "\n\n")
        f.write(cleaned_text + "\n")


def output_validator(
    output: str | None = None,
    error_keywords=None,
    phrase_checks=None,
    log_file_path: str | None = None,
) -> dict:
    """
    Parses process output for error keywords and phrase-value checks.
    Can read output from a string or directly from a log file.

    Args:
        output (str): The process output text.
        error_keywords (list[str], optional): List of keywords to search for errors.
        phrase_checks (dict[str, str], optional): Dict of {phrase: expected_value}.
        log_file_path (str, optional): Path to the log file to read output from.

    Returns:
        dict: {
            "is_pass": bool, True if no errors or mismatches found,
            "errors": list of lines with error keywords,
            "phrase_mismatches": list of (phrase, found_value, expected_value)
        }
    """
    if error_keywords is None:
        error_keywords = ["error", "failed", "exception", "fatal"]
    if phrase_checks is None:
        phrase_checks = {}

    # Read output from file if log_file_path is provided
    if log_file_path:
        with open(log_file_path, "r") as f:
            output = f.read()

    errors = []
    phrase_mismatches = []

    for line in output.splitlines():
        if any(kw.lower() in line.lower() for kw in error_keywords):
            errors.append(line)

    for phrase, expected in phrase_checks.items():
        match = re.search(rf"{re.escape(phrase)}\s*=\s*(\S+)", output)
        if match:
            found_value = match.group(1)
            if found_value != expected:
                phrase_mismatches.append((phrase, found_value, expected))
    is_pass = True
    if errors or phrase_mismatches:
        is_pass = False
        raise RuntimeError(
            f"Output validation failed: {len(errors)} errors found, "
            f"{len(phrase_mismatches)} phrase mismatches found."
        )

    result = {
        "is_pass": is_pass,
        "errors": errors,
        "phrase_mismatches": phrase_mismatches,
    }

    is_pass = not (errors or phrase_mismatches)

    result = {
        "is_pass": is_pass,
        "errors": errors,
        "phrase_mismatches": phrase_mismatches,
    }

    return result


class MediaProxy:
    """
    Class is responsible for starting and stopping the media_proxy process
    on a given host. It interacts with the host's connection to execute commands
    and manage the process.

    Attributes:
        host: A host object, containing connection details.
        t: Port number for SDK API server (default: 8002).
        a: MCM Agent Proxy API address in the format host:port (default: localhost:50051).
        d: PCI device port for SMPTE 2110 media data transportation (default: 0000:31:00.0).
        i: IP address for SMPTE 2110 (default: 192.168.96.1).
        r: IP address for RDMA (default: 192.168.96.2).
        p: Local port ranges for incoming RDMA connections (default: 9100-9999).
        use_sudo: Whether to use sudo for starting the process (default: True).
        run_media_proxy_process: The process object representing the running media_proxy process.

    Methods:
        start():
            Starts the media_proxy process with the specified configuration and
            port parameters. Logs the status of the process.
        stop():
            Stops the running media_proxy process and logs its output.
    """

    def __init__(
        self,
        host,
        no_proxy=None,
        t=None,
        a_ip=None,
        a_port=None,
        d=None,
        i=None,
        r=None,
        p=None,
        use_sudo=True,
        log_path=None,
    ):
        self.host = host
        self.no_proxy = no_proxy
        self.t = t
        self.a_ip = a_ip
        self.a_port = a_port
        self.d = d
        self.i = i
        self.r = r
        self.p = p
        self.use_sudo = use_sudo
        self.run_media_proxy_process = None
        self.cmd = None
        self.is_pass = False
        self.log_path = log_path
        self.subdir = Path("media_proxy_logs", self.host.name)
        self.filename = "media_proxy.log"
        self.log_file_path = Path(
            log_path if log_path else LOG_FOLDER, self.subdir, self.filename
        )

    def start(self):
        if not self.run_media_proxy_process or not self.run_media_proxy_process.running:
            connection = self.host.connection
            # if self.use_sudo:
            #     connection.enable_sudo()

            cmd = "media_proxy"
            if self.no_proxy:
                cmd = f"no_proxy={self.no_proxy} {cmd}"
            if self.t:
                cmd += f" -t {self.t}"
            if self.a_ip:
                cmd += f" -a {self.a_ip}:{self.a_port if self.a_port else 50051}"
            if self.d:
                cmd += f" -d {self.d}"
            if self.i:
                cmd += f" -i {self.i}"
            if self.r:
                cmd += f" -r {self.r}"
            if self.p:
                cmd += f" -p {self.p}"

            self.cmd = cmd
            logger.info(
                f"Starting media proxy on host {self.host.name} with command: {self.cmd}"
            )
            try:
                logger.debug(f"Media proxy command on {self.host.name}: {self.cmd}")
                self.run_media_proxy_process = connection.start_process(
                    self.cmd, stderr_to_stdout=True
                )

                # Start background logging thread
                def log_output():
                    for line in self.run_media_proxy_process.get_stdout_iter():
                        save_process_log(
                            subdir=self.subdir,
                            filename=self.filename,
                            text=line.rstrip(),
                            cmd=cmd,
                            log_dir=self.log_path,
                        )

                threading.Thread(target=log_output, daemon=True).start()
            except Exception as e:
                logger.error(
                    f"Failed to start media proxy process on host {self.host.name}"
                )
                raise RuntimeError(
                    f"Failed to start media proxy process on host {self.host.name}: {e}"
                )
            # if self.use_sudo:
            #     connection.disable_sudo()
            logger.info(
                f"Media proxy started on host {self.host.name}: {self.run_media_proxy_process.running}"
            )

    def stop(self, log_path=None):
        # Use the provided log_path or the one stored in the object
        log_path = log_path if log_path is not None else self.log_path

        if self.run_media_proxy_process and self.run_media_proxy_process.running:
            self.run_media_proxy_process.stop()
            time.sleep(STOP_GRACEFULLY_PERIOD)  # Give some time for the process to stop
            if self.run_media_proxy_process.running:
                self.run_media_proxy_process.kill()
                logger.error(
                    f"Media proxy process on host {self.host.name} did not exit gracefully and had to be killed."
                )
            logger.info(f"Media proxy stopped on host {self.host.name}")

            result = output_validator(
                log_file_path=self.log_file_path,
                error_keywords=MEDIA_PROXY_ERROR_KEYWORDS,
            )
            if result["errors"]:
                logger.error(f"Errors found: {result['errors']}")

            self.is_pass = result["is_pass"]

            self.run_media_proxy_process = None

            if not self.is_pass:
                logger.error(f"Media proxy process on host {self.host.name} failed.")


class MeshAgent:
    """
    Represents a Mesh Agent that manages the lifecycle of a mesh-agent process.

    This class is responsible for starting and stopping the mesh-agent process
    on a given host. It interacts with the host's connection to execute commands
    and manage the process.

    Attributes:
        host: A host object, containing connection details.
        mesh_agent_process (Process): The process object representing the
            running mesh-agent process.
        c: Control API port number (default 8100)
        p: Proxy API port number (default 50051)

    Methods:
        start():
            Starts the mesh-agent process with the specified configuration and
            port parameters. Logs the status of the process.
        stop():
            Stops the running mesh-agent process and logs its output.
    """

    def __init__(self, host, log_path=None):
        self.host = host
        self.mesh_ip = None
        self.c = 8100
        self.p = 50051
        self.mesh_agent_process = None
        self.cmd = None
        self.is_pass = False
        self.external = False
        self.log_path = log_path
        self.subdir = Path(
            "mesh_agent_logs", "mesh-agent" if host is None else host.name
        )
        self.filename = "mesh_agent.log"
        self.log_file_path = Path(
            log_path if log_path else LOG_FOLDER, self.subdir, self.filename
        )

    def start(self, c=None, p=None):
        if not self.mesh_agent_process or not self.mesh_agent_process.running:
            cmd = "mesh-agent"
            if c:
                self.c = c
                cmd += f" -c {self.c}"
            if p:
                self.p = p
                cmd += f" -p {self.p}"

            self.cmd = cmd
            logger.info(
                f"Starting mesh agent on host {self.host.name} with command: {self.cmd}"
            )
            self.mesh_agent_process = self.host.connection.start_process(
                self.cmd, stderr_to_stdout=True
            )

            # Start background logging thread
            def log_output():
                for line in self.mesh_agent_process.get_stdout_iter():
                    save_process_log(
                        subdir=self.subdir,
                        filename=self.filename,
                        text=line.rstrip(),
                        cmd=cmd,
                        log_dir=self.log_path,
                    )

            threading.Thread(target=log_output, daemon=True).start()

            self.mesh_ip = self.host.connection.ip
            logger.info(
                f"Mesh agent started on host {self.host.name}: {self.mesh_agent_process.running}"
            )

    def stop(self, log_path=None):
        # Use the provided log_path or the one stored in the object
        log_path = log_path if log_path is not None else self.log_path

        if self.mesh_agent_process and self.mesh_agent_process.running:
            self.mesh_agent_process.stop()
            logger.info(f"Mesh agent stopped on host {self.host.name}")

            result = output_validator(
                log_file_path=self.log_file_path,
                error_keywords=MESH_AGENT_ERROR_KEYWORDS,
            )
            if result["errors"]:
                logger.error(f"Errors found: {result['errors']}")

            self.is_pass = result["is_pass"]

            self.mesh_agent_process = None

            if not self.is_pass:
                logger.error(f"Mesh agent process on host {self.host.name} failed.")
