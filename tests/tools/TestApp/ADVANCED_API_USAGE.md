# Advanced API Features Testing Guide

This guide describes the usage of advanced API testing applications for comprehensive validation of callback handling, error recovery, memory management, and performance profiling in the Media Communications Mesh SDK.

## Overview

The advanced API testing suite provides deep validation of:
- **Callback and Event Handling**: Testing asynchronous event processing, callback latency, and thread safety
- **Error Injection and Recovery**: Validating error handling mechanisms, recovery procedures, and system resilience
- **Memory Management**: Testing memory allocation, leak detection, fragmentation analysis, and boundary validation
- **Performance Profiling**: Comprehensive performance monitoring, trend analysis, and resource utilization tracking

## Applications

### CallbackTestApp - Callback and Event Handling
Tests callback mechanisms, event processing, and thread safety validation.

### ErrorInjectionApp - Error Injection and Recovery
Validates error handling, recovery mechanisms, and system resilience under failure conditions.

### MemoryTestApp - Memory Management Validation
Tests memory allocation patterns, leak detection, fragmentation analysis, and boundary checking.

### PerformanceProfileApp - Performance Profiling
Comprehensive performance monitoring with CPU, memory, I/O, and network profiling capabilities.

## Basic Usage

### Callback Testing
```bash
# Basic callback functionality test
sudo ./CallbackTestApp --mode callbacks --payload video

# Event handling stress test
sudo ./CallbackTestApp --mode events --stress-test --trigger-errors

# Combined callback and event testing with latency measurement
sudo ./CallbackTestApp --mode combined --measure-latency --output callback_results.txt
```

### Error Injection Testing
```bash
# Test all error types with recovery validation
sudo ./ErrorInjectionApp --error-type all --validate-recovery

# Network error simulation
sudo ./ErrorInjectionApp --error-type network --simulate-network-loss --injection-interval 3000

# Memory error testing with OOM simulation
sudo ./ErrorInjectionApp --error-type memory --simulate-oom --recovery-timeout 60000
```

### Memory Management Testing
```bash
# Memory leak detection
sudo ./MemoryTestApp --mode leak --track-leaks --test-duration 300

# Memory fragmentation analysis
sudo ./MemoryTestApp --mode fragmentation --stress-patterns --allocation-rate 500

# Comprehensive memory stress test
sudo ./MemoryTestApp --mode stress --max-allocations 50000 --validate-alignment
```

### Performance Profiling
```bash
# CPU performance profiling
sudo ./PerformanceProfileApp --mode cpu --enable-cpu --sample-interval 500

# Comprehensive performance analysis
sudo ./PerformanceProfileApp --mode all --detailed-analysis --csv performance.csv

# Throughput-focused profiling
sudo ./PerformanceProfileApp --mode throughput --enable-network --test-duration 600
```

## Advanced Configuration

### Callback Testing Scenarios

#### Thread Safety Validation
```bash
# Multi-threaded callback validation
sudo ./CallbackTestApp --mode callbacks --validate-threading --stress-test --test-duration 180

# High-frequency event processing
sudo ./CallbackTestApp --mode events --stress-test --payload audio --measure-latency
```

#### Callback Latency Analysis
```bash
# Precise latency measurement
sudo ./CallbackTestApp --mode combined --measure-latency --payload video --output latency_analysis.txt

# Stress test with latency monitoring
sudo ./CallbackTestApp --stress-test --measure-latency --trigger-errors --test-duration 300
```

### Error Recovery Testing

#### Connection Resilience
```bash
# Connection error recovery testing
sudo ./ErrorInjectionApp --error-type connection --validate-recovery --injection-interval 10000 --recovery-timeout 30000

# Network failure simulation
sudo ./ErrorInjectionApp --error-type network --simulate-network-loss --test-duration 600 --output network_recovery.txt
```

#### Timeout Handling
```bash
# Timeout condition simulation
sudo ./ErrorInjectionApp --error-type timeout --simulate-timeouts --injection-interval 5000

# All error types with comprehensive recovery
sudo ./ErrorInjectionApp --error-type all --validate-recovery --simulate-oom --simulate-network-loss --simulate-timeouts
```

### Memory Management Analysis

#### Leak Detection and Analysis
```bash
# Comprehensive leak detection
sudo ./MemoryTestApp --mode leak --track-leaks --test-boundaries --validate-alignment --output memory_leaks.txt

# Long-running leak analysis
sudo ./MemoryTestApp --mode all --track-leaks --test-duration 1800 --allocation-rate 200
```

#### Fragmentation Testing
```bash
# Memory fragmentation stress test
sudo ./MemoryTestApp --mode fragmentation --stress-patterns --max-allocations 100000

# Boundary validation testing
sudo ./MemoryTestApp --mode stress --test-boundaries --validate-alignment --allocation-rate 1000
```

### Performance Monitoring

#### CPU and Memory Profiling
```bash
# Detailed CPU profiling
sudo ./PerformanceProfileApp --mode cpu --enable-cpu --sample-interval 250 --detailed-analysis

# Memory usage monitoring
sudo ./PerformanceProfileApp --mode memory --enable-memory --enable-cpu --test-duration 900
```

#### Network and I/O Analysis
```bash
# Network performance profiling
sudo ./PerformanceProfileApp --mode all --enable-network --enable-io --csv network_performance.csv

# High-frequency sampling for detailed analysis
sudo ./PerformanceProfileApp --mode all --sample-interval 100 --detailed-analysis --test-duration 300
```

## Common Options

### CallbackTestApp Options
- `--mode <type>`: callbacks, events, combined
- `--trigger-errors`: Force error conditions for testing
- `--stress-test`: Enable high frequency stress testing
- `--validate-threading`: Enable thread safety validation
- `--measure-latency`: Measure callback latency
- `--test-duration <sec>`: Test duration in seconds
- `--output <file>`: Save test results to file

### ErrorInjectionApp Options
- `--error-type <type>`: connection, memory, network, timeout, all
- `--injection-interval <ms>`: Error injection interval
- `--recovery-timeout <ms>`: Recovery timeout
- `--validate-recovery`: Enable recovery validation
- `--simulate-oom`: Simulate out-of-memory conditions
- `--simulate-network-loss`: Simulate network failures
- `--simulate-timeouts`: Simulate timeout conditions

### MemoryTestApp Options
- `--mode <type>`: leak, fragmentation, stress, all
- `--allocation-rate <rate>`: Allocations per second
- `--max-allocations <count>`: Maximum concurrent allocations
- `--track-leaks`: Enable leak detection
- `--validate-alignment`: Check memory alignment
- `--test-boundaries`: Test buffer boundaries
- `--stress-patterns`: Enable stress test patterns

### PerformanceProfileApp Options
- `--mode <type>`: throughput, latency, cpu, memory, all
- `--sample-interval <ms>`: Profiling sample interval
- `--enable-cpu`: Enable CPU profiling
- `--enable-memory`: Enable memory profiling
- `--enable-io`: Enable I/O profiling
- `--enable-network`: Enable network profiling
- `--detailed-analysis`: Enable detailed analysis
- `--csv <file>`: Save results in CSV format

## Test Scenarios

### 1. Callback Performance Validation
Test callback mechanisms under various conditions:
```bash
# Basic callback functionality
sudo ./CallbackTestApp --mode callbacks --payload video --test-duration 120 --output callback_basic.txt

# High-stress callback testing
sudo ./CallbackTestApp --mode combined --stress-test --measure-latency --trigger-errors --test-duration 300 --output callback_stress.txt

# Thread safety validation
sudo ./CallbackTestApp --validate-threading --stress-test --payload audio --test-duration 180 --output callback_threading.txt
```

### 2. Error Recovery Validation
Test system resilience and recovery mechanisms:
```bash
# Comprehensive error recovery testing
sudo ./ErrorInjectionApp --error-type all --validate-recovery --injection-interval 8000 --recovery-timeout 45000 --test-duration 600 --output error_recovery.txt

# Network resilience testing
sudo ./ErrorInjectionApp --error-type network --simulate-network-loss --validate-recovery --test-duration 300 --output network_resilience.txt

# Memory error handling
sudo ./ErrorInjectionApp --error-type memory --simulate-oom --validate-recovery --injection-interval 15000 --output memory_errors.txt
```

### 3. Memory Management Validation
Test memory allocation, leak detection, and fragmentation:
```bash
# Comprehensive memory testing
sudo ./MemoryTestApp --mode all --track-leaks --validate-alignment --test-boundaries --test-duration 600 --output memory_comprehensive.txt

# Memory leak hunting
sudo ./MemoryTestApp --mode leak --track-leaks --allocation-rate 100 --test-duration 900 --output memory_leaks.txt

# Fragmentation stress test
sudo ./MemoryTestApp --mode fragmentation --stress-patterns --max-allocations 75000 --output memory_fragmentation.txt
```

### 4. Performance Profiling
Monitor system performance under various loads:
```bash
# Complete performance profile
sudo ./PerformanceProfileApp --mode all --detailed-analysis --sample-interval 500 --test-duration 600 --csv complete_profile.csv --output performance_summary.txt

# CPU-focused profiling
sudo ./PerformanceProfileApp --mode cpu --enable-cpu --sample-interval 250 --detailed-analysis --test-duration 300 --output cpu_profile.txt

# Throughput analysis
sudo ./PerformanceProfileApp --mode throughput --enable-network --enable-io --detailed-analysis --test-duration 900 --csv throughput_analysis.csv
```

### 5. Combined Advanced Testing
Run multiple tests together for comprehensive validation:
```bash
# Background performance monitoring during callback testing
sudo ./PerformanceProfileApp --mode all --sample-interval 1000 --test-duration 600 --csv background_perf.csv &
PERF_PID=$!

sudo ./CallbackTestApp --mode combined --stress-test --measure-latency --test-duration 300 --output callback_with_perf.txt

kill $PERF_PID
```

## Result Analysis

### Callback Test Results
The callback test results include:
- Event processing statistics (connection, data, buffer, status events)
- Callback latency measurements (min/avg/max)
- Thread safety analysis
- Error handling validation

Example analysis:
```bash
# Check callback performance
grep "Event Rate" callback_*.txt

# Analyze latency trends
grep "Latency" callback_*.txt | sort -n

# Verify thread safety
grep "Thread safety" callback_*.txt
```

### Error Recovery Results
Error injection results include:
- Error injection statistics by type
- Recovery success rates and timing
- System availability measurements
- Resilience analysis

Example analysis:
```bash
# Check recovery success rates
grep "Recovery Success Rate" error_*.txt

# Analyze error injection rates
grep "Error Injection Rate" error_*.txt

# System availability analysis
grep "System Availability" error_*.txt
```

### Memory Test Results
Memory test results include:
- Allocation/deallocation statistics
- Memory leak detection
- Fragmentation analysis
- Boundary violation detection

Example analysis:
```bash
# Check for memory leaks
grep "Memory Leaks" memory_*.txt

# Analyze allocation success rates
grep "Allocation Success Rate" memory_*.txt

# Memory fragmentation analysis
grep "Fragmentation Ratio" memory_*.txt
```

### Performance Profile Results
Performance results include:
- CPU usage trends and statistics
- Memory usage patterns
- Network throughput measurements
- I/O performance metrics

Example analysis:
```bash
# CPU usage analysis
grep "CPU usage" performance_*.txt

# Memory trend analysis
grep "Memory Trend" performance_*.txt

# Throughput performance
grep "Throughput" performance_*.txt
```

## Troubleshooting

### Common Issues

1. **Callback Test Failures**
   - Check thread safety violations
   - Verify callback registration
   - Monitor callback latency spikes

2. **Error Recovery Problems**
   - Increase recovery timeout
   - Check error injection frequency
   - Verify recovery mechanisms

3. **Memory Test Issues**
   - Monitor system memory limits
   - Check for actual memory leaks vs. test artifacts
   - Verify memory alignment requirements

4. **Performance Issues**
   - Check system resource availability
   - Verify sampling interval settings
   - Monitor for performance degradation trends

### Debug Options
Enable verbose logging for detailed troubleshooting:
```bash
sudo ./CallbackTestApp --verbose --mode combined
sudo ./ErrorInjectionApp --verbose --error-type all
sudo ./MemoryTestApp --verbose --mode stress
sudo ./PerformanceProfileApp --verbose --mode all
```

## Best Practices

1. **Test Planning**: Run tests in order of increasing complexity
2. **Resource Monitoring**: Monitor system resources during testing
3. **Result Analysis**: Save all results for comparison and trend analysis
4. **Environment**: Ensure consistent test environment conditions
5. **Duration**: Allow sufficient test duration for meaningful results
6. **Isolation**: Run memory-intensive tests separately to avoid interference

This advanced API testing framework provides comprehensive validation of the Media Communications Mesh SDK's robustness, performance characteristics, and reliability under various stress conditions.
