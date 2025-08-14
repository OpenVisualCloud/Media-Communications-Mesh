# SDK Bridge-related Parameter Propagation — Developer's Guide

SDK allows passing some parameters to be applied when Egress or Ingress Bridge is created in Media Proxy.

When adding or modifying any of the RDMA or SMPTE ST 2110 SDK parameters, many parts of code should be adjusted accordingly. The affected software components are SDK, Media Proxy, and Agent.

The following diagram shows the propagation flow of SDK parameters from the JSON configuration to the bridge's `configure()` method. RDMA classes are taken as an example.

> Note: Each function AND each data structure mentioned in the diagram should be revised and adjusted appropriately.

```mermaid
flowchart
    a1(["mesh_create_tx_connection()
        mesh_create_rx_connection()"])

    subgraph sdk [SDK]
        direction TB
        a2("parse_from_json()")
        a3("CreateConnection()")
    end

    subgraph proxy [Media Proxy]
        direction TB
        a4("connection::Config.assign_from_pb()")
        a5("connection::Config.assign_to_pb()")
        a8("StartCommandQueue()")
        a9("create_bridge()")
        a10("RdmaTx::configure()
             RdmaRx::configure()")
    end

    subgraph agent [Agent]
        direction TB
        a6("SDKConnectionConfig.AssignFromPb()")
        a6-1("CheckPayloadCompatibility()")
        a7("SDKConnectionConfig.AssignToPb()")
    end

    a1 -- JSON config --> a2 -- mesh::ConnectionConfig --> a3
    a3 -- Protobuf sdk::ConnectionConfig --> a4 -- connection::Config --> a5
    a5 -- Protobuf sdk.ConnectionConfig --> a6 -- SDKConnectionConfig --> a6-1
    a6-1 -- SDKConnectionConfig --> a7
    a7 -- Protobuf sdk.ConnectionConfig --> a8 -- BridgeConfig --> a9
    a9 -- Rdma class config struct --> a10
```

## Checklist: What to adjust to enable passing SDK parameters to the recipient class configuration?

1. Adjust the SDK code
    * Structure `mesh::ConnectionConfig`.
    * Method `parse_from_json()`
    * Proto file `conn-config.proto` – Add/Modify/Remove fields.
    * Method `CreateConnection()`.
1. Adjust the Media Proxy code
    * Structure `connection::Config`.
    * Method `connection::Config.assign_from_pb()`.
    * Method `connection::Config.assign_to_pb()`.
1. Adjust the Agent code
    * Structure `SDKConnectionConfig`.
    * Function `SDKConnectionConfig.AssignFromPb()`.
    * Function `CheckPayloadCompatibility()`.
    * Function `SDKConnectionConfig.AssignToPb()`.
1. Adjust the Media Proxy code
    * Structure `BridgeConfig`.
    * Method `StartCommandQueue()`.
    * Recipient class config struct, e.g. RDMA.
    * Method `create_bridge()`.
1. Update documentation.
