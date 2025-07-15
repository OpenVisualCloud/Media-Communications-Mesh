# RDMA Configuration Testing Guide

## Overview

The RDMA test applications provide comprehensive testing capabilities for different RDMA providers, endpoint configurations, and performance scenarios. These tools help validate RDMA functionality and measure performance characteristics.

## Applications

### TxRDMATestApp - RDMA Transmitter Test
Tests RDMA transmission performance with configurable parameters.

### RxRDMATestApp - RDMA Receiver Test  
Tests RDMA reception performance with validation and measurement capabilities.

### rdma_benchmark.sh - Automated Benchmark Suite
Runs comprehensive RDMA performance benchmarks across multiple configurations.

## Basic Usage

### Simple TCP RDMA Test

**Receiver:**
```bash
sudo ./RxRDMATestApp --rdma-provider tcp --rdma-endpoints 2 --test-duration 30
```

**Transmitter:**
```bash
sudo ./TxRDMATestApp --rdma-provider tcp --rdma-endpoints 2 --test-duration 30
```

### High-Performance Verbs Test

**Receiver:**
```bash
sudo ./RxRDMATestApp \
    --rdma-provider verbs \
    --rdma-endpoints 8 \
    --enable-latency \
    --enable-verify \
    --output rx_verbs_results.txt
```

**Transmitter:**
```bash
sudo ./TxRDMATestApp \
    --rdma-provider verbs \
    --rdma-endpoints 8 \
    --packet-size 8192 \
    --burst-size 100 \
    --enable-latency \
    --output tx_verbs_results.txt
```

## Advanced Configuration

### Latency Optimization Test

```bash
# Low-latency configuration
sudo ./TxRDMATestApp \
    --rdma-provider verbs \
    --rdma-endpoints 1 \
    --packet-size 64 \
    --burst-size 1 \
    --burst-delay 0 \
    --queue-capacity 8 \
    --enable-latency \
    --test-duration 60
```

### Throughput Optimization Test

```bash
# High-throughput configuration
sudo ./TxRDMATestApp \
    --rdma-provider verbs \
    --rdma-endpoints 8 \
    --packet-size 65536 \
    --burst-size 200 \
    --burst-delay 100 \
    --queue-capacity 64 \
    --enable-throughput \
    --test-duration 120
```

### Data Integrity Test

```bash
# Receiver with pattern verification
sudo ./RxRDMATestApp \
    --enable-verify \
    --enable-loss \
    --pattern sequential \
    --dump received_data.bin \
    --output integrity_results.txt

# Transmitter with sequential pattern
sudo ./TxRDMATestApp \
    --pattern sequential \
    --packet-size 4096 \
    --burst-size 50
```

## Automated Benchmarking

### Quick Benchmark
```bash
sudo ./rdma_benchmark.sh --quick
```

### Full Performance Suite
```bash
sudo ./rdma_benchmark.sh \
    --duration 60 \
    --output results_$(date +%Y%m%d) \
    --verbose
```

### Custom Configuration Test
```bash
sudo ./rdma_benchmark.sh \
    --providers verbs \
    --endpoints 1,2,4,8 \
    --packet-sizes 1024,8192,65536 \
    --burst-sizes 1,10,100 \
    --duration 30
```

### Latency-Focused Benchmark
```bash
sudo ./rdma_benchmark.sh \
    --packet-sizes 64,256,1024 \
    --burst-sizes 1 \
    --endpoints 1,2 \
    --duration 60
```

## Configuration Parameters

### RDMA Options

| Parameter | Values | Description |
|-----------|--------|-------------|
| `--rdma-provider` | tcp, verbs | RDMA transport provider |
| `--rdma-endpoints` | 1-8 | Number of parallel endpoints |
| `--queue-capacity` | 8-128 | Buffer queue capacity |
| `--delay` | 0-5000 | Connection creation delay (ms) |

### Test Parameters

| Parameter | Values | Description |
|-----------|--------|-------------|
| `--test-duration` | 1-3600 | Test duration in seconds |
| `--packet-size` | 64-65536 | Packet size in bytes |
| `--burst-size` | 1-1000 | Packets per burst |
| `--burst-delay` | 0-100000 | Inter-burst delay (μs) |
| `--pattern` | sequential, random, zero | Test data pattern |

### Measurement Options

| Parameter | Description |
|-----------|-------------|
| `--enable-latency` | Enable latency measurements |
| `--enable-throughput` | Enable throughput measurements |
| `--enable-verify` | Enable pattern verification (RX) |
| `--enable-loss` | Enable packet loss tracking (RX) |
| `--enable-cpu` | Enable CPU usage monitoring |

## Performance Optimization

### For Low Latency

1. **Use fewer endpoints** (1-2) to reduce context switching
2. **Small packet sizes** (64-1024 bytes) for reduced processing time
3. **Single packet bursts** to minimize queuing delays
4. **Small buffer queues** (8-16) to reduce memory latency
5. **RDMA verbs provider** for hardware acceleration

Example:
```bash
sudo ./TxRDMATestApp \
    --rdma-provider verbs \
    --rdma-endpoints 1 \
    --packet-size 256 \
    --burst-size 1 \
    --queue-capacity 8
```

### For High Throughput

1. **Use multiple endpoints** (4-8) for parallelism
2. **Large packet sizes** (8192-65536 bytes) for efficiency
3. **Large burst sizes** (50-200) for sustained transfer rates
4. **Large buffer queues** (32-128) for buffering
5. **Optimized burst delays** to prevent buffer overflow

Example:
```bash
sudo ./TxRDMATestApp \
    --rdma-provider verbs \
    --rdma-endpoints 8 \
    --packet-size 32768 \
    --burst-size 100 \
    --queue-capacity 64 \
    --burst-delay 1000
```

### For Data Integrity

1. **Enable pattern verification** to detect corruption
2. **Enable packet loss tracking** to monitor reliability
3. **Use sequential patterns** for easy verification
4. **Dump received data** for offline analysis
5. **Monitor corruption rates** across configurations

Example:
```bash
sudo ./RxRDMATestApp \
    --enable-verify \
    --enable-loss \
    --pattern sequential \
    --dump integrity_test.bin
```

## Result Analysis

### Understanding Output Files

**TX Results (`tx_*_results.txt`):**
- `Packets Sent`: Total packets transmitted
- `Average Throughput`: Transmission throughput in Mbps
- `Average Latency`: Round-trip latency measurements

**RX Results (`rx_*_results.txt`):**
- `Packets Received`: Total packets received
- `Packet Loss Rate`: Percentage of lost packets
- `Corruption Rate`: Percentage of corrupted packets (if verification enabled)
- `Average Throughput`: Reception throughput in Mbps

**Benchmark Summary (`benchmark_summary.csv`):**
- Comparative results across all test configurations
- Sortable by throughput, latency, or loss rate

### Key Metrics

| Metric | Good Values | Poor Values | Optimization |
|--------|-------------|-------------|--------------|
| Latency | < 10 μs | > 100 μs | Reduce endpoints, packet size |
| Throughput | > 1000 Mbps | < 100 Mbps | Increase endpoints, packet size |
| Packet Loss | 0% | > 1% | Increase queue capacity, reduce burst rate |
| Corruption | 0% | > 0.1% | Check hardware, reduce transfer rate |

## Troubleshooting

### Common Issues

**"Permission denied" errors:**
- Run with `sudo` (RDMA requires root privileges)

**"Failed to create connection" errors:**
- Check if MCM control plane is running
- Verify RDMA hardware/drivers are available
- Try TCP provider if verbs fails

**High packet loss:**
- Increase buffer queue capacity
- Reduce burst size or add burst delays
- Check network infrastructure

**Poor performance with verbs:**
- Verify RDMA hardware is available (`ibstat`, `ibv_devices`)
- Check RDMA drivers are loaded
- Ensure proper network configuration

### System Requirements

**For TCP Provider:**
- Any Linux system with network connectivity
- MCM control plane running

**For Verbs Provider:**
- RDMA-capable network hardware (InfiniBand, RoCE, iWARP)
- RDMA drivers installed (`libibverbs`, device-specific drivers)
- Proper network configuration for RDMA

### Verification Commands

```bash
# Check RDMA devices
ibv_devices

# Check device capabilities  
ibv_devinfo

# Verify network connectivity
ping <target_ip>

# Check MCM control plane
ps aux | grep mcm
```

## Example Workflows

### 1. Basic Performance Validation

```bash
# Quick verification of both providers
sudo ./rdma_benchmark.sh --quick --verbose

# Review results
cat rdma_benchmark_results_*/benchmark_summary.csv
```

### 2. Latency Characterization

```bash
# Test latency across different configurations
sudo ./rdma_benchmark.sh \
    --packet-sizes 64,128,256,512,1024 \
    --burst-sizes 1 \
    --endpoints 1,2 \
    --duration 60 \
    --output latency_test

# Analyze latency results
grep "Average Latency" latency_test_*/rx_*_results.txt
```

### 3. Throughput Optimization

```bash
# Find optimal throughput configuration
sudo ./rdma_benchmark.sh \
    --packet-sizes 4096,8192,16384,32768,65536 \
    --burst-sizes 10,50,100,200 \
    --endpoints 4,8 \
    --duration 120 \
    --output throughput_test

# Find best performing configuration
sort -t, -k2 -nr throughput_test_*/benchmark_summary.csv | head -5
```

### 4. Reliability Testing

```bash
# Long-duration stability test
sudo ./RxRDMATestApp \
    --test-duration 3600 \
    --enable-verify \
    --enable-loss \
    --output stability_test.txt &

sudo ./TxRDMATestApp \
    --test-duration 3600 \
    --pattern sequential \
    --packet-size 8192 \
    --burst-size 50

# Check for any packet loss or corruption
grep -E "(Loss|Corruption)" stability_test.txt
```
