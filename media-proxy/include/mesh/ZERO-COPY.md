# Zero Copy — Development Notes

## Existing Multipoint Group Implementation

### Example Case – User App sends media stream

```mermaid
flowchart LR
    tx-app(Tx App)
    tx-sdk(SDK)
    rx-app1(Rx App 1)
    rx-sdk1(SDK)
    rx-app2(Rx App 2)
    rx-sdk2(SDK)

    subgraph Media Proxy
        subgraph group-boundaries[Multipoint Group]
            group(Copy-to-All
                  Engine)
        end

        local-rx(Local Rx)
        local-tx1(Local Tx 1)
        local-tx2(Local Tx 2)
        bridge-tx1(Egress Bridge 1)
        bridge-tx2(Egress Bridge 2)
    end

    tx-app ==> tx-sdk ==> local-rx ==.transmit==> group
    group ==.transmit==> local-tx1 ==> rx-sdk1 ==> rx-app1
    group ==.transmit==> local-tx2 ==> rx-sdk2 ==> rx-app2
    group ==.transmit==> bridge-tx1
    group ==.transmit==> bridge-tx2
```

## Development Phase 1 – Implement Zero-Copy Multipoint Group and Tx/Rx Gateways

### Case 1.1 – User App sends media stream

```mermaid
flowchart LR
    tx-app(Tx App)
    tx-sdk(SDK)
    rx-app1(Rx App 1)
    rx-sdk1(SDK)
    rx-app2(Rx App 2)
    rx-sdk2(SDK)

    subgraph Media Proxy
        subgraph group-boundaries[Multipoint Group]
            group(Zero-Copy
                  Engine)
        end

        subgraph local-rx-wrapper[Local Rx Wrapper]
            local-rx(Local Rx)
            rx-gateway(Rx Gateway)
        end

        subgraph local-tx1-wrapper[Local Tx Wrapper 1]
            tx-gateway3(Tx Gateway)
            local-tx1(Local Tx 1)
        end

        subgraph local-tx2-wrapper[Local Tx Wrapper 2]
            tx-gateway4(Tx Gateway)
            local-tx2(Local Tx 2)
        end

        subgraph bridge-tx2-wrapper[Egress Bridge Wrapper 2]
            tx-gateway2(Tx Gateway)
            bridge-tx2(Egress Bridge 2)
        end

        subgraph bridge-tx1-wrapper[Egress Bridge Wrapper 1]
            tx-gateway1(Tx Gateway)
            bridge-tx1(Egress Bridge 1)
        end
    end

    tx-app ==> tx-sdk ==> local-rx ==.transmit==> rx-gateway -.-> group
    group -.-> tx-gateway3 ==.do_receive==> local-tx1 ==> rx-sdk1 ==> rx-app1
    group -.-> tx-gateway4 ==.do_receive==> local-tx2 ==> rx-sdk2 ==> rx-app2
    group -.-> tx-gateway1 ==.do_receive==> bridge-tx1
    group -.-> tx-gateway2 ==.do_receive==> bridge-tx2

    style group fill:navy
    style rx-gateway fill:brown
    style tx-gateway1 fill:green
    style tx-gateway2 fill:green
    style tx-gateway3 fill:green
    style tx-gateway4 fill:green
```

### Case 1.2 – Receive external ST2110 media stream

```mermaid
flowchart LR
    rx-app1(Rx App 1)
    rx-sdk1(SDK)
    rx-app2(Rx App 2)
    rx-sdk2(SDK)

    subgraph Media Proxy
        subgraph group-boundaries[Multipoint Group]
            group(Zero-Copy
                  Engine)
        end

        subgraph bridge-rx-wrapper[Ingress Bridge Wrapper]
            bridge-rx(Ingress Bridge)
            rx-gateway(Rx Gateway)
        end

        subgraph local-tx1-wrapper[Local Tx Wrapper 1]
            tx-gateway3(Tx Gateway)
            local-tx1(Local Tx 1)
        end

        subgraph local-tx2-wrapper[Local Tx Wrapper 2]
            tx-gateway4(Tx Gateway)
            local-tx2(Local Tx 2)
        end

        subgraph bridge-tx2-wrapper[Egress Bridge Wrapper 2]
            tx-gateway2(Tx Gateway)
            bridge-tx2(Egress Bridge 2)
        end

        subgraph bridge-tx1-wrapper[Egress Bridge Wrapper 1]
            tx-gateway1(Tx Gateway)
            bridge-tx1(Egress Bridge 1)
        end
    end

    bridge-rx ==.transmit==> rx-gateway -.-> group
    group -.-> tx-gateway3 ==.do_receive==> local-tx1 ==> rx-sdk1 ==> rx-app1
    group -.-> tx-gateway4 ==.do_receive==> local-tx2 ==> rx-sdk2 ==> rx-app2
    group -.-> tx-gateway1 ==.do_receive==> bridge-tx1
    group -.-> tx-gateway2 ==.do_receive==> bridge-tx2

    style group fill:navy
    style rx-gateway fill:brown
    style tx-gateway1 fill:green
    style tx-gateway2 fill:green
    style tx-gateway3 fill:green
    style tx-gateway4 fill:green
```

## Development Phase 2 – Add Zero-Copy Support in SDK

### Case 2.1 – Receive external ST2110 media stream

```mermaid
flowchart LR
    rx-app1(Rx App 1)
    rx-sdk1(SDK)
    rx-app2(Rx App 2)
    rx-sdk2(SDK)

    subgraph Media Proxy
        subgraph group-boundaries[Multipoint Group]
            group(Zero-Copy
                  Engine)
        end

        subgraph bridge-rx-wrapper[Ingress Bridge Wrapper]
            bridge-rx(Ingress Bridge)
            rx-gateway(Rx Gateway)
        end

        subgraph bridge-tx2-wrapper[Egress Bridge Wrapper 2]
            tx-gateway2(Tx Gateway)
            bridge-tx2(Egress Bridge 2)
        end

        subgraph bridge-tx1-wrapper[Egress Bridge Wrapper 1]
            tx-gateway1(Tx Gateway)
            bridge-tx1(Egress Bridge 1)
        end

        local-tx1(Local Tx 1)
        local-tx2(Local Tx 2)
    end

    bridge-rx ==.transmit==> rx-gateway -.-> group
    group -.-> local-tx1 -..-> rx-sdk1 ==> rx-app1
    group -.-> local-tx2 -..-> rx-sdk2 ==> rx-app2
    group -.-> tx-gateway1 ==.do_receive==> bridge-tx1
    group -.-> tx-gateway2 ==.do_receive==> bridge-tx2

    style group fill:navy
    style rx-gateway fill:brown
    style tx-gateway1 fill:green
    style tx-gateway2 fill:green
    style rx-sdk1 fill:green
    style rx-sdk2 fill:green
    style local-tx1 fill:purple
    style local-tx2 fill:purple
```

### Case 2.2 – User App sends media stream

```mermaid
flowchart LR
    tx-app(Tx App)
    tx-sdk(SDK)
    rx-app1(Rx App 1)
    rx-sdk1(SDK)
    rx-app2(Rx App 2)
    rx-sdk2(SDK)

    subgraph Media Proxy
        subgraph group-boundaries[Multipoint Group]
            group(Zero-Copy
                  Engine)
        end

        local-rx(Local Rx)
        local-tx1(Local Tx 1)
        local-tx2(Local Tx 2)

        subgraph bridge-tx2-wrapper[Egress Bridge Wrapper 2]
            tx-gateway2(Tx Gateway)
            bridge-tx2(Egress Bridge 2)
        end

        subgraph bridge-tx1-wrapper[Egress Bridge Wrapper 1]
            tx-gateway1(Tx Gateway)
            bridge-tx1(Egress Bridge 1)
        end
    end

    tx-app ==> tx-sdk -.-> local-rx -.-> group
    group -.-> local-tx1 -..-> rx-sdk1 ==> rx-app1
    group -.-> local-tx2 -..-> rx-sdk2 ==> rx-app2
    group -.-> tx-gateway1 ==.do_receive==> bridge-tx1
    group -.-> tx-gateway2 ==.do_receive==> bridge-tx2

    style group fill:navy
    style tx-gateway1 fill:green
    style tx-gateway2 fill:green
    style tx-sdk fill:brown
    style rx-sdk1 fill:green
    style rx-sdk2 fill:green
    style local-tx1 fill:purple
    style local-tx2 fill:purple
    style local-rx fill:purple
```

## Development Phase 3 – Add Zero-Copy Support in Ingress Bridges

### Case 3.1 – Receive external ST2110 media stream

```mermaid
flowchart LR
    rx-app1(Rx App 1)
    rx-sdk1(SDK)
    rx-app2(Rx App 2)
    rx-sdk2(SDK)

    subgraph Media Proxy
        subgraph group-boundaries[Multipoint Group]
            group(Zero-Copy
                  Engine
                  *EBU MXL inside*)
        end

        local-tx1(Local Tx 1)
        local-tx2(Local Tx 2)
        
        subgraph bridge-rx-wrapper[Ingress Bridge Wrapper]
            bridge-rx(Ingress Bridge)
            rx-gateway(Rx Gateway)
        end

        subgraph bridge-tx2-wrapper[Egress Bridge Wrapper 2]
            tx-gateway2(Tx Gateway)
            bridge-tx2(Egress Bridge 2)
        end

        subgraph bridge-tx1-wrapper[Egress Bridge Wrapper 1]
            tx-gateway1(Tx Gateway)
            bridge-tx1(Egress Bridge 1)
        end
    end

    bridge-rx ==.allocate
                .transmit==> rx-gateway -.-> group
    group -.-> local-tx1 -..-> rx-sdk1 ==> rx-app1
    group -.-> local-tx2 -..-> rx-sdk2 ==> rx-app2
    group -.-> tx-gateway1 ==.do_receive==> bridge-tx1
    group -.-> tx-gateway2 ==.do_receive==> bridge-tx2

    style group fill:navy
    style rx-gateway fill:brown
    style tx-gateway1 fill:green
    style tx-gateway2 fill:green
    style rx-sdk1 fill:green
    style rx-sdk2 fill:green
    style bridge-rx fill:darkred
    style local-tx1 fill:purple
    style local-tx2 fill:purple
```

## Zero-Copy Connection Creation flow

```mermaid
sequenceDiagram
    autonumber

    participant sdk as SDK
    participant proxy as Media Proxy
    participant agent as Mesh Agent

    sdk->>proxy: CreateConnection RPC
    proxy->>agent: RegisterConnection RPC
    note over sdk: Wait for Connection Linked event
    agent->>proxy: ApplyConfig Command
    note over proxy: Create multipoint group
    note over proxy: Link connection to group
    note over proxy: Add conn_id to associations
    proxy->>sdk: Connection Linked event + Config
    note over sdk: Setup shared memory access
    sdk->>proxy: ActivateConnection RPC
    proxy->>sdk: Success
```

## Zero-Copy Connection Deletion flow

```mermaid
sequenceDiagram
    autonumber

    participant sdk as SDK
    participant proxy as Media Proxy
    participant agent as Mesh Agent

    note over sdk: Shutdown shared memory access
    sdk->>proxy: DeleteConnection RPC
    proxy->>agent: UnregisterConnection RPC
    note over proxy: Remove conn_id from associations
    proxy->>sdk: Success
    agent->>proxy: ApplyConfig Command
    note over proxy: Unlink connection from group
    note over proxy: Delete multipoint group if empty
    note over proxy: Erase multipoint group if no associations
```

## Class diagram

```mermaid
flowchart
    input[GroupInput]
    output[GroupOutput]
    zerooutput[ZeroCopyGroupOutput]
    output --> zerooutput
```
