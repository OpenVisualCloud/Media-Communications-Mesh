# Concept – New Architecture of Media Proxy to Support Multipoint Groups

## Connection (base class)
* Interface for Transmitting data – _may be implemented in the derived class_.
* Interface for Receiving data – _may be implemented in the derived class_.
* State – _When changes, triggers an Event_.
  * `Not Configured` – _After initialization._
  * `Configured`
  * `Establishing`
  * `Active`
  * `Suspended`
  * `Closing`
  * `Closed`
  * `Deleting`
* Operations
  * `Create` – _Performed by constructor_.
  * `Configure`
  * `Establish`
  * `Suspend` – _For test/debug purposes_.
  * `Resume` – _For test/debug purposes_.
  * `Shutdown`
  * `Delete` – _Initiated externally. Can't be performed by the Connection class_.
* Status – _When changes, triggers an Event_.
  * `Initial`
  * `Transition`
  * `Healthy`
  * `Failure`
  * `Shutdown`
* Base metrics
  * Throughput – _Move calculation to the Telemetry extractor level_.
  * Transaction counter.
  * Byte counter.
  * Error counter.
  * Status.
  * State.
  * Maciej suggested:
    * Backend load / capacity metric. Report example: `load: 85%` – _Secondary_.
    * Interface to enable/disable metrics by name.
  * Tomasz suggested:
    * Unified logger interface.
* Information
  * Created at.
  * Established at.
* Interface for sending metrics.
* Metrics Reset interface – _Access from MCM Agent, option to clear all or certain metrics_.

### Intermediary Layer: ST2110, RDMA, Local
* Base functionality of the specific backend.

### Backend Layer: (xxx)-Tx, (xxx)-Rx

* Transmitter or Receiver functionality of the specific backend.

## Connection Lifecycle State Diagram

```mermaid
stateDiagram
    notinit: Not Initialized
    configuring: Configuring
    configured: Configured
    establishing: Establishing
    active: Active
    suspended: Suspended
    closing: Closing
    closed: Closed
    deleting: Deleting

    [*] --> notinit
    notinit --> configuring: Configure
    configuring --> configured: Configure Succeeded
    configuring --> notinit: Configure Failed
    configured --> configuring: Configure
    configured --> establishing: Establish
    establishing --> active: Establish Succeeded
    establishing --> closed: Establish Failed
    active --> closing: Shutdown
    active --> suspended: Suspend
    suspended --> active: Resume
    suspended --> closing: Shutdown
    closing --> closed: Shutdown Completed
    closed --> deleting: Delete
    closed --> configuring: Configure
    closed --> establishing: Establish
    deleting --> [*]
```

## Diagram of Classes
```mermaid
flowchart
    point["**Connection** (or Point)
           _basic throuhput metrics_"]

    st["ST2110
       _base class_"]           

    sttx["ST2110 Tx
          _base class_"]           
    sttx20["ST2110-20 Tx
          "]           
    sttx22["ST2110-22 Tx
          "]           
    sttx30["ST2110-30 Tx
          "]           

    strx["ST2110 Rx
          _base class_"]           
    strx20["ST2110-20 Rx
          "]           
    strx22["ST2110-22 Rx
          "]           
    strx30["ST2110-30 Rx
          "]           

    rdma["RDMA
          _base class_"]           
    rdmatx["RDMA Tx
           "]           
    rdmarx["RDMA Rx
           "]           

    local["Local
           _base class_"]           
    localtx["Local Tx
            "]           
    localrx["Local Rx
            "]           

    multipoint[Multipoint Group]

    point --> st
    point ---> rdma
    point ---> local
    point ----> multipoint
    st --> strx
    st --> sttx
    sttx --> sttx20
    sttx --> sttx22
    sttx --> sttx30
    strx --> strx20
    strx --> strx22
    strx --> strx30
    rdma --> rdmatx
    rdma --> rdmarx
    local --> localtx
    local --> localrx
```

### Interface for Receiving data
* **Research** – Analyze the ST2110-XX Rx code and find how the new data appears.
* **Research** – Analyze the RDMA Rx code and find how the new data appears.
* **`DONE`** **Research** – Analyze the Memif Rx code and find how the new data appears.
* Identify how the interface for Receiving data can be defined.

### Interface for Transmitting data
* **Research** – Analyze the ST2110-XX Tx code and find how the data is sent.
* **Research** – Analyze the RDMA Tx code and find how the data is sent.
* **`DONE`** **Research** – Analyze the Memif Tx code and find how the data is sent.
* Identify how the interface for Transmitting data can be defined.

### Interface for sending metrics
* The base **Connection** class defines a virtual method that returns an array of metrics, each with a name, a type, and a values.
* The Telemetry engine periodically calls this method and forwards the returned metrics to MCM Agent.

### Metrics Reset interface
* MCM Agent sends a list of metric names to be reset.
* Counters and triggers can be reset using this interface.
* The base **Connection** class defines a virtual method that takes a list of metric names and resets them by name.
* Derived classes that have counters and triggers specific to a particular backend can override the method to reset additional metrics.

## Bridge
* Type
  * Ingress Bridge – _Inbound traffic to Multipoint Group_.
  * Egress Bridge – _Outbound traffic from Multipoint Group_.
* Bridge stores an identifier of a Connection class object.
* Status and state are taken directly from the linked Connection class object.

## Multipoint Group
* Stores a list of members in the group
* Every member is of **Connection** class type.
* One Producer/Importer – _Can be null_.
* Many Consumers/Exporters – _Can be null_.
* Can have no Producers but many Consumers.
* Can have one Producer and no Consumers.
* Can have no Producers and no Consumers – _Empty group_.
* Business logic
  * If no Producers or no Consumers, no traffic is passing. Status: `Inactive`.
  * 
* State – When changes, triggers an Event.
  * `Created` – _After initialization._
  * `Enabling`
  * `Enabled`
  * `Disabling`
  * `Disabled`
  * `Deleting`
* Operations
  * `Create` – _Performed by constructor_.
  * `Enable`
  * `Disable` – _For test/debug purposes_.
  * `Delete` – _Initiated externally. Can't be performed by the Multipoint Group class_.
* Status – When changes, triggers an Event.
  * `Healthy` – _Green, active traffic is passing_.
  * `Inactive` – _Yellow, no activity_.
  * `Failure` – _Red, no activity_.
* Base metrics
  * Throughput.
  * Transaction counter.
  * Byte counter.
  * Error counter.
  * Status.
  * State.
* Information
  * Created at.
  * Enabled at.

### Multipoint Group – Example 1
```mermaid
flowchart LR
    rx1[**ST2110-20Rx**
        _derived from
        Connection_]
    tx1[**LocalTx**
        _derived from
        Connection_]
    tx2[**LocalTx**
        _derived from
        Connection_]
    tx3[**ST2110-20Tx**
        _derived from
        Connection_]
    tx4[**RdmaTx**
        _derived from
        Connection_]
    logic[Zero Copy Engine]
    src[**Video Camera**
        Uncompressed Video
        ST2110-20]
    app1[User App 1]
    app2[User App 2]
    mon[**Video Monitor**
        Uncompressed Video
        ST2110-20]
    dest[Another Media Proxy]

    src -- Network --> rx1

    subgraph "Single Node"

      subgraph "Media Proxy"
          rx1 -- Link --> logic
          logic -- Link --> tx1
          logic -- Link --> tx2
          logic -- Link --> tx3
          logic -- Link --> tx4

          subgraph Multipoint Group
              logic
          end
      end

      tx1 -- Memif --> app1
      tx2 -- Memif --> app2
    end

    tx3 -- Network ----> mon
    tx4 -- Network ----> dest
```

### Multipoint Group – Example 2, 3, etc.
TBD