import logging
import threading
import time
from pathlib import Path

from mfd_connect.exceptions import ConnectionCalledProcessError
from Engine.mcm_apps import save_process_log, output_validator, LOG_FOLDER
from Engine.const import STOP_GRACEFULLY_PERIOD

logger = logging.getLogger(__name__)

MTL_MANAGER_ERROR_KEYWORDS = ["[ERROR]", "exception", "fail", "critical"]


class MtlManager:
    """
    Class to manage the lifecycle of the MtlManager process on a remote host.

    Attributes:
        host: Host object containing a .connection attribute for remote command execution.
        mtl_manager_process: The running MtlManager process object (if started).
        log_path: Path where logs will be stored
        is_pass: Flag indicating if the process completed successfully
    """

    def __init__(self, host, use_sudo=False, log_path=None):
        """
        Initialize the MtlManager with a host object.
        :param host: Host object with a .connection attribute.
        :param log_path: Path where logs will be stored
        """
        self.host = host
        self.use_sudo = use_sudo
        self.cmd = "sudo MtlManager" if use_sudo else "MtlManager"
        self.mtl_manager_process = None
        self.is_pass = False
        self.log_path = log_path
        self.subdir = Path("mtl_manager_logs", self.host.name)
        self.filename = "mtl_manager.log"
        self.log_file_path = Path(
            log_path if log_path else LOG_FOLDER, self.subdir, self.filename
        )

    def start(self):
        """
        Starts the MtlManager process on the remote host using sudo.
        Returns True if started successfully, False otherwise.
        """
        if not self.mtl_manager_process or not self.mtl_manager_process.running:
            connection = self.host.connection
            try:
                logger.info(f"Running command on host {self.host.name}: {self.cmd}")
                self.mtl_manager_process = connection.start_process(
                    self.cmd, stderr_to_stdout=True
                )

                if not self.mtl_manager_process.running:
                    err = self.mtl_manager_process.stderr_text
                    logger.error(f"MtlManager failed to start. Error output:\n{err}")
                    return False

                def log_output():
                    for line in self.mtl_manager_process.get_stdout_iter():
                        save_process_log(
                            subdir=str(self.subdir),
                            filename=self.filename,
                            text=line.rstrip(),
                            cmd=self.cmd,
                            log_dir=self.log_path or LOG_FOLDER,
                        )

                threading.Thread(target=log_output, daemon=True).start()

                logger.info(
                    f"MtlManager started with PID {self.mtl_manager_process.pid}."
                )
                return True
            except ConnectionCalledProcessError as e:
                logger.error(f"Failed to start MtlManager: {e}")
                return False
        else:
            logger.info("MtlManager is already running.")
            return True

    def stop(self, log_path=None):
        """
        Stops all MtlManager processes on the remote host using sudo pkill.

        :param log_path: Optional path to save logs, overrides the path set during initialization
        :return: True if the process was stopped successfully, False otherwise
        """
        log_path = log_path if log_path is not None else self.log_path

        connection = self.host.connection
        try:
            if self.mtl_manager_process and self.mtl_manager_process.running:
                self.mtl_manager_process.stop()
                time.sleep(STOP_GRACEFULLY_PERIOD)
                if self.mtl_manager_process.running:
                    self.mtl_manager_process.kill()
                    logger.error(
                        f"MtlManager process on host {self.host.name} did not exit gracefully and had to be killed."
                    )

            try:
                result = connection.execute_command("pgrep MtlManager")
                if result.return_code == 0:
                    logger.info("Stopping MtlManager using sudo pkill MtlManager...")
                    try:
                        connection.execute_command("sudo pkill MtlManager")
                    except ConnectionCalledProcessError as e:
                        logger.warning(f"pkill command had non-zero exit: {e}")
                else:
                    logger.info(
                        f"No MtlManager process found running on host {self.host.name}"
                    )
            except ConnectionCalledProcessError:
                logger.info(
                    f"No MtlManager process found running on host {self.host.name}"
                )

            logger.info(f"MtlManager stopped on host {self.host.name}")

            try:
                result = output_validator(
                    log_file_path=str(self.log_file_path),
                    error_keywords=MTL_MANAGER_ERROR_KEYWORDS,
                )
                if result["errors"]:
                    logger.error(f"Errors found in MtlManager log: {result['errors']}")
                self.is_pass = result["is_pass"]
            except Exception as e:
                logger.error(f"Error validating MtlManager logs: {e}")
                self.is_pass = False

            self.mtl_manager_process = None
            if not self.is_pass:
                logger.error(f"MtlManager process on host {self.host.name} failed.")

            return self.is_pass

        except ConnectionCalledProcessError as e:
            logger.error(f"Failed to stop MtlManager: {e}")
            self.is_pass = False
            return False
