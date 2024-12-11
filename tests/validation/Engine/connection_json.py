class ConnectionJson:
    """Class used to prepare connection.json config file"""

    def __init__(
        bufferQueueCapacity=16,
        maxPayloadSize=2097152,
        maxMetadataSize=8192,
    )
        self.bufferQueueCapacity = bufferQueueCapacity
        self.maxPayloadSize = maxPayloadSize
        self.maxMetadataSize = maxMetadataSize
