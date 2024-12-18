# MCM Control Plane REST API Definition (**Control Plane API**)

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
            "id": "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX"
            "status": {
                "healthy": true,
                "startedAt": 2024-10-17T17:24:53Z,
                "connectionNum": 1
            },
        },
        {
            "id": "XXXXXXXX-YYYY-XXXX-XXXX-XXXXXXXXXXXX"
            "status": {
                "healthy": true,
                "startedAt": 2024-10-17T17:24:54Z,
                "connectionNum": 2
            }
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
            "id": "XXXXXXXX-XXXX-YYYY-XXXX-XXXXXXXXXXXX",
            "status": {
                "createdAt": 2024-10-17T17:24:53Z,
                "buffersSent": 301
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
            "id": "XXXXXXXX-XXXX-XXXX-YYYY-XXXXXXXXXXXX",
            "status": {
                "createdAt": 2024-10-17T17:24:53Z
            }
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
            "id": "XXXXXXXX-XXXX-XXXX-XXXX-YYYYYYYYYYYY",
            "status": {
                "createdAt": 2024-10-17T17:24:53Z
            },
            "config": {
                "ipAddr": "224.0.0.1"
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
