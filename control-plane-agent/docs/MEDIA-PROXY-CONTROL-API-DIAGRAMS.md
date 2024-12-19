# Media Proxy Control API (**Proxy API**)

* Mesh Agent runs a server to handle **Proxy API** requests.
* Media Proxy is a client of **Proxy API**.
* **Proxy API** is based on gRPC.
* Media Proxy starts a Command Queue to allow the Agent to send commands to Media Proxy.

## Register / Unregister Media Proxy at Agent

```mermaid
sequenceDiagram
    box MCM Media Proxy
        participant proxy as Media Proxy
        participant cmd as Command Processor thread
        participant tele as Telemetry thread
    end
    box MCM Control Plane Agent
        participant agent as Proxy API
        participant regproxy as Media Proxy Registry
    end

    note over proxy, regproxy: Media Proxy connects to Agent
    proxy ->>+ agent: rpc RegisterMediaProxy
    agent ->> regproxy: Add to Registry
    agent ->>- proxy: Success

    proxy ->>+ agent: rpc StartCommandQueue
    agent ->>- cmd: Returns Command Queue stream
    cmd ->> cmd: Read from Command Queue and execute commands
    cmd ->> proxy: Agent disconnects

    proxy ->>+ agent: rpc StartTelemetryQueue
    agent ->>- tele: Returns Telemetry Queue stream
    tele ->> tele: Periodically flush metrics to Telemetry Queue
    tele ->> proxy: Agent disconnects

    note over proxy, regproxy: Media Proxy disconnects from Agent
    proxy ->>+ agent: rpc UnregisterMediaProxy
    agent ->> regproxy: Remove from Registry
    agent ->>- proxy: Success
```

## Register New Connection / Unregister Connection

```mermaid
sequenceDiagram
    participant sdk as MCM SDK
    participant proxy as Media Proxy
    box MCM Control Plane Agent
        participant agent as Proxy API
        participant regconn as Connection Registry
        participant reggroup as Multipoint Group Registry
        participant regbridge as Bridge Registry
    end

    note over sdk, regbridge: Create Tx or Rx connection
    sdk ->>+ proxy: Create connection
    proxy ->>+ agent: rpc RegisterConnection
    agent ->> regconn: Add to Registry
    agent ->>+ proxy: rpc/command CreateMultipointGroup
    proxy ->>- agent: Success
    agent ->> reggroup: Add to Registry
    agent ->>+ proxy: rpc/command CreateBridge
    proxy ->>- agent: Success
    agent ->> regbridge: Add to Registry
    agent ->>- proxy: Success
    proxy ->>- sdk: Connection created

    note over sdk, regbridge: Delete connection
    sdk ->>+ proxy: Delete connection
    proxy ->>+ agent: rpc UnregisterConnection
    agent ->> regconn: Remove from Registry
    agent ->>+ proxy: rpc/command DeleteMultipointGroup
    proxy ->>- agent: Success
    agent ->> reggroup: Remove from Registry
    agent ->>+ proxy: rpc/command DeleteBridge
    proxy ->>- agent: Success
    agent ->> regbridge: Remove from Registry
    agent ->>- proxy: Success
    proxy ->>- sdk: Connection deleted
```
