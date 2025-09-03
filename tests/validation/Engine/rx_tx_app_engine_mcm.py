# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh
import logging
import os
import threading
import time
from pathlib import Path

from mfd_connect.exceptions import RemoteProcessInvalidState

import Engine.rx_tx_app_client_json
import Engine.rx_tx_app_connection_json
from Engine.const import LOG_FOLDER, DEFAULT_OUTPUT_PATH
from common.log_constants import (
    RX_REQUIRED_LOG_PHRASES,
    RX_OPTIONAL_LOG_PHRASES,
    TX_REQUIRED_LOG_PHRASES,
    RX_TX_APP_ERROR_KEYWORDS,
)
from Engine.mcm_apps import (
    get_media_proxy_port,
    save_process_log,
    get_mcm_path,
)
from Engine.rx_tx_app_file_validation_utils import validate_file, cleanup_file

logger = logging.getLogger(__name__)


def create_client_json(
    build: str, client: Engine.rx_tx_app_client_json.ClientJson, log_path: str = "", instance_num = None, instance_type = None
) -> None:
    logger.debug("Client JSON:")
    for line in client.to_json().splitlines():
        logger.debug(line)
    filename = (
        f"client_{instance_type.lower()}_{instance_num}.json"
        if instance_num is not None and instance_type is not None
        else "client.json"
    )
    output_path = str(Path(build, "tests", "tools", "TestApp", "build", filename))
    logger.debug(f"Client JSON path: {output_path}")
    client.prepare_and_save_json(output_path=output_path)
    log_dir = log_path if log_path else LOG_FOLDER
    client.copy_json_to_logs(log_path=log_dir, filename=filename)


def create_connection_json(
    build: str,
    rx_tx_app_connection: Engine.rx_tx_app_connection_json.ConnectionJson,
    log_path: str = "",
    instance_num = None,
    instance_type = None
) -> None:
    logger.debug("Connection JSON:")
    for line in rx_tx_app_connection.to_json().splitlines():
        logger.debug(line)
    filename = (
        f"connection_{instance_type.lower()}_{instance_num}.json"
        if instance_num is not None  and instance_type is not None
        else "connection.json"
    )
    output_path = str(
        Path(build, "tests", "tools", "TestApp", "build", filename)
    )
    logger.debug(f"Connection JSON path: {output_path}")
    rx_tx_app_connection.prepare_and_save_json(output_path=output_path)
    log_dir = log_path if log_path else LOG_FOLDER
    rx_tx_app_connection.copy_json_to_logs(log_path=log_dir, filename=filename)


class AppRunnerBase:
    def __init__(
        self,
        host,
        media_path,
        rx_tx_app_connection,
        payload_type,
        file_dict,
        file,
        input_file=None,
        output_file=None,
        output_path=None,
        no_proxy="127.0.0.1",
        loop=None,
        log_path=None,
        media_proxy_port=None,
        instance_num=None,
    ):
        self.host = host
        self.mcm_path = get_mcm_path(host)
        self.media_path = media_path
        self.file_dict = file_dict
        self.file = file
        self.rx_tx_app_connection = rx_tx_app_connection()
        self.payload = payload_type.from_file_info(
            media_path=self.media_path, file_info=file_dict
        )
        self.media_proxy_port = get_media_proxy_port(host)
        self.rx_tx_app_client_json = Engine.rx_tx_app_client_json.ClientJson(
            host, apiConnectionString=f"Server=127.0.0.1; Port={self.media_proxy_port}"
        )
        self.rx_tx_app_connection_json = (
            Engine.rx_tx_app_connection_json.ConnectionJson(
                host=host,
                rx_tx_app_connection=self.rx_tx_app_connection,
                payload=self.payload,
            )
        )
        self.app_path = host.connection.path(
            self.mcm_path, "tests", "tools", "TestApp", "build"
        )
        client_filename = f"client_{instance_num}.json" if instance_num is not None else "client.json"
        connection_filename = f"connection_{instance_num}.json" if instance_num is not None else "connection.json"
        
        self.client_cfg_file = host.connection.path(self.app_path, client_filename)
        self.connection_cfg_file = host.connection.path(
            self.app_path, connection_filename
        )
        self.input = input_file
        self.output_path = getattr(
            host.topology.extra_info, "output_path", DEFAULT_OUTPUT_PATH
        )
        if output_file and output_path:
            self.output = host.connection.path(output_path, output_file)
        elif output_file:
            self.output = host.connection.path(self.output_path, output_file)
        else:
            self.output = None

        self.no_proxy = no_proxy
        self.loop = loop
        self.process = None
        self.cmd = None
        self.is_pass = False
        self.log_path = log_path
        self.media_proxy_port = media_proxy_port
        self.instance_num = instance_num

    def _get_app_cmd(self, direction):
        payload_type = self.payload.payload_type.value.capitalize()
        cmd = f"./{direction}{payload_type}App"
        cmd += f" {self.client_cfg_file} {self.connection_cfg_file}"
        if self.input:
            cmd += f" {self.input}"
            if self.loop:
                cmd += f" -l{self.loop}"
        if self.output:
            cmd += f" {self.output}"

        env_vars = []
        if self.no_proxy:
            env_vars.append(f"NO_PROXY={self.no_proxy} no_proxy={self.no_proxy}")
        if self.media_proxy_port:
            env_vars.append(f"MCM_MEDIA_PROXY_PORT={self.media_proxy_port}")

        if env_vars:
            cmd = f"{' '.join(env_vars)} {cmd}"

        self.cmd = cmd
        return cmd

    def _ensure_output_directory_exists(self):
        """Ensure the output directory exists, create if it doesn't."""
        if self.output and str(self.output) != "/dev/null":
            if "/dev/" in str(self.output):
                logger.debug(
                    f"Skipping directory creation for special device path: {self.output}"
                )
                return

            output_dir = self.host.connection.path(self.output).parent
            logger.debug(f"Ensuring output directory exists: {output_dir}")

            try:
                check_cmd = f"[ -d {output_dir} ] && echo 'exists' || echo 'not exists'"
                result = self.host.connection.execute_command(check_cmd, shell=True)
                if "not exists" in result.stdout:
                    logger.debug(f"Creating directory: {output_dir}")
                    mkdir_cmd = f"mkdir -p {output_dir}"
                    self.host.connection.execute_command(mkdir_cmd, shell=True)
                else:
                    logger.debug(f"Directory already exists: {output_dir}")
            except Exception as e:
                logger.warning(f"Error creating directory {output_dir}: {str(e)}")

    def start(self):
        log_dir = self.log_path if self.log_path else LOG_FOLDER

        direction_type = self.direction.lower() if hasattr(self, 'direction') else None
        
        create_client_json(
            self.mcm_path, 
            self.rx_tx_app_client_json, 
            log_path=log_dir,
            instance_num=self.instance_num,
            instance_type=direction_type
        )
        create_connection_json(
            self.mcm_path, 
            self.rx_tx_app_connection_json, 
            log_path=log_dir,
            instance_num=self.instance_num,
            instance_type=direction_type
        )
        self._ensure_output_directory_exists()

    def stop(self):
        validation_info = []
        file_validation_passed = True
        app_log_validation_status = False
        app_log_error_count = 0

        if self.process:
            try:
                if self.process.running:
                    self.process.stop()
                    if self.process.running:
                        logger.warning("Failed to stop the app process.")
                        self.process.kill()
            except RemoteProcessInvalidState:
                logger.info("Process has already finished (nothing to stop).")
            logger.info(f"{self.direction} app stopped.")

            log_dir = self.log_path if self.log_path is not None else LOG_FOLDER
            subdir = f"RxTx/{self.host.name}"
            if self.instance_num is not None:
                filename = f"{self.direction.lower()}_{self.instance_num}.log"
            else:
                filename = f"{self.direction.lower()}.log"
            log_file_path = os.path.join(log_dir, subdir, filename)

            app_log_validation_status = False
            app_log_error_count = 0
            from common.log_validation_utils import check_phrases_in_order
            
            # Add a short delay to ensure logs are fully written
            import time
            time.sleep(1.0)

            if self.direction in ("Rx", "Tx"):
                from common.log_validation_utils import validate_log_file

                required_phrases = (
                    RX_REQUIRED_LOG_PHRASES
                    if self.direction == "Rx"
                    else TX_REQUIRED_LOG_PHRASES
                )

                # For RX tests, filter out optional phrases
                # This applies to both single RX and multi-RX tests
                if self.direction == "Rx":
                    # Remove optional phrases from required phrases list
                    required_phrases = [p for p in required_phrases if p not in RX_OPTIONAL_LOG_PHRASES]
                
                # Add a short delay to ensure logs are fully written
                time.sleep(0.5)
                
                validation_result = validate_log_file(
                    log_file_path, required_phrases, self.direction, strict_order=False
                )

                self.is_pass = validation_result["is_pass"]
                app_log_validation_status = validation_result["is_pass"]
                app_log_error_count = validation_result["error_count"]
                validation_info.extend(validation_result["validation_info"])

                if (
                    not validation_result["is_pass"]
                    and validation_result["missing_phrases"]
                ):
                    print(
                        f"{self.direction} process did not pass. First missing phrase: {validation_result['missing_phrases'][0]}"
                    )
            else:
                from common.log_validation_utils import output_validator

                result = output_validator(
                    log_file_path=log_file_path,
                    error_keywords=RX_TX_APP_ERROR_KEYWORDS,
                )
                if result["errors"]:
                    logger.warning(f"Errors found: {result['errors']}")
                self.is_pass = result["is_pass"]
                app_log_validation_status = result["is_pass"]
                app_log_error_count = len(result["errors"])
                validation_info.append(f"=== {self.direction} App Log Validation ===")
                validation_info.append(f"Log file: {log_file_path}")
                validation_info.append(
                    f"Validation result: {'PASS' if result['is_pass'] else 'FAIL'}"
                )
                validation_info.append(f"Errors found: {len(result['errors'])}")
                if result["errors"]:
                    validation_info.append("Error details:")
                    for error in result["errors"]:
                        validation_info.append(f"  - {error}")
                if result["phrase_mismatches"]:
                    validation_info.append("Phrase mismatches:")
                    for phrase, found, expected in result["phrase_mismatches"]:
                        validation_info.append(
                            f"  - {phrase}: found '{found}', expected '{expected}'"
                        )

        if (
            self.direction == "Rx"
            and self.output
            and self.output_path
            and not str(self.output_path).startswith("/dev/null")
        ):
            validation_info.append(f"\n=== {self.direction} Output File Validation ===")
            validation_info.append(f"Expected output file: {self.output}")

            file_info, file_validation_passed = validate_file(
                self.host.connection, self.output, cleanup=False
            )
            validation_info.extend(file_info)
            self.is_pass = self.is_pass and file_validation_passed

        if validation_info:
            validation_info.append(f"\n=== Overall Validation Summary ===")
            overall_status = (
                "PASS" if self.is_pass and file_validation_passed else "FAIL"
            )
            validation_info.append(f"Overall validation: {overall_status}")
            validation_info.append(
                f"App log validation: {'PASS' if app_log_validation_status else 'FAIL'}"
            )
            if self.direction == "Rx":
                file_status = "PASS" if file_validation_passed else "FAIL"
                validation_info.append(f"File validation: {file_status}")
                if not self.is_pass or not file_validation_passed:
                    validation_info.append(
                        "Note: Overall validation fails if either app log or file validation fails"
                    )

            log_dir = self.log_path if self.log_path is not None else LOG_FOLDER
            subdir = f"RxTx/{self.host.name}"
            if self.instance_num is not None:
                validation_filename = f"{self.direction.lower()}_{self.instance_num}_validation.log"
            else:
                validation_filename = f"{self.direction.lower()}_validation.log"

            save_process_log(
                subdir=subdir,
                filename=validation_filename,
                text="\n".join(validation_info),
                log_dir=log_dir,
            )

    @property
    def direction(self):
        if isinstance(self, LapkaExecutor.Tx):
            return "Tx"
        elif isinstance(self, LapkaExecutor.Rx):
            return "Rx"
        else:
            return "Unknown"


class LapkaExecutor:
    class Tx(AppRunnerBase):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, **kwargs)
            self.input = self.payload.media_file_path

        def start(self):
            super().start()
            logger.debug(
                f"Starting Tx app with payload: {self.payload.payload_type} on {self.host}"
            )
            cmd = self._get_app_cmd("Tx")
            self.process = self.host.connection.start_process(
                cmd, shell=True, stderr_to_stdout=True, cwd=self.app_path
            )

            def log_output():
                log_dir = self.log_path if self.log_path is not None else LOG_FOLDER
                log_filename = f"tx_{self.instance_num}.log" if self.instance_num is not None else "tx.log"
                for line in self.process.get_stdout_iter():
                    save_process_log(
                        subdir=f"RxTx/{self.host.name}",
                        filename=log_filename,
                        text=line.rstrip(),
                        cmd=cmd,
                        log_dir=log_dir,
                    )

            threading.Thread(target=log_output, daemon=True).start()
            return self.process

    class Rx(AppRunnerBase):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, **kwargs)
            self._generate_output_file_path()

        def _generate_output_file_path(self):
            """Generate a proper output file path with improved naming convention."""
            if self.output is None:
                if self.output_path == "/dev/null":
                    self.output = self.host.connection.path("/dev/null")
                    logger.debug(f"Using /dev/null as output path")
                    return

                input_path = Path(str(self.payload.media_file_path))
                timestamp = int(time.time())

                if self.instance_num is not None:
                    output_filename = f"rx_{self.instance_num}_{self.host.name}_{input_path.stem}_{timestamp}{input_path.suffix}"
                else:
                    output_filename = f"rx_{self.host.name}_{input_path.stem}_{timestamp}{input_path.suffix}"

                self.output = self.host.connection.path(
                    self.output_path, output_filename
                )

            logger.debug(f"Generated output path: {self.output}")

        def start(self):
            super().start()
            logger.debug(
                f"Starting Rx app with payload: {self.payload.payload_type} on {self.host}"
            )
            cmd = self._get_app_cmd("Rx")
            self.process = self.host.connection.start_process(
                cmd, shell=True, stderr_to_stdout=True, cwd=self.app_path
            )

            def log_output():
                log_dir = self.log_path if self.log_path is not None else LOG_FOLDER
                log_filename = f"rx_{self.instance_num}.log" if self.instance_num is not None else "rx.log"
                for line in self.process.get_stdout_iter():
                    save_process_log(
                        subdir=f"RxTx/{self.host.name}",
                        filename=log_filename,
                        text=line.rstrip(),
                        cmd=cmd,
                        log_dir=log_dir,
                    )

            threading.Thread(target=log_output, daemon=True).start()
            return self.process

        def cleanup(self):
            """Clean up the output file created by the Rx app."""
            if self.output:
                success = cleanup_file(self.host.connection, str(self.output))
                if success:
                    logger.debug(f"Cleaned up Rx output file: {self.output}")
                else:
                    logger.warning(f"Failed to clean up Rx output file: {self.output}")
                return success
            return True
