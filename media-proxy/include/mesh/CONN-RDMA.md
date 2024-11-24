Mermaid flow of RdmaRx:

```mermaid
flowchart TD
    start([Start]) --> init[RdmaRx Constructor]
    init --> config[configure]
    config -->|Success| establish[on_establish]
    config -->|Failure| close1[Set state to CLOSED]
    establish -->|Initialized| alreadyInit[Return error_already_initialized]
    alreadyInit --> |Continue| Continue_flow
    establish -->|Not Initialized| devInit[rdma_init]
    devInit -->|Success| epInit[ep_init]
    devInit -->|Failure| close2[Set state to CLOSED]
    epInit -->|Success| allocBuffer[allocate_buffer]
    epInit -->|Failure| close3[Set state to CLOSED]
    allocBuffer -->|Success| epConfig[configure_endpoint]
    allocBuffer -->|Failure| cleanup1[cleanup_resources]
    epConfig -->|Success| startThread[start_thread]
    epConfig -->|Failure| cleanup2[cleanup_resources]
    startThread -->|Success| active[Set state to ACTIVE]
    startThread -->|Failure| cleanup3[cleanup_resources]
    cleanup1 --> close4[Set state to CLOSED]
    cleanup2 --> close5[Set state to CLOSED]
    cleanup3 --> close6[Set state to CLOSED]

    active --> frameThread[frame_thread]
    frameThread -->|Loop Until Canceled| processBuf[process_buffers]
    processBuf --> handleBuf[handle_rdma_cq]
    handleBuf -->|Success| processBuf
    handleBuf -->|Failure| cleanup4[cleanup_resources]
    cleanup4 --> close7[Set state to CLOSED]

    style start fill:#8fbc8f,stroke:#333,stroke-width:2px
    style active fill:#ffdd99,stroke:#333,stroke-width:2px
    style close1 fill:#ff9999,stroke:#333,stroke-width:2px
    style close2 fill:#ff9999,stroke:#333,stroke-width:2px
    style close3 fill:#ff9999,stroke:#333,stroke-width:2px
    style close4 fill:#ff9999,stroke:#333,stroke-width:2px
    style close5 fill:#ff9999,stroke:#333,stroke-width:2px
    style close6 fill:#ff9999,stroke:#333,stroke-width:2px
    style close7 fill:#ff9999,stroke:#333,stroke-width:2px
```

-------------------------------------------------------------------------------

Mermaid flow of RdmaRx frame_thread:

```mermaid
flowchart TD
    start([Start Frame Thread]) --> loopStart[Loop Until Canceled]
    loopStart -->|If cancelled| cleanup3[Log Cancellation and Cleanup Resources]
    
    loopStart --> bufIter[Iterate Over Buffers]
    bufIter -->|Buffer Allocated| cqRead[Read From Completion Queue]
    bufIter -->|Buffer Not Allocated| skipBuffer[Skip This Buffer]
    skipBuffer --> bufIter

    cqRead -->|CQ Event Found| process[Process CQ Event]
    cqRead -->|No CQ Event| retry[Wait/Retry]
    retry --> cqRead

    process -->|Success| passToHandler[Pass Buffer to handle_rdma_cq]
    process -->|Failure| cleanup1[Log Error and Cleanup Resources]

    passToHandler -->|handle_rdma_cq Success| continueLoop[Continue to Next Buffer]
    passToHandler -->|handle_rdma_cq Failure| cleanup2[Log Error and Cleanup Resources]
    
    cleanup1 --> exitThread[Set State to CLOSED and Exit]
    cleanup2 --> exitThread
    cleanup3 --> exitThread
    
    continueLoop --> bufIter
    exitThread --> threadEnd([End Frame Thread])

    style start fill:#8fbc8f,stroke:#333,stroke-width:2px
    style threadEnd fill:#8fbc8f,stroke:#333,stroke-width:2px
    style retry fill:#f0e68c,stroke:#333,stroke-width:2px
    style process fill:#ffdd99,stroke:#333,stroke-width:2px
    style cleanup1 fill:#ff9999,stroke:#333,stroke-width:2px
    style cleanup2 fill:#ff9999,stroke:#333,stroke-width:2px
    style cleanup3 fill:#ff9999,stroke:#333,stroke-width:2px
    style exitThread fill:#ff9999,stroke:#333,stroke-width:2px
```

-------------------------------------------------------------------------------

Mermaid flow of RdmaTx frame_thread:

```mermaid
flowchart TD
    start([Start Frame Thread]) --> loopStart[Loop Until Canceled]
    loopStart -->|If Cancelled| cleanup3[Log Cancellation and Cleanup Resources]

    loopStart --> bufIter[Iterate Over Buffers]
    bufIter -->|Buffer Ready| prepareTx[Prepare Buffer for Transmission]
    bufIter -->|Buffer Not Ready| skipBuffer[Skip This Buffer]
    skipBuffer --> bufIter

    prepareTx --> cqRead[Read From Completion Queue]
    cqRead -->|CQ Event Found| transmit[Transmit Data]
    cqRead -->|No CQ Event| retry[Wait/Retry]
    retry --> cqRead

    transmit -->|Success| passToHandler[Pass Buffer to handle_rdma_cq]
    transmit -->|Failure| cleanup1[Log Error and Cleanup Resources]

    passToHandler -->|handle_rdma_cq Success| continueLoop[Continue to Next Buffer]
    passToHandler -->|handle_rdma_cq Failure| cleanup2[Log Error and Cleanup Resources]

    cleanup1 --> exitThread[Set State to CLOSED and Exit]
    cleanup2 --> exitThread
    cleanup3 --> exitThread

    continueLoop --> bufIter
    exitThread --> threadEnd([End Frame Thread])

    style start fill:#8fbc8f,stroke:#333,stroke-width:2px
    style threadEnd fill:#8fbc8f,stroke:#333,stroke-width:2px
    style retry fill:#f0e68c,stroke:#333,stroke-width:2px
    style prepareTx fill:#ffdd99,stroke:#333,stroke-width:2px
    style transmit fill:#ffdd99,stroke:#333,stroke-width:2px
    style cleanup1 fill:#ff9999,stroke:#333,stroke-width:2px
    style cleanup2 fill:#ff9999,stroke:#333,stroke-width:2px
    style cleanup3 fill:#ff9999,stroke:#333,stroke-width:2px
    style exitThread fill:#ff9999,stroke:#333,stroke-width:2px
```

