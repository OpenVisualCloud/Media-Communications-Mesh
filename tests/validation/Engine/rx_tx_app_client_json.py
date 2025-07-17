# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024-2025 Intel Corporation
# Media Communications Mesh

import json


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
