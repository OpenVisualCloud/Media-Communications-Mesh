# Test Scenarios – Single Multipoint Group

The following scenarios allow to validate the Mesh transmission in a single Multipoint Group.

To validate it with multiple Multipoint Groups, run a combination of scenarios simultaneously, e.g. **Scenario 2.5** + **Scenario 4.3** + **Scenario 6.3**.

## **Scenarios 1.x** – Local Single Node Transmission (memif)

### **Scenario 1.1** – Single Node / Single Receiver — Blob, Video, Audio

```mermaid
flowchart
    subgraph Single Node
        tx((Tx App))
        rx((Rx App))
        proxy(Media Proxy)
    end
    tx --> proxy
    proxy --> rx
```

### **Scenario 1.2** – Single Node / 3x Receivers — Blob, Video, Audio

```mermaid
flowchart
    subgraph Single Node
        tx((Tx App))
        rx1((Rx App 1))
        rx2((Rx App 2))
        rx3((Rx App 3))
        proxy(Media Proxy)
    end
    tx --> proxy
    proxy --> rx1
    proxy --> rx2
    proxy --> rx3
```

## **Scenarios 2.x** – RDMA Transmission

### **Scenario 2.1** – 2x Nodes · _Direct Network Cable Connection_ / Single Receiver — Blob, Video, Audio

```mermaid
flowchart LR
    subgraph Node A
        tx((Tx App))
        proxy1(Media Proxy A)
    end
    subgraph Node B
        rxB1((Rx App))
        proxy2(Media Proxy B)
    end
    tx --> proxy1
    proxy1 -- RDMA --> proxy2
    proxy2 --> rxB1
```

### **Scenario 2.2** – 2x Nodes / Single Receiver — Blob, Video, Audio

```mermaid
flowchart LR
    subgraph Node A
        tx((Tx App))
        proxy1(Media Proxy A)
    end
    subgraph Node B
        rxB1((Rx App))
        proxy2(Media Proxy B)
    end
    sw(["Network
        Switch"])
    tx --> proxy1
    proxy1 -- RDMA --> sw
    sw -- RDMA --> proxy2
    proxy2 --> rxB1
```

### **Scenario 2.3** – 2x Nodes / 1x Receiver per Node — Blob, Video, Audio

```mermaid
flowchart LR
    subgraph Node B
        rxB1((Rx App 2))
        proxy2(Media Proxy B)
    end
    subgraph Node A
        rxA1((Rx App 1))
        tx((Tx App))
        proxy1(Media Proxy A)
    end
    sw(["Network
        Switch"])
    tx --> proxy1
    proxy1 --> rxA1
    proxy1 -- RDMA ----> sw
    sw -- RDMA --> proxy2
    proxy2 --> rxB1
```

### **Scenario 2.4** – 2x Nodes / 2x Receivers per Node — Blob, Video, Audio

```mermaid
flowchart LR
    subgraph Node B
        rxB1((Rx App 3))
        rxB2((Rx App 4))
        proxy2(Media Proxy B)
    end
    subgraph Node A
        tx((Tx App))
        rxA1((Rx App 1))
        rxA2((Rx App 2))
        proxy1(Media Proxy A)
    end
    sw(["Network
        Switch"])
    tx --> proxy1
    proxy1 --> rxA1
    proxy1 --> rxA2
    proxy1 -- RDMA ----> sw
    sw -- RDMA --> proxy2
    proxy2 --> rxB1
    proxy2 --> rxB2
```

### **Scenario 2.5** – 3x Nodes / 2x Receivers per Node — Blob, Video, Audio

```mermaid
flowchart LR
    subgraph Node C
        rxC1((Rx App 5))
        rxC2((Rx App 6))
        proxy3(Media Proxy C)
    end
    subgraph Node B
        rxB1((Rx App 3))
        rxB2((Rx App 4))
        proxy2(Media Proxy B)
    end
    subgraph Node A
        tx((Tx App))
        rxA1((Rx App 1))
        rxA2((Rx App 2))
        proxy1(Media Proxy A)
    end
    sw1(["Network
         Switch"])
    sw2(["Network
         Switch"])
    tx --> proxy1
    proxy1 --> rxA1
    proxy1 --> rxA2
    proxy1 -- RDMA ----> sw1
    proxy1 -- RDMA ----> sw2
    sw1 -- RDMA --> proxy2
    proxy2 --> rxB1
    proxy2 --> rxB2
    sw2 -- RDMA --> proxy3
    proxy3 --> rxC1
    proxy3 --> rxC2
```

## **Scenarios 3.x** – ST 2110 Outbound Transmission – Single Node

### **Scenario 3.1** – Single Node · _Direct Network Cable Connection_ — Video, Audio

```mermaid
flowchart LR
    subgraph Single Node
        tx((Tx App))
        proxy(Media Proxy)
    end
    dev("ST 2110 Compliant
         Input Device
         _(Monitor)_")
    tx --> proxy
    proxy -- ST 2110 --> dev
```

### **Scenario 3.2** – Single Node — Video, Audio

```mermaid
flowchart LR
    subgraph Single Node
        tx((Tx App))
        proxy(Media Proxy)
    end
    sw(["Network
        Switch"])
    dev("ST 2110 Compliant
         Input Device
         _(Monitor)_")
    tx --> proxy
    proxy -- ST 2110 --> sw
    sw -- ST 2110 --> dev
```

### **Scenario 3.3** – Single Node / Local Receiver — Video, Audio

```mermaid
flowchart LR
    subgraph Single Node
        tx((Tx App))
        rx((Rx App))
        proxy(Media Proxy)
    end
    sw(["Network
        Switch"])
    dev("ST 2110 Compliant
         Input Device
         _(Monitor)_")
    tx --> proxy
    proxy --> rx
    proxy -- ST 2110 ----> sw
    sw -- ST 2110 --> dev
```

## **Scenarios 4.x** – ST 2110 Outbound Transmission – Multiple Nodes

### **Scenario 4.1** – 2x Nodes / 1x Receiver per Node — Video, Audio

```mermaid
flowchart LR
    subgraph Node B
        rxB1((Rx App 2))
        proxy2(Media Proxy B)
    end
    subgraph Node A
        tx((Tx App))
        rxA1((Rx App 1))
        proxy1(Media Proxy A)
    end
    sw1(["Network
         Switch"])
    sw2(["Network
         Switch"])
    dev("ST 2110 Compliant
         Input Device
         _(Monitor)_")
    tx --> proxy1
    proxy1 --> rxA1
    proxy1 -- ST 2110 ----> sw1
    proxy1 -- RDMA ----> sw2
    sw2 -- RDMA --> proxy2
    proxy2 --> rxB1
    sw1 -- ST 2110 --> dev
```

### **Scenario 4.2** – 2x Nodes / 2x Receivers per Node — Video, Audio

```mermaid
flowchart LR
    subgraph Node B
        rxB1((Rx App 3))
        rxB2((Rx App 4))
        proxy2(Media Proxy B)
    end
    subgraph Node A
        tx((Tx App))
        rxA1((Rx App 1))
        rxA2((Rx App 2))
        proxy1(Media Proxy A)
    end
    sw1(["Network
         Switch"])
    sw2(["Network
         Switch"])
    dev("ST 2110 Compliant
         Input Device
         _(Monitor)_")
    tx --> proxy1
    proxy1 --> rxA1
    proxy1 --> rxA2
    proxy1 -- ST 2110 ----> sw1
    proxy1 -- RDMA ----> sw2
    sw2 -- RDMA --> proxy2
    proxy2 --> rxB1
    proxy2 --> rxB2
    sw1 -- ST 2110 --> dev
```

### **Scenario 4.3** – 3x Nodes / 2x Receivers per Node — Video, Audio

```mermaid
flowchart LR
    subgraph Node C
        rxC1((Rx App 5))
        rxC2((Rx App 6))
        proxy3(Media Proxy C)
    end
    subgraph Node B
        rxB1((Rx App 3))
        rxB2((Rx App 4))
        proxy2(Media Proxy B)
    end
    subgraph Node A
        tx((Tx App))
        rxA1((Rx App 1))
        rxA2((Rx App 2))
        proxy1(Media Proxy A)
    end
    sw1(["Network
          Switch"])
    sw2(["Network
          Switch"])
    sw3(["Network
          Switch"])
    dev("ST 2110 Compliant
         Input Device
         _(Monitor)_")
    tx --> proxy1
    proxy1 --> rxA1
    proxy1 --> rxA2
    proxy1 -- ST 2110 ----> sw1
    proxy1 -- RDMA ----> sw2
    sw2 -- RDMA --> proxy2
    proxy1 -- RDMA ----> sw3
    sw3 -- RDMA --> proxy3
    proxy2 --> rxB1
    proxy2 --> rxB2
    proxy3 --> rxC1
    proxy3 --> rxC2
    sw1 -- ST 2110 --> dev
```

## **Scenarios 5.x** – ST 2110 Inbound Transmission – Single Node

### **Scenario 5.1** – Single Node · _Direct Network Cable Connection_ / Single Receiver — Video, Audio

```mermaid
flowchart LR
    subgraph Single Node
        rx((Rx App))
        proxy(Media Proxy)
    end
    dev("ST 2110 Compliant
         Output Device
         _(Camera)_")
    dev -- ST 2110 --> proxy
    proxy --> rx
```

### **Scenario 5.2** – Single Node / Single Receiver — Video, Audio

```mermaid
flowchart LR
    subgraph Single Node
        rx((Rx App))
        proxy(Media Proxy)
    end
    dev("ST 2110 Compliant
         Output Device
         _(Camera)_")
    sw(["Network
        Switch"])
    dev -- ST 2110 --> sw
    sw -- ST 2110 --> proxy
    proxy --> rx
```

### **Scenario 5.3** – Single Node / 3x Receivers — Video, Audio

```mermaid
flowchart LR
    subgraph Single Node
        rx1((Rx App 1))
        rx2((Rx App 2))
        rx3((Rx App 3))
        proxy(Media Proxy)
    end
    dev("ST 2110 Compliant
         Output Device
         _(Camera)_")
    sw(["Network
        Switch"])
    dev -- ST 2110 --> sw
    sw -- ST 2110 --> proxy
    proxy --> rx1
    proxy --> rx2
    proxy --> rx3
```

## **Scenarios 6.x** – ST 2110 Inbound Transmission – Multiple Nodes

### **Scenario 6.1** – 2x Nodes / 1x Receiver per Node — Video, Audio

```mermaid
flowchart LR
    subgraph Node A
        rxA1((Rx App 1))
        proxy1(Media Proxy A)
    end
    subgraph Node B
        rxB1((Rx App 2))
        proxy2(Media Proxy B)
    end
    dev("ST 2110 Compliant
         Output Device
         _(Camera)_")
    sw(["Network
        Switch"])
    dev -- ST 2110 --> sw
    sw -- ST 2110 --> proxy1
    proxy1 --> rxA1
    proxy1 -- RDMA ----> proxy2
    proxy2 --> rxB1
```

### **Scenario 6.2** – 2x Nodes / 2x Receivers per Node — Video, Audio

```mermaid
flowchart LR
    subgraph Node A
        rxA1((Rx App 1))
        rxA2((Rx App 2))
        proxy1(Media Proxy A)
    end
    subgraph Node B
        rxB1((Rx App 3))
        rxB2((Rx App 4))
        proxy2(Media Proxy B)
    end
    dev("ST 2110 Compliant
         Output Device
         _(Camera)_")
    sw(["Network
        Switch"])
    dev -- ST 2110 --> sw
    sw -- ST 2110 --> proxy1
    proxy1 --> rxA1
    proxy1 --> rxA2
    proxy1 -- RDMA ----> proxy2
    proxy2 --> rxB1
    proxy2 --> rxB2
```

### **Scenario 6.3** – 3x Nodes / 2x Receivers per Node — Video, Audio

```mermaid
flowchart LR
    subgraph Node C
        rxC1((Rx App 5))
        rxC2((Rx App 6))
        proxy3(Media Proxy C)
    end
    subgraph Node B
        rxB1((Rx App 3))
        rxB2((Rx App 4))
        proxy2(Media Proxy B)
    end
    subgraph Node A
        rxA1((Rx App 1))
        rxA2((Rx App 2))
        proxy1(Media Proxy A)
    end
    dev("ST 2110 Compliant
         Output Device
         _(Camera)_")
    sw(["Network
        Switch"])
    dev -- ST 2110 --> sw
    sw -- ST 2110 --> proxy1
    proxy1 --> rxA1
    proxy1 --> rxA2
    proxy1 -- RDMA ----> proxy2
    proxy1 -- RDMA ----> proxy3
    proxy2 --> rxB1
    proxy2 --> rxB2
    proxy3 --> rxC1
    proxy3 --> rxC2
```
