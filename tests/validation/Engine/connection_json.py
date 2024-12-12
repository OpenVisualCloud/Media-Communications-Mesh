import json

from connection import Connection
from payload import Payload

class ConnectionJson:
    """Class used to prepare connection.json config file"""
    def __init__(
        self,
        bufferQueueCapacity=16,
        maxPayloadSize=2097152,
        maxMetadataSize=8192,
        connection=Connection(),
        payload=Payload()
    ):
        self.bufferQueueCapacity = bufferQueueCapacity
        self.maxPayloadSize = maxPayloadSize
        self.maxMetadataSize = maxMetadataSize
        self.connection = connection
        self.payload = payload

    def toDict(self) -> dict:
        return {
            "bufferQueueCapacity": self.bufferQueueCapacity,
            "maxPayloadSize": self.maxPayloadSize,
            "maxMetadataSize": self.maxMetadataSize,
            **self.connection.toDict(),
            **self.payload.toDict()
        }

    def toJson(self):
        return json.dumps(self.toDict())
