# [DRAFT] MCM Control Plane REST API Definition (**Control Plane API**)

***THIS IS A DRAFT. WORK IN PROGRESS.***

* Mesh Agent runs a server to handle **Control Plane API** requests.
* **Control Plane API** is exposed to any external network configuration tools.
* **Control Plane API** is a REST API based on HTTP 1.1.

## List Media Proxies
```
GET /media-proxy
```
Query parameters
```
status - if 'true', include Media Proxy status
config - if 'true', include Media Proxy configuration
```
**Status**: `200`
```
Content-Type: application/json

{
    "mediaProxy": [
        {
            "id": "9d41fa7d-cc07-485a-b988-c51bcc8edb0e",
            "conns": [
                {
                    "id": "312de6e6-b770-4634-96f5-64cb8afa464b",
                    "groupId": "192.168.96.11:9002",
                    "config": {
                        "bufQueueCapacity": 16,
                        "maxPayloadSize": 0,
                        "maxMetadataSize": 0,
                        "calculatedPayloadSize": 921600,
                        "conn": {
                            "st2110": {
                                "remoteIpAddr": "192.168.96.11",
                                "port": 9002,
                                "pacing": "",
                                "payloadType": 112
                            }
                        },
                        "payload": {
                            "video": {
                                "width": 640,
                                "height": 360,
                                "fps": 60
                            }
                        },
                        "kind": "rx"
                    },
                    "status": {
                        "registeredAt": "2024-12-31T10:54:41.986Z",
                        "updatedAt": "2024-12-31T10:55:56.429Z",
                        "state": "active",
                        "linked": true,
                        "inbound": 1715097600,
                        "outbound": 1715097600,
                        "trnSucceeded": 1861,
                        "trnFailed": 0,
                        "tps": 24.9,
                        "inBandwidthMbit": 183.952,
                        "outBandwidthMbit": 183.952,
                        "errors": 0,
                        "errorsDelta": 0
                    }
                }
            ],
            "bridges": [
                {
                    "id": "287f4d28-1cd5-430b-8c8b-616d7c502eaa",
                    "groupId": "192.168.96.11:9002",
                    "config": {
                        "bufQueueCapacity": 16,
                        "maxPayloadSize": 0,
                        "maxMetadataSize": 0,
                        "calculatedPayloadSize": 921600,
                        "conn": {
                            "st2110": {
                                "remoteIpAddr": "192.168.96.11",
                                "port": 9002,
                                "pacing": "",
                                "payloadType": 112
                            }
                        },
                        "payload": {
                            "video": {
                                "width": 640,
                                "height": 360,
                                "fps": 60
                            }
                        },
                        "type": "st2110",
                        "kind": "tx",
                        "st2110": {
                            "remoteIp": "192.168.96.11",
                            "port": 9002
                        }
                    },
                    "status": {
                        "registeredAt": "2024-12-31T10:54:41.986Z",
                        "updatedAt": "2024-12-31T10:55:56.429Z",
                        "state": "active",
                        "linked": true,
                        "inbound": 1715097600,
                        "outbound": 1590681600,
                        "trnSucceeded": 1726,
                        "trnFailed": 0,
                        "tps": 24.9,
                        "inBandwidthMbit": 183.952,
                        "outBandwidthMbit": 183.952,
                        "errors": 0,
                        "errorsDelta": 0
                    }
                },
                {
                    "id": "1702a578-63f5-4414-ba32-0065a4ab980c",
                    "groupId": "192.168.96.11:9002",
                    "config": {
                        "bufQueueCapacity": 16,
                        "maxPayloadSize": 0,
                        "maxMetadataSize": 0,
                        "calculatedPayloadSize": 921600,
                        "conn": {
                            "st2110": {
                                "remoteIpAddr": "192.168.96.11",
                                "port": 9002,
                                "pacing": "",
                                "payloadType": 112
                            }
                        },
                        "payload": {
                            "video": {
                                "width": 640,
                                "height": 360,
                                "fps": 60
                            }
                        },
                        "type": "rdma",
                        "kind": "tx",
                        "rdma": {
                            "remoteIp": "192.168.97.12",
                            "port": 9300
                        }
                    },
                    "status": {
                        "registeredAt": "2024-12-31T10:55:02.081Z",
                        "updatedAt": "2024-12-31T10:55:56.429Z",
                        "state": "active",
                        "linked": true,
                        "inbound": 1252454400,
                        "outbound": 1251532800,
                        "trnSucceeded": 1358,
                        "trnFailed": 0,
                        "tps": 24.9,
                        "inBandwidthMbit": 183.952,
                        "outBandwidthMbit": 183.952,
                        "errors": 0,
                        "errorsDelta": 0
                    }
                }
            ]
        },
        {
            "id": "b74aa26c-c18d-425c-b34d-2202a8e54405",
            "conns": [
                {
                    "id": "32cb5664-804e-4d07-a5aa-b8d3829fb35b",
                    "groupId": "192.168.96.10:9002",
                    "config": {
                        "bufQueueCapacity": 16,
                        "maxPayloadSize": 0,
                        "maxMetadataSize": 0,
                        "calculatedPayloadSize": 921600,
                        "conn": {
                            "st2110": {
                                "remoteIpAddr": "192.168.96.10",
                                "port": 9002,
                                "pacing": "",
                                "payloadType": 112
                            }
                        },
                        "payload": {
                            "video": {
                                "width": 640,
                                "height": 360,
                                "fps": 60
                            }
                        },
                        "kind": "tx"
                    },
                    "status": {
                        "registeredAt": "2024-12-31T10:54:29.673Z",
                        "updatedAt": "2024-12-31T10:55:56.809Z",
                        "state": "active",
                        "linked": true,
                        "inbound": 1598976000,
                        "outbound": 1598976000,
                        "trnSucceeded": 1735,
                        "trnFailed": 0,
                        "tps": 24.9,
                        "inBandwidthMbit": 183.768,
                        "outBandwidthMbit": 183.768,
                        "errors": 0,
                        "errorsDelta": 0
                    }
                }
            ],
            "bridges": [
                {
                    "id": "f5dbafe4-f868-4d9e-a014-721c5c096eba",
                    "groupId": "192.168.96.10:9002",
                    "config": {
                        "bufQueueCapacity": 16,
                        "maxPayloadSize": 0,
                        "maxMetadataSize": 0,
                        "calculatedPayloadSize": 921600,
                        "conn": {
                            "st2110": {
                                "remoteIpAddr": "192.168.96.10",
                                "port": 9002,
                                "pacing": "",
                                "payloadType": 112
                            }
                        },
                        "payload": {
                            "video": {
                                "width": 640,
                                "height": 360,
                                "fps": 60
                            }
                        },
                        "type": "st2110",
                        "kind": "rx",
                        "st2110": {
                            "remoteIp": "192.168.96.10",
                            "port": 9002
                        }
                    },
                    "status": {
                        "registeredAt": "2024-12-31T10:54:29.673Z",
                        "updatedAt": "2024-12-31T10:55:56.809Z",
                        "state": "active",
                        "linked": true,
                        "inbound": 1598976000,
                        "outbound": 1598976000,
                        "trnSucceeded": 1735,
                        "trnFailed": 0,
                        "tps": 24.9,
                        "inBandwidthMbit": 183.768,
                        "outBandwidthMbit": 183.768,
                        "errors": 0,
                        "errorsDelta": 0
                    }
                }
            ]
        },
        {
            "id": "bf0ae69d-cf68-4559-8b2c-ad88df25b354",
            "conns": [
                {
                    "id": "fb35b954-6ff9-40fc-b5b3-33b1a3750947",
                    "groupId": "192.168.96.11:9002",
                    "config": {
                        "bufQueueCapacity": 16,
                        "maxPayloadSize": 0,
                        "maxMetadataSize": 0,
                        "calculatedPayloadSize": 921600,
                        "conn": {
                            "st2110": {
                                "remoteIpAddr": "192.168.96.11",
                                "port": 9002,
                                "pacing": "",
                                "payloadType": 112
                            }
                        },
                        "payload": {
                            "video": {
                                "width": 640,
                                "height": 360,
                                "fps": 60
                            }
                        },
                        "kind": "tx"
                    },
                    "status": {
                        "registeredAt": "2024-12-31T10:55:02.081Z",
                        "updatedAt": "2024-12-31T10:55:56.335Z",
                        "state": "active",
                        "linked": true,
                        "inbound": 1249689600,
                        "outbound": 1249689600,
                        "trnSucceeded": 1356,
                        "trnFailed": 0,
                        "tps": 24.9,
                        "inBandwidthMbit": 183.952,
                        "outBandwidthMbit": 183.952,
                        "errors": 0,
                        "errorsDelta": 0
                    }
                }
            ],
            "bridges": [
                {
                    "id": "9baa5e11-1192-457d-8c3c-3464cb4d65cb",
                    "groupId": "192.168.96.11:9002",
                    "config": {
                        "bufQueueCapacity": 16,
                        "maxPayloadSize": 0,
                        "maxMetadataSize": 0,
                        "calculatedPayloadSize": 921600,
                        "conn": {
                            "st2110": {
                                "remoteIpAddr": "192.168.96.11",
                                "port": 9002,
                                "pacing": "",
                                "payloadType": 112
                            }
                        },
                        "payload": {
                            "video": {
                                "width": 640,
                                "height": 360,
                                "fps": 60
                            }
                        },
                        "type": "rdma",
                        "kind": "rx",
                        "rdma": {
                            "remoteIp": "192.168.97.10",
                            "port": 9300
                        }
                    },
                    "status": {
                        "registeredAt": "2024-12-31T10:55:02.081Z",
                        "updatedAt": "2024-12-31T10:55:56.335Z",
                        "state": "active",
                        "linked": true,
                        "inbound": 1249689600,
                        "outbound": 1249689600,
                        "trnSucceeded": 1356,
                        "trnFailed": 0,
                        "tps": 24.9,
                        "inBandwidthMbit": 183.952,
                        "outBandwidthMbit": 183.952,
                        "errors": 0,
                        "errorsDelta": 0
                    }
                }
            ]
        }
    ]
}
```

## Get Media Proxy Status
```
GET /media-proxy/{id}
```
Query parameters
```
config - if 'true', include Media Proxy configuration
```
**Status**: `200`
```
Content-Type: application/json

{
    "status": {
        "healthy": true,
        "startedAt": 2024-10-17T17:24:53Z,
        "connectionNum": 1
    },
    "config": {
        "ipAddr": "192.168.95.1"
    }
}
```

## List Connections
```
GET /connection
```
Query parameters
```
status - if 'true', include connection status
config - if 'true', include connection configuration
```
**Status**: `200`
```
Content-Type: application/json

{
    "connection": [
        {
            "id": "32cb5664-804e-4d07-a5aa-b8d3829fb35b",
            "proxyId": "b74aa26c-c18d-425c-b34d-2202a8e54405",
            "groupId": "192.168.96.10:9002",
            "config": {
                "bufQueueCapacity": 16,
                "maxPayloadSize": 0,
                "maxMetadataSize": 0,
                "calculatedPayloadSize": 921600,
                "conn": {
                    "st2110": {
                        "remoteIpAddr": "192.168.96.10",
                        "port": 9002,
                        "pacing": "",
                        "payloadType": 112
                    }
                },
                "payload": {
                    "video": {
                        "width": 640,
                        "height": 360,
                        "fps": 60
                    }
                },
                "kind": "tx"
            },
            "status": {
                "registeredAt": "2024-12-31T10:54:29.673Z",
                "updatedAt": null,
                "state": "active",
                "linked": false,
                "inbound": 0,
                "outbound": 0,
                "trnSucceeded": 0,
                "trnFailed": 0,
                "tps": 0,
                "inBandwidthMbit": 0,
                "outBandwidthMbit": 0,
                "errors": 0,
                "errorsDelta": 0
            }
        },
        {
            "id": "312de6e6-b770-4634-96f5-64cb8afa464b",
            "proxyId": "9d41fa7d-cc07-485a-b988-c51bcc8edb0e",
            "groupId": "192.168.96.11:9002",
            "config": {
                "bufQueueCapacity": 16,
                "maxPayloadSize": 0,
                "maxMetadataSize": 0,
                "calculatedPayloadSize": 921600,
                "conn": {
                    "st2110": {
                        "remoteIpAddr": "192.168.96.11",
                        "port": 9002,
                        "pacing": "",
                        "payloadType": 112
                    }
                },
                "payload": {
                    "video": {
                        "width": 640,
                        "height": 360,
                        "fps": 60
                    }
                },
                "kind": "rx"
            },
            "status": {
                "registeredAt": "2024-12-31T10:54:41.986Z",
                "updatedAt": null,
                "state": "active",
                "linked": false,
                "inbound": 0,
                "outbound": 0,
                "trnSucceeded": 0,
                "trnFailed": 0,
                "tps": 0,
                "inBandwidthMbit": 0,
                "outBandwidthMbit": 0,
                "errors": 0,
                "errorsDelta": 0
            }
        },
        {
            "id": "fb35b954-6ff9-40fc-b5b3-33b1a3750947",
            "proxyId": "bf0ae69d-cf68-4559-8b2c-ad88df25b354",
            "groupId": "192.168.96.11:9002",
            "config": {
                "bufQueueCapacity": 16,
                "maxPayloadSize": 0,
                "maxMetadataSize": 0,
                "calculatedPayloadSize": 921600,
                "conn": {
                    "st2110": {
                        "remoteIpAddr": "192.168.96.11",
                        "port": 9002,
                        "pacing": "",
                        "payloadType": 112
                    }
                },
                "payload": {
                    "video": {
                        "width": 640,
                        "height": 360,
                        "fps": 60
                    }
                },
                "kind": "tx"
            },
            "status": {
                "registeredAt": "2024-12-31T10:55:02.081Z",
                "updatedAt": null,
                "state": "active",
                "linked": false,
                "inbound": 0,
                "outbound": 0,
                "trnSucceeded": 0,
                "trnFailed": 0,
                "tps": 0,
                "inBandwidthMbit": 0,
                "outBandwidthMbit": 0,
                "errors": 0,
                "errorsDelta": 0
            }
        }
    ]
}
```

## Get Connection Status
```
GET /connection/{id}
```
Query parameters
```
config - if 'true', include connection configuration
```
**Status**: `200`
```
Content-Type: application/json

{
    "status": {
        "createdAt": 2024-10-17T17:24:53Z,
        "buffersSent": 300
    },
    "config": {
        "kind": "sender",
        "connType": "ST2110",
        "conn": {
            "remoteIpAddr": "192.168.95.2",
            "remotePort": 9001,
        },
        "payloadType": "video",
        "payload": {
            "width": 1920,
            "height": 1080,
            "fps": 60,
            "pixelFormat": "yuv422p10le"
        },
        "bufferSize": 2073600
    }
}
```

## List Multipoint Groups
```
GET /multipoint-group
```
Query parameters
```
status - if 'true', include Multipoint Group status
config - if 'true', include Multipoint Group configuration
```
**Status**: `200`
```
Content-Type: application/json

{
    "multipointGroup": [
        {
            "id": "192.168.96.10:9002",
            "connIds": [
                "32cb5664-804e-4d07-a5aa-b8d3829fb35b"
            ],
            "bridgeIds": [
                "f5dbafe4-f868-4d9e-a014-721c5c096eba"
            ]
        },
        {
            "id": "192.168.96.11:9002",
            "connIds": [
                "312de6e6-b770-4634-96f5-64cb8afa464b",
                "fb35b954-6ff9-40fc-b5b3-33b1a3750947"
            ],
            "bridgeIds": [
                "287f4d28-1cd5-430b-8c8b-616d7c502eaa",
                "9baa5e11-1192-457d-8c3c-3464cb4d65cb",
                "1702a578-63f5-4414-ba32-0065a4ab980c"
            ]
        }
    ]
}
```

## Get Multipoint Group Status
```
GET /multipoint-group/{id}
```
**Status**: `200`
```
Content-Type: application/json

{
    "status": {
        "createdAt": 2024-10-17T17:24:53Z
    }
}
```

## Add Multipoint Group
```
PUT /multipoint-group

Content-Type: application/json

{
    "config": {
        "ipAddr": "224.0.0.1"
    }
}
```
**Status**: `201`
```
Content-Type: application/json

{
    "id": "XXXXXXXX-YYYY-XXXX-YYYY-XXXXXXXXXXXX"
}
```

## Delete Multipoint Group
```
DELETE /multipoint-group/{id}
```
**Status**: `200`

## List Bridges
```
GET /bridge
```
Query parameters
```
status - if 'true', include Bridge status
config - if 'true', include Bridge configuration
```
**Status**: `200`
```
Content-Type: application/json

{
    "bridge": [
        {
            "id": "f5dbafe4-f868-4d9e-a014-721c5c096eba",
            "proxyId": "b74aa26c-c18d-425c-b34d-2202a8e54405",
            "groupId": "192.168.96.10:9002",
            "config": {
                "bufQueueCapacity": 16,
                "maxPayloadSize": 0,
                "maxMetadataSize": 0,
                "calculatedPayloadSize": 921600,
                "conn": {
                    "st2110": {
                        "remoteIpAddr": "192.168.96.10",
                        "port": 9002,
                        "pacing": "",
                        "payloadType": 112
                    }
                },
                "payload": {
                    "video": {
                        "width": 640,
                        "height": 360,
                        "fps": 60
                    }
                },
                "type": "st2110",
                "kind": "rx",
                "st2110": {
                    "remoteIp": "192.168.96.10",
                    "port": 9002
                }
            },
            "status": {
                "registeredAt": "2024-12-31T10:54:29.673Z",
                "updatedAt": null,
                "state": "active",
                "linked": false,
                "inbound": 0,
                "outbound": 0,
                "trnSucceeded": 0,
                "trnFailed": 0,
                "tps": 0,
                "inBandwidthMbit": 0,
                "outBandwidthMbit": 0,
                "errors": 0,
                "errorsDelta": 0
            }
        },
        {
            "id": "287f4d28-1cd5-430b-8c8b-616d7c502eaa",
            "proxyId": "9d41fa7d-cc07-485a-b988-c51bcc8edb0e",
            "groupId": "192.168.96.11:9002",
            "config": {
                "bufQueueCapacity": 16,
                "maxPayloadSize": 0,
                "maxMetadataSize": 0,
                "calculatedPayloadSize": 921600,
                "conn": {
                    "st2110": {
                        "remoteIpAddr": "192.168.96.11",
                        "port": 9002,
                        "pacing": "",
                        "payloadType": 112
                    }
                },
                "payload": {
                    "video": {
                        "width": 640,
                        "height": 360,
                        "fps": 60
                    }
                },
                "type": "st2110",
                "kind": "tx",
                "st2110": {
                    "remoteIp": "192.168.96.11",
                    "port": 9002
                }
            },
            "status": {
                "registeredAt": "2024-12-31T10:54:41.986Z",
                "updatedAt": null,
                "state": "active",
                "linked": false,
                "inbound": 0,
                "outbound": 0,
                "trnSucceeded": 0,
                "trnFailed": 0,
                "tps": 0,
                "inBandwidthMbit": 0,
                "outBandwidthMbit": 0,
                "errors": 0,
                "errorsDelta": 0
            }
        },
        {
            "id": "9baa5e11-1192-457d-8c3c-3464cb4d65cb",
            "proxyId": "bf0ae69d-cf68-4559-8b2c-ad88df25b354",
            "groupId": "192.168.96.11:9002",
            "config": {
                "bufQueueCapacity": 16,
                "maxPayloadSize": 0,
                "maxMetadataSize": 0,
                "calculatedPayloadSize": 921600,
                "conn": {
                    "st2110": {
                        "remoteIpAddr": "192.168.96.11",
                        "port": 9002,
                        "pacing": "",
                        "payloadType": 112
                    }
                },
                "payload": {
                    "video": {
                        "width": 640,
                        "height": 360,
                        "fps": 60
                    }
                },
                "type": "rdma",
                "kind": "rx",
                "rdma": {
                    "remoteIp": "192.168.97.10",
                    "port": 9300
                }
            },
            "status": {
                "registeredAt": "2024-12-31T10:55:02.081Z",
                "updatedAt": null,
                "state": "active",
                "linked": false,
                "inbound": 0,
                "outbound": 0,
                "trnSucceeded": 0,
                "trnFailed": 0,
                "tps": 0,
                "inBandwidthMbit": 0,
                "outBandwidthMbit": 0,
                "errors": 0,
                "errorsDelta": 0
            }
        },
        {
            "id": "1702a578-63f5-4414-ba32-0065a4ab980c",
            "proxyId": "9d41fa7d-cc07-485a-b988-c51bcc8edb0e",
            "groupId": "192.168.96.11:9002",
            "config": {
                "bufQueueCapacity": 16,
                "maxPayloadSize": 0,
                "maxMetadataSize": 0,
                "calculatedPayloadSize": 921600,
                "conn": {
                    "st2110": {
                        "remoteIpAddr": "192.168.96.11",
                        "port": 9002,
                        "pacing": "",
                        "payloadType": 112
                    }
                },
                "payload": {
                    "video": {
                        "width": 640,
                        "height": 360,
                        "fps": 60
                    }
                },
                "type": "rdma",
                "kind": "tx",
                "rdma": {
                    "remoteIp": "192.168.97.12",
                    "port": 9300
                }
            },
            "status": {
                "registeredAt": "2024-12-31T10:55:02.081Z",
                "updatedAt": null,
                "state": "active",
                "linked": false,
                "inbound": 0,
                "outbound": 0,
                "trnSucceeded": 0,
                "trnFailed": 0,
                "tps": 0,
                "inBandwidthMbit": 0,
                "outBandwidthMbit": 0,
                "errors": 0,
                "errorsDelta": 0
            }
        }
    ]
}
```

## Get Bridge Status
```
GET /bridge/{id}
```
Query parameters
```
config - if 'true', include Bridge configuration
```
**Status**: `200`
```
Content-Type: application/json

{
    "status": {
        "createdAt": 2024-10-17T17:24:53Z
    }
    ... TBD ...
}
```

## Add Bridge
```
PUT /bridge

Content-Type: application/json

{
    "config": {
        "ipAddr": "224.0.0.1"
    }
    ... TBD ...
}
```
**Status**: `201`
```
Content-Type: application/json

{
    "id": "XXXXXXXX-XXXX-XXXX-XXXX-YYYYYYYYYYYY"
}
```

## Delete Bridge
```
DELETE /bridge/{id}
```
**Status**: `200`

## Get Logic Rules Manifest
```
GET /manifest
```
**Status**: `200`
```
Content-Type: application/json

{
    "manifest": {
        "rules": [
            ... TBD ...
        ]
    }
}
```

## Update Logic Rules Manifest
```
POST /manifest

Content-Type: application/json

{
    "manifest": {
        "rules": [
            ... TBD ...
        ]
    }
}
```
**Status**: `200`
