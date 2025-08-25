# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2025 Intel Corporation
# Media Communications Mesh
import logging
import os
import threading
import time

from mfd_connect.exceptions import (
    SSHRemoteProcessEndException,
    RemoteProcessInvalidState,
)
from Engine.const import LOG_FOLDER
from Engine.mcm_apps import save_process_log
from Engine.rx_tx_app_file_validation_utils import cleanup_file

from .ffmpeg_io import FFmpegIO


SLEEP_BETWEEN_CHECKS = 0.5
RETRIES = 4
logger = logging.getLogger(__name__)


class FFmpeg:
    def __init__(
        self,
        prefix_variables: dict = {},
        ffmpeg_path: str = "ffmpeg",
        ffmpeg_input: FFmpegIO | None = None,
        ffmpeg_output: FFmpegIO | None = None,
        yes_overwrite: bool = False,
    ):
        self.prefix_variables = prefix_variables
        self.ffmpeg_path = ffmpeg_path
        self.ffmpeg_input = ffmpeg_input
        self.ffmpeg_output = ffmpeg_output
        self.yes_overwrite = yes_overwrite

    def get_items(self) -> dict:
        response = {
            "ffmpeg_path": self.ffmpeg_path,
            "ffmpeg_input": {},
            "ffmpeg_output": {},
            "yes_overwrite": self.yes_overwrite,
        }
        if self.ffmpeg_input:
            response["ffmpeg_input"] = self.ffmpeg_input.get_items()
        if self.ffmpeg_output:
            response["ffmpeg_output"] = self.ffmpeg_output.get_items()
        # TODO: Check if it takes yes_overwrite into account, and if should take prefix_variables into account
        return response

    def get_command(self) -> str:
        prefix = ""
        for key, value in self.prefix_variables.items():
            prefix += f"{key}={value} "
        response = f"{prefix}{self.ffmpeg_path}"
        if self.ffmpeg_input:
            response += self.ffmpeg_input.get_command()
        if self.ffmpeg_output:
            response += self.ffmpeg_output.get_command()
        if self.yes_overwrite:
            response += " -y"
        return response


class FFmpegExecutor:
    """
    FFmpeg wrapper with MCM plugin
    """

    def __init__(self, host, ffmpeg_instance: FFmpeg, log_path=None):
        self.host = host
        self.ff = ffmpeg_instance
        self.log_path = log_path
        self._processes = []
        self.is_pass = False

    def validate(self):
        """
        Validates the FFmpeg process execution and output.
        
        Performs two types of validation:
        1. Log validation - checks for required phrases and error keywords
        2. File validation - checks if the output file exists and has expected characteristics
        
        Generates validation report files.
        
        Returns:
            bool: True if validation passed, False otherwise
        """
        process_passed = True
        validation_info = []
        
        for process in self._processes:
            if process.return_code != 0:
                logger.warning(f"FFmpeg process on {self.host.name} failed with return code {process.return_code}")
                process_passed = False
        
        # Determine if this is a receiver or transmitter
        is_receiver = False
        if self.ff.ffmpeg_input and self.ff.ffmpeg_output:
            input_path = getattr(self.ff.ffmpeg_input, "input_path", None)
            output_path = getattr(self.ff.ffmpeg_output, "output_path", None)
            
            if input_path == "-" or (
                output_path and output_path != "-" and "." in output_path
            ):
                is_receiver = True
        
        direction = "Rx" if is_receiver else "Tx"
        
        # Find the log file
        log_dir = self.log_path if self.log_path else LOG_FOLDER
        subdir = f"RxTx/{self.host.name}"
        input_class_name = None
        if self.ff.ffmpeg_input:
            input_class_name = self.ff.ffmpeg_input.__class__.__name__
        prefix = "mtl_" if input_class_name and "Mtl" in input_class_name else "mcm_"
        log_filename = prefix + ("ffmpeg_rx.log" if is_receiver else "ffmpeg_tx.log")
        
        log_file_path = os.path.join(log_dir, subdir, log_filename)
        
        # Perform log validation
        from common.log_validation_utils import validate_log_file
        from common.ffmpeg_handler.log_constants import (
            FFMPEG_RX_REQUIRED_LOG_PHRASES, 
            FFMPEG_TX_REQUIRED_LOG_PHRASES,
            FFMPEG_ERROR_KEYWORDS
        )
        
        required_phrases = FFMPEG_RX_REQUIRED_LOG_PHRASES if is_receiver else FFMPEG_TX_REQUIRED_LOG_PHRASES
        
        if os.path.exists(log_file_path):
            validation_result = validate_log_file(
                log_file_path, 
                required_phrases, 
                direction, 
                FFMPEG_ERROR_KEYWORDS
            )
            
            log_validation_passed = validation_result['is_pass']
            validation_info.extend(validation_result['validation_info'])
        else:
            logger.warning(f"Log file not found at {log_file_path}")
            validation_info.append(f"=== {direction} Log Validation ===")
            validation_info.append(f"Log file: {log_file_path}")
            validation_info.append(f"Validation result: FAIL")
            validation_info.append(f"Errors found: 1")
            validation_info.append(f"Missing log file")
            log_validation_passed = False
        
        # File validation for Rx only run if output path isn't "/dev/null" or doesn't start with "/dev/null/"
        file_validation_passed = True
        if is_receiver and self.ff.ffmpeg_output and hasattr(self.ff.ffmpeg_output, "output_path"):
            output_path = self.ff.ffmpeg_output.output_path
            if output_path and not str(output_path).startswith("/dev/null"):
                validation_info.append(f"\n=== {direction} Output File Validation ===")
                validation_info.append(f"Expected output file: {output_path}")
                
                from Engine.rx_tx_app_file_validation_utils import validate_file
                file_info, file_validation_passed = validate_file(
                    self.host.connection, output_path, cleanup=False
                )
                validation_info.extend(file_info)
        
        # Overall validation status
        self.is_pass = process_passed and log_validation_passed and file_validation_passed
        
        # Save validation report
        validation_info.append(f"\n=== Overall Validation Summary ===")
        validation_info.append(f"Overall validation: {'PASS' if self.is_pass else 'FAIL'}")
        validation_info.append(f"Process validation: {'PASS' if process_passed else 'FAIL'}")
        validation_info.append(f"Log validation: {'PASS' if log_validation_passed else 'FAIL'}")
        if is_receiver:
            validation_info.append(f"File validation: {'PASS' if file_validation_passed else 'FAIL'}")
        validation_info.append(f"Note: Overall validation fails if any validation step fails")
        
        # Save validation report to a file
        from common.log_validation_utils import save_validation_report
        validation_path = os.path.join(log_dir, subdir, f"{direction.lower()}_validation.log")
        save_validation_report(validation_path, validation_info, self.is_pass)
        
        return self.is_pass

    def start(self):
        """Starts the FFmpeg process on the host, waits for the process to start."""
        cmd = self.ff.get_command()
        ffmpeg_process = self.host.connection.start_process(
            cmd, stderr_to_stdout=True, shell=True
        )
        self._processes.append(ffmpeg_process)
        CURRENT_RETRIES = RETRIES
        retries_counter = 0
        while not ffmpeg_process.running and retries_counter <= CURRENT_RETRIES:
            retries_counter += 1
            time.sleep(SLEEP_BETWEEN_CHECKS)
        # FIXME: Find a better way to check if the process is running; code below throws an error when the process is actually running sometimes
        if ffmpeg_process.running:
            logger.info(
                f"FFmpeg process started on {self.host.name} with command: {cmd}"
            )
        else:
            logger.debug(
                f"FFmpeg process failed to start on {self.host.name} after {CURRENT_RETRIES} retries."
            )

        log_dir = self.log_path if self.log_path else LOG_FOLDER
        subdir = f"RxTx/{self.host.name}"

        is_receiver = False

        if self.ff.ffmpeg_input and self.ff.ffmpeg_output:
            input_path = getattr(self.ff.ffmpeg_input, "input_path", None)
            output_path = getattr(self.ff.ffmpeg_output, "output_path", None)

            if input_path == "-" or (
                output_path and output_path != "-" and "." in output_path
            ):
                is_receiver = True

        filename = "ffmpeg_rx.log" if is_receiver else "ffmpeg_tx.log"
        input_class_name = None
        if self.ff.ffmpeg_input:
            input_class_name = self.ff.ffmpeg_input.__class__.__name__
        prefix = "mtl_" if input_class_name and "Mtl" in input_class_name else "mcm_"
        filename = prefix + filename

        def log_output():
            log_dir = self.log_path if self.log_path is not None else LOG_FOLDER
            for line in ffmpeg_process.get_stdout_iter():
                save_process_log(
                    subdir=subdir,
                    filename=filename,
                    text=line.rstrip(),
                    cmd=cmd,
                    log_dir=log_dir,
                )

        threading.Thread(target=log_output, daemon=True).start()

    # TODO: Think about a better way to handle the default wait time
    def stop(self, wait: float = 0.0) -> float:
        elapsed = 0.0
        CURRENT_RETRIES = RETRIES
        for process in self._processes:
            try:
                while process.running and wait >= 0:
                    time.sleep(SLEEP_BETWEEN_CHECKS)
                    elapsed += SLEEP_BETWEEN_CHECKS
                    wait -= SLEEP_BETWEEN_CHECKS
                while process.running and CURRENT_RETRIES >= 0:
                    process.stop()
                    CURRENT_RETRIES -= 1
                    time.sleep(SLEEP_BETWEEN_CHECKS)
                    elapsed += SLEEP_BETWEEN_CHECKS
                if process.running:
                    process.kill()
            except SSHRemoteProcessEndException as e:
                logger.warning(
                    f"FFmpeg process on {self.host.name} was already stopped: {e}"
                )
            except RemoteProcessInvalidState as e:
                logger.warning(
                    f"FFmpeg process on {self.host.name} is in an invalid state: {e}"
                )
            except Exception as e:
                logger.error(
                    f"Error while stopping FFmpeg process on {self.host.name}: {e}"
                )
                raise e
            logger.debug(
                f">>> FFmpeg execution on '{self.host.name}' host returned:\n{process.stdout_text}"
            )
            if process.return_code != 0:
                logger.warning(
                    f"FFmpeg process on {self.host.name} return code is {process.return_code}"
                )
            # assert process.return_code == 0 # Sometimes a different return code is returned for a graceful stop, so we do not assert it here
        else:
            logger.info("No FFmpeg process to stop!")
        return elapsed

    def wait_with_timeout(self, timeout=None):
        """Wait for the process to complete with a timeout."""
        if not timeout:
            timeout = 60  # Default timeout or use a constant like MCM_RXTXAPP_RUN_TIMEOUT

        try:
            for process in self._processes:
                if process.running:
                    process.wait(timeout=timeout)
        except Exception as e:
            logger.warning(f"FFmpeg process did not finish in time or error occurred: {e}")
            return False
        return True

    def cleanup(self):
        """Clean up any resources or output files."""
        if (self.ff.ffmpeg_output and 
            hasattr(self.ff.ffmpeg_output, "output_path") and 
            self.ff.ffmpeg_output.output_path and
            self.ff.ffmpeg_output.output_path != "-" and
            not str(self.ff.ffmpeg_output.output_path).startswith("/dev/null")):
            
            success = cleanup_file(self.host.connection, str(self.ff.ffmpeg_output.output_path))
            if success:
                logger.debug(f"Cleaned up output file: {self.ff.ffmpeg_output.output_path}")
            else:
                logger.warning(f"Failed to clean up output file: {self.ff.ffmpeg_output.output_path}")

def no_proxy_to_prefix_variables(host, prefix_variables: dict | None = None):
    """
    Handles the no_proxy and NO_PROXY environment variables for FFmpeg execution.

    Decision table for no_proxy and NO_PROXY:
    | test_config | decision                            | topology_config |
    | ----------- | ----------------------------------- | --------------- |
    | present     | use test_config's no_proxy (<-)     | present         |
    | present     | use test_config's no_proxy (<-)     | not present     |
    | not present | use topology_config's no_proxy (->) | present         |
    | not present | do not use no_proxy (return {})     | not present     |

    Lower-case no_proxy from topology_config is propagated to prefix_variables' upper-case NO_PROXY.
    if test_config defines NO_PROXY or no_proxy – it will be added to the prefix_variables (overwrite topology_config);
    else it will try to return the no_proxy from topology_config's media_proxy;
    else return an empty prefix_variables dictionary.
    """
    prefix_variables = prefix_variables if prefix_variables else {}
    try:
        if "no_proxy" not in prefix_variables.keys():
            prefix_variables["no_proxy"] = host.topology.extra_info.media_proxy.get(
                "no_proxy"
            )
        if "NO_PROXY" not in prefix_variables.keys():
            prefix_variables["NO_PROXY"] = host.topology.extra_info.media_proxy.get(
                "no_proxy"
            )  # lower-case getter on purpose
    except KeyError:  # when no extra_info or media_proxy is set, return {}
        pass
    finally:
        return prefix_variables
