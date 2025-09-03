# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh

import json
from pathlib import Path


class ClientJson:
    """Class used to create client.json configuration file"""

    def __init__(
        self,
        host,
        apiVersion="v1",
        apiConnectionString="Server=127.0.0.1; Port=8002",
        apiDefaultTimeoutMicroseconds=100000,
        maxMediaConnections=32,
    ):
        self.host = host
        self.apiVersion = apiVersion
        self.apiConnectionString = apiConnectionString
        self.apiDefaultTimeoutMicroseconds = apiDefaultTimeoutMicroseconds
        self.maxMediaConnections = maxMediaConnections

    def set_client(self, edits: dict) -> None:
        self.apiVersion = edits.get("apiVersion", self.apiVersion)
        self.apiConnectionString = edits.get(
            "apiConnectionString", self.apiConnectionString
        )
        self.apiDefaultTimeoutMicroseconds = edits.get(
            "apiDefaultTimeoutMicroseconds", self.apiDefaultTimeoutMicroseconds
        )
        self.maxMediaConnections = edits.get(
            "maxMediaConnections", self.maxMediaConnections
        )

    def to_json(self) -> str:
        json_dict = {
            "apiVersion": self.apiVersion,
            "apiConnectionString": self.apiConnectionString,
            "apiDefaultTimeoutMicroseconds": self.apiDefaultTimeoutMicroseconds,
            "maxMediaConnections": self.maxMediaConnections,
        }
        return json.dumps(json_dict, indent=4)

    def prepare_and_save_json(self, output_path: str = "client.json") -> None:
        f = self.host.connection.path(output_path)
        json_content = self.to_json().replace('"', '\\"')
        f.write_text(json_content)

    def copy_json_to_logs(self, log_path: str, filename="client.json") -> None:
        """Copy the client.json file to the log path on runner with specified filename.
        
        Places the file in the mesh-agent directory and includes rx/tx prefix based on instance name.
        """
        source_path = self.host.connection.path(filename)  # Original file
        
        # Create directory path under the host name directory
        mesh_agent_dir = Path(log_path) / "RxTx" / self.host.name
        mesh_agent_dir.mkdir(parents=True, exist_ok=True)
        
        # Just use the filename as provided by the caller
        # The create_client_json function now provides the properly formatted filename
        new_filename = filename
            
        dest_path = mesh_agent_dir / new_filename

        # Copy the client.json file to the mesh-agent directory
        with open(dest_path, "w") as dest_file:
            dest_file.write(self.to_json())
