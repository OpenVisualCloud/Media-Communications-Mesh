# Multipoint Group Testing Guide

This guide describes the usage of the multipoint group test applications for testing group communication scenarios with the Media Communications Mesh SDK.

## Overview

The multipoint group test applications support testing of group communication topologies including:
- **Mesh networks**: All nodes communicate with all other nodes
- **Star topologies**: Central hub with multiple spokes
- **Ring topologies**: Circular communication patterns

These applications provide comprehensive testing of:
- Group membership and discovery
- Synchronization across multiple nodes
- Data pattern verification
- Quality of Service (QoS) settings
- Performance monitoring and analysis

## Applications

### TxMultipointApp - Group Transmitter
The transmitter application creates and manages a multipoint group, sending data to group members.

### RxMultipointApp - Group Receiver  
The receiver application joins a multipoint group and receives data from group members.

## Basic Usage

### Simple Group Communication
```bash
# Start receiver (joins group)
sudo ./RxMultipointApp --group broadcast_test --node receiver1

# Start transmitter (creates group)
sudo ./TxMultipointApp --group broadcast_test --node transmitter1 --packets 1000
```

### Multi-Node Mesh Network
```bash
# Node 1 (transmitter and receiver)
sudo ./TxMultipointApp --group mesh_network --node node1 --node-id 1 --topology mesh &
sudo ./RxMultipointApp --group mesh_network --node node1 --node-id 1 --topology mesh &

# Node 2 (transmitter and receiver)  
sudo ./TxMultipointApp --group mesh_network --node node2 --node-id 2 --topology mesh &
sudo ./RxMultipointApp --group mesh_network --node node2 --node-id 2 --topology mesh &

# Node 3 (transmitter and receiver)
sudo ./TxMultipointApp --group mesh_network --node node3 --node-id 3 --topology mesh &
sudo ./RxMultipointApp --group mesh_network --node node3 --node-id 3 --topology mesh &
```

### Star Topology with Central Hub
```bash
# Central hub (receives from all spokes)
sudo ./RxMultipointApp --group star_network --node hub --node-id 0 --topology star

# Spoke nodes (send to hub)
sudo ./TxMultipointApp --group star_network --node spoke1 --node-id 1 --topology star --hub-id 0 &
sudo ./TxMultipointApp --group star_network --node spoke2 --node-id 2 --topology star --hub-id 0 &
sudo ./TxMultipointApp --group star_network --node spoke3 --node-id 3 --topology star --hub-id 0 &
```

## Synchronization Testing

### Frame Rate Synchronization
```bash
# Synchronized video transmission at 30 fps
sudo ./TxMultipointApp --group sync_test --payload-type video --frame-rate 30 --enable-sync --sync-tolerance 5

# Receiver with sync analysis
sudo ./RxMultipointApp --group sync_test --payload-type video --frame-rate 30 --enable-sync
```

### Audio Synchronization  
```bash
# Audio group with precise timing
sudo ./TxMultipointApp --group audio_sync --payload-type audio --frame-rate 1000 --enable-sync --sync-tolerance 1

# Audio receiver with sync monitoring
sudo ./RxMultipointApp --group audio_sync --payload-type audio --frame-rate 1000 --enable-sync
```

## Advanced Configuration

### QoS Settings
```bash
# High priority reliable transmission
sudo ./TxMultipointApp --group priority_test --priority high --reliability reliable

# Best effort low priority
sudo ./TxMultipointApp --group bulk_test --priority low --reliability best_effort
```

### Group Management
```bash
# Large group with heartbeat monitoring
sudo ./TxMultipointApp --group large_test --max-size 16 --heartbeat-interval 1000 --enable-heartbeat

# Receiver with member tracking
sudo ./RxMultipointApp --group large_test --max-size 16 --heartbeat-timeout 3000 --enable-tracking
```

### Data Pattern Verification
```bash
# Transmitter with sequential pattern
sudo ./TxMultipointApp --group verify_test --pattern sequential --enable-verify

# Receiver with pattern checking
sudo ./RxMultipointApp --group verify_test --enable-verify --enable-tracking
```

## Performance Testing

### Throughput Testing
```bash
# High throughput video transmission
sudo ./TxMultipointApp --group perf_test --payload-type video --width 3840 --height 2160 --fps 60 --packets 10000 --enable-stats

# Performance monitoring receiver
sudo ./RxMultipointApp --group perf_test --enable-tracking --enable-sync --test-duration 300 --output perf_results.txt
```

### Latency Testing
```bash
# Low latency blob transmission
sudo ./TxMultipointApp --group latency_test --payload-type blob --size 1024 --frame-rate 1000 --priority high --enable-timing

# Latency measurement receiver
sudo ./RxMultipointApp --group latency_test --enable-sync --timeout 10 --output latency_results.txt
```

## Common Options

### Transmitter Options
- `--group <name>`: Group identifier
- `--node <name>`: Node name within group
- `--node-id <id>`: Unique node ID (0-255)
- `--topology <type>`: mesh, star, ring
- `--payload-type <type>`: video, audio, blob
- `--packets <count>`: Number of packets to send
- `--frame-rate <fps>`: Transmission frame rate
- `--enable-sync`: Enable synchronization features
- `--priority <level>`: QoS priority (low, normal, high)
- `--reliability <mode>`: best_effort, reliable
- `--pattern <type>`: Data pattern (random, sequential, broadcast)
- `--enable-heartbeat`: Send periodic heartbeats
- `--enable-stats`: Collect performance statistics

### Receiver Options
- `--group <name>`: Group to join
- `--node <name>`: This node's name
- `--node-id <id>`: This node's unique ID
- `--topology <type>`: Expected topology
- `--test-duration <sec>`: How long to receive
- `--timeout <ms>`: Receive timeout
- `--enable-tracking`: Track group members
- `--enable-sync`: Analyze synchronization
- `--enable-verify`: Verify data patterns
- `--heartbeat-timeout <ms>`: Member timeout
- `--output <file>`: Save results to file
- `--dump <file>`: Dump received data

## Example Scenarios

### 1. Video Broadcast Testing
Test video streaming to multiple receivers:
```bash
# Video transmitter
sudo ./TxMultipointApp --group video_broadcast --payload-type video --width 1920 --height 1080 --fps 30 --packets 1800 --output tx_video.txt

# Multiple receivers
sudo ./RxMultipointApp --group video_broadcast --test-duration 60 --enable-tracking --output rx_video_1.txt &
sudo ./RxMultipointApp --group video_broadcast --test-duration 60 --enable-tracking --output rx_video_2.txt &
sudo ./RxMultipointApp --group video_broadcast --test-duration 60 --enable-tracking --output rx_video_3.txt &
```

### 2. Audio Conference Testing
Test multi-party audio communication:
```bash
# Each participant sends and receives
for i in {1..4}; do
    sudo ./TxMultipointApp --group audio_conf --node participant$i --node-id $i --payload-type audio --topology mesh --enable-heartbeat &
    sudo ./RxMultipointApp --group audio_conf --node participant$i --node-id $i --topology mesh --enable-tracking &
done
```

### 3. Reliable Data Distribution
Test reliable data delivery with verification:
```bash
# Reliable transmitter with verification
sudo ./TxMultipointApp --group reliable_data --payload-type blob --size 8192 --reliability reliable --pattern sequential --enable-verify --packets 1000

# Receivers with pattern verification
sudo ./RxMultipointApp --group reliable_data --enable-verify --enable-tracking --test-duration 120 --output reliable_rx1.txt &
sudo ./RxMultipointApp --group reliable_data --enable-verify --enable-tracking --test-duration 120 --output reliable_rx2.txt &
```

### 4. Performance Stress Testing
Test high load scenarios:
```bash
# High throughput transmitter
sudo ./TxMultipointApp --group stress_test --payload-type video --width 3840 --height 2160 --fps 60 --packets 18000 --priority high --enable-stats --output stress_tx.txt

# Multiple receivers for load testing
for i in {1..8}; do
    sudo ./RxMultipointApp --group stress_test --node stress_rx$i --enable-tracking --test-duration 300 --output stress_rx$i.txt &
done
```

## Result Analysis

The output files contain detailed statistics including:
- Packet counts and throughput
- Group membership information
- Synchronization analysis
- Pattern verification results
- Timing and latency measurements

Example output analysis:
```bash
# Compare receiver performance
grep "Average Throughput" stress_rx*.txt

# Check synchronization quality
grep "Sync Drift" *_sync.txt

# Verify data integrity
grep "Pattern Errors" *verify*.txt
```

## Troubleshooting

### Common Issues

1. **Group Discovery Failures**
   - Ensure all nodes use the same group name
   - Check network connectivity between nodes
   - Verify MCM agent is running

2. **Synchronization Problems**
   - Check network latency between nodes
   - Adjust sync tolerance settings
   - Verify frame rate settings match

3. **Performance Issues**
   - Check system resources (CPU, memory)
   - Verify network bandwidth
   - Consider lowering frame rates or quality

### Debug Options
Add verbose logging for troubleshooting:
```bash
sudo ./TxMultipointApp --verbose --group debug_test
sudo ./RxMultipointApp --verbose --group debug_test
```

## Best Practices

1. **Node IDs**: Assign unique node IDs (0-255) to avoid conflicts
2. **Timing**: Allow receivers to start before transmitters
3. **Resources**: Monitor system resources during high load tests
4. **Network**: Ensure sufficient network bandwidth for all streams
5. **Results**: Save results to files for analysis and comparison
6. **Cleanup**: Stop applications gracefully to ensure proper cleanup

This testing framework provides comprehensive validation of multipoint group communication capabilities in the Media Communications Mesh SDK.
