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
from Engine.const import LOG_FOLDER, RX_TX_APP_ERROR_KEYWORDS, DEFAULT_OUTPUT_PATH
from Engine.mcm_apps import (
    get_media_proxy_port,
    output_validator,
    save_process_log,
    get_mcm_path,
)
from Engine.rx_tx_app_file_validation_utils import validate_file

logger = logging.getLogger(__name__)


def create_client_json(
    build: str, client: Engine.rx_tx_app_client_json.ClientJson
) -> None:
    logger.debug("Client JSON:")
    for line in client.to_json().splitlines():
        logger.debug(line)
    output_path = str(Path(build, "tests", "tools", "TestApp", "build", "client.json"))
    logger.debug(f"Client JSON path: {output_path}")
    client.prepare_and_save_json(output_path=output_path)


def create_connection_json(
    build: str, rx_tx_app_connection: Engine.rx_tx_app_connection_json.ConnectionJson
) -> None:
    logger.debug("Connection JSON:")
    for line in rx_tx_app_connection.to_json().splitlines():
        logger.debug(line)
    output_path = str(
        Path(build, "tests", "tools", "TestApp", "build", "connection.json")
    )
    logger.debug(f"Connection JSON path: {output_path}")
    rx_tx_app_connection.prepare_and_save_json(output_path=output_path)


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
        self.client_cfg_file = host.connection.path(self.app_path, "client.json")
        self.connection_cfg_file = host.connection.path(
            self.app_path, "connection.json"
        )
        self.input = input_file
        self.output_path = getattr(
            host.topology.extra_info, "output_path", DEFAULT_OUTPUT_PATH
        )

        # Handle output file path construction
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

        # Build environment variables
        env_vars = []
        if self.no_proxy:
            env_vars.append(f"NO_PROXY={self.no_proxy} no_proxy={self.no_proxy}")
        if self.media_proxy_port:
            env_vars.append(f"MCM_MEDIA_PROXY_PORT={self.media_proxy_port}")

        # Add environment variables to command
        if env_vars:
            cmd = f"{' '.join(env_vars)} {cmd}"

        self.cmd = cmd
        return cmd

    def _ensure_output_directory_exists(self):
        """Ensure the output directory exists, create if it doesn't."""
        if self.output:
            output_dir = self.host.connection.path(self.output).parent
            logger.debug(f"Ensuring output directory exists: {output_dir}")
            output_dir.mkdir(parents=True, exist_ok=True)

    def start(self):
        create_client_json(self.mcm_path, self.rx_tx_app_client_json)
        create_connection_json(self.mcm_path, self.rx_tx_app_connection_json)
        self._ensure_output_directory_exists()

    def stop(self):
        validation_info = []
        file_validation_passed = True

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

            log_dir = self.log_path if self.log_path else LOG_FOLDER
            subdir = f"RxTx/{self.host.name}"
            filename = f"{self.direction.lower()}.log"
            log_file_path = os.path.join(log_dir, subdir, filename)

            result = output_validator(
                log_file_path=log_file_path,
                error_keywords=RX_TX_APP_ERROR_KEYWORDS,
            )
            if result["errors"]:
                logger.warning(f"Errors found: {result['errors']}")

            self.is_pass = result["is_pass"]

            # Collect log validation info
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

        # File validation for Rx only run if output path isn't "/dev/null"
        if self.direction == "Rx" and self.output and self.output_path != "/dev/null":
            validation_info.append(f"\n=== {self.direction} Output File Validation ===")
            validation_info.append(f"Expected output file: {self.output}")

            file_info, file_validation_passed = validate_file(
                self.host.connection, self.output, cleanup=False
            )
            validation_info.extend(file_info)
            self.is_pass = self.is_pass and file_validation_passed

        # Save validation report to file
        if validation_info:
            validation_info.append(f"\n=== Overall Validation Summary ===")
            overall_status = (
                "PASS" if self.is_pass and file_validation_passed else "FAIL"
            )
            validation_info.append(f"Overall validation: {overall_status}")
            validation_info.append(
                f"App log validation: {'PASS' if result['is_pass'] else 'FAIL'}"
            )
            if self.direction == "Rx":
                file_status = "PASS" if file_validation_passed else "FAIL"
                validation_info.append(f"File validation: {file_status}")
                # Add note about overall validation logic
                if not self.is_pass or not file_validation_passed:
                    validation_info.append(
                        "Note: Overall validation fails if either app log or file validation fails"
                    )

            # Save to validation report file
            log_dir = self.log_path if self.log_path else LOG_FOLDER
            subdir = f"RxTx/{self.host.name}"
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

            # Start background logging thread
            subdir = f"RxTx/{self.host.name}"
            filename = "tx.log"

            def log_output():
                for line in self.process.get_stdout_iter():
                    save_process_log(
                        subdir=subdir,
                        filename=filename,
                        text=line.rstrip(),
                        cmd=cmd,
                        log_dir=self.log_path,
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
                # Generate output filename from input media file
                input_path = Path(str(self.payload.media_file_path))
                timestamp = int(time.time())

                # Create a descriptive filename with timestamp and host info
                output_filename = f"rx_{self.host.name}_{input_path.stem}_{timestamp}{input_path.suffix}"

                # Use the configured output path or default
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

            # Start background logging thread
            subdir = f"RxTx/{self.host.name}"
            filename = "rx.log"

            def log_output():
                for line in self.process.get_stdout_iter():
                    save_process_log(
                        subdir=subdir,
                        filename=filename,
                        text=line.rstrip(),
                        cmd=cmd,
                        log_dir=self.log_path,
                    )

            threading.Thread(target=log_output, daemon=True).start()
            return self.process

        def cleanup(self):
            """Clean up the output file created by the Rx app."""
            from Engine.rx_tx_app_file_validation_utils import cleanup_file

            if self.output:
                success = cleanup_file(self.host.connection, str(self.output))
                if success:
                    logger.debug(f"Cleaned up Rx output file: {self.output}")
                else:
                    logger.warning(f"Failed to clean up Rx output file: {self.output}")
                return success
            return True
