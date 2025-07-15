# Protocol & Format Coverage Testing

This document describes the Priority 5 testing applications that provide comprehensive protocol and format coverage testing for the Media Communications Mesh SDK.

## Overview

Priority 5 focuses on **Protocol & Format Coverage** testing through four specialized applications:

1. **ProtocolTestApp** - Extended protocol support testing
2. **FormatValidationApp** - Format validation and testing 
3. **CodecTestApp** - Codec-specific testing
4. **ContainerValidationApp** - Container format validation

## Applications

### 1. ProtocolTestApp

Tests extended protocol support including UDP, TCP, RDMA, multicast, and custom protocols.

#### Features
- **Multi-Protocol Support**: UDP, TCP, RDMA, multicast, custom protocols
- **Transport Modes**: Unicast, multicast, broadcast
- **Packet Testing**: Multiple sizes, fragmentation, reordering, duplication
- **Validation**: Checksums, sequencing, encryption
- **Performance Metrics**: Latency, throughput, packet loss, jitter

#### Usage Examples
```bash
# Test all UDP packet sizes with fragmentation
./ProtocolTestApp --protocol udp --test-fragmentation --packet-sizes 1500,4096,8192,16384

# Test multicast with encryption
./ProtocolTestApp --protocol multicast --enable-encryption --transport multicast

# Test custom protocol with reordering
./ProtocolTestApp --protocol custom --test-reordering --custom-headers "CustomProto: v1.0"

# Comprehensive protocol testing
./ProtocolTestApp --protocol all --test-fragmentation --test-reordering --validate-checksums
```

#### Configuration Options
- `--protocol <type>`: UDP, TCP, RDMA, multicast, custom, all
- `--mode <type>`: unicast, multicast, broadcast
- `--packet-sizes <sizes>`: Comma-separated packet sizes
- `--bandwidth <mbps>`: Target bandwidth
- `--test-fragmentation`: Enable fragmentation testing
- `--test-reordering`: Enable packet reordering tests
- `--validate-checksums`: Enable checksum validation
- `--enable-encryption`: Enable encryption testing

### 2. FormatValidationApp

Validates media format headers, metadata, and structure across multiple codecs and containers.

#### Features
- **Format Support**: H.264, H.265, AV1, VP9, JPEG, AAC, Opus
- **Header Validation**: NAL units, OBU headers, format-specific structures
- **Metadata Validation**: Format metadata and parameters
- **Corruption Testing**: Corruption detection and handling
- **Sample Generation**: Generate valid format samples

#### Usage Examples
```bash
# Test all video codecs with header validation
./FormatValidationApp --format video --codecs h264,h265,av1 --validate-headers

# Test container formats with deep validation
./FormatValidationApp --format container --containers mp4,ts,mkv --deep-validation

# Test custom files with corruption detection
./FormatValidationApp --test-files sample1.mp4,sample2.mkv --test-corruption

# Comprehensive format testing
./FormatValidationApp --format all --validate-headers --validate-metadata --test-corruption
```

#### Configuration Options
- `--format <type>`: video, audio, container, custom, all
- `--codecs <list>`: Comma-separated codec list
- `--containers <list>`: Comma-separated container list
- `--validate-headers`: Enable header validation
- `--validate-metadata`: Enable metadata validation
- `--test-corruption`: Enable corruption testing
- `--generate-samples`: Generate format samples

### 3. CodecTestApp

Performs codec-specific testing including encoding/decoding performance, quality metrics, and parameter validation.

#### Features
- **Codec Support**: H.264, H.265, AV1, VP9, JPEG, AAC, Opus
- **Performance Testing**: Encoding/decoding speed, throughput
- **Quality Metrics**: PSNR, SSIM, bitrate accuracy
- **Parameter Testing**: Multiple resolutions, frame rates, quality levels
- **Mode Testing**: Different encoding presets and profiles

#### Usage Examples
```bash
# Test H.264 with multiple quality levels
./CodecTestApp --codec h264 --test-quality-levels --profile baseline

# Test H.265 with all resolutions and framerates
./CodecTestApp --codec h265 --test-resolutions --test-framerates --preset slow

# Test AV1 with specific configuration
./CodecTestApp --codec av1 --width 3840 --height 2160 --fps 60 --bitrate 20000

# Comprehensive codec testing
./CodecTestApp --codec all --test-quality-levels --test-resolutions --validate-output
```

#### Configuration Options
- `--codec <type>`: h264, h265, av1, vp9, jpeg, aac, opus, all
- `--profile <profile>`: Codec profile (baseline, main, high)
- `--preset <preset>`: Encoding preset (ultrafast, fast, medium, slow)
- `--width/height <pixels>`: Video dimensions
- `--fps <fps>`: Frame rate
- `--bitrate <kbps>`: Target bitrate
- `--test-quality-levels`: Test multiple quality levels
- `--test-resolutions`: Test multiple resolutions
- `--validate-output`: Validate encoded output

### 4. ContainerValidationApp

Validates container format structure, metadata, and streaming operations.

#### Features
- **Container Support**: MP4, MPEG-TS, MKV, AVI, MOV, WebM
- **Structure Validation**: Container headers, box/atom structure
- **Metadata Validation**: Track information, timestamps, duration
- **Operation Testing**: Seeking, muxing, demuxing
- **Corruption Detection**: Corrupted container handling

#### Usage Examples
```bash
# Test MP4 containers with deep analysis
./ContainerValidationApp --container mp4 --deep-analysis --validate-structure

# Test all container formats with seeking
./ContainerValidationApp --container all --test-seeking --test-muxing

# Test specific files with corruption detection
./ContainerValidationApp --test-files sample1.mp4,sample2.mkv --test-corruption

# Comprehensive container testing
./ContainerValidationApp --container all --validate-structure --test-seeking --deep-analysis
```

#### Configuration Options
- `--container <type>`: mp4, ts, mkv, avi, mov, webm, all
- `--validate-structure`: Enable structure validation
- `--validate-metadata`: Enable metadata validation
- `--test-seeking`: Enable seeking functionality testing
- `--test-muxing`: Enable muxing operation testing
- `--test-corruption`: Enable corruption detection testing
- `--deep-analysis`: Enable deep container analysis

## Test Scenarios

### Protocol Coverage Testing
```bash
# UDP Performance Testing
./ProtocolTestApp --protocol udp --bandwidth 1000 --packet-sizes 1500,9000 --test-duration 300

# RDMA Validation
./ProtocolTestApp --protocol rdma --validate-checksums --test-fragmentation

# Multicast Group Testing
./ProtocolTestApp --protocol multicast --transport multicast --test-reordering
```

### Format Validation Testing
```bash
# Video Codec Validation
./FormatValidationApp --format video --validate-headers --test-corruption --generate-samples

# Audio Format Testing
./FormatValidationApp --format audio --codecs aac,opus --validate-metadata

# Container Structure Analysis
./FormatValidationApp --format container --deep-validation --test-files *.mp4
```

### Codec Performance Testing
```bash
# H.264 Encoding Performance
./CodecTestApp --codec h264 --test-quality-levels --test-resolutions --validate-output

# AV1 Advanced Testing
./CodecTestApp --codec av1 --preset slow --test-encoding-modes --deep-validation

# Multi-Codec Comparison
./CodecTestApp --codec all --width 1920 --height 1080 --fps 30 --test-duration 600
```

### Container Operation Testing
```bash
# MP4 Operations
./ContainerValidationApp --container mp4 --test-seeking --test-muxing --test-demuxing

# MPEG-TS Validation
./ContainerValidationApp --container ts --validate-structure --test-corruption

# Multi-Container Testing
./ContainerValidationApp --container all --generate-samples --deep-analysis
```

## Advanced Configuration

### Custom Protocol Testing
```json
{
  "connection": {
    "protocol": "custom",
    "customHeaders": "X-Custom-Protocol: v2.0",
    "validation": {
      "checksums": true,
      "sequencing": true,
      "encryption": true
    }
  }
}
```

### Format Validation Configuration
```json
{
  "formatValidation": {
    "enableHeaders": true,
    "enableMetadata": true,
    "enableCorruption": true,
    "deepValidation": true
  },
  "supportedCodecs": ["h264", "h265", "av1"],
  "testFiles": ["sample1.mp4", "sample2.mkv"]
}
```

### Codec Testing Configuration
```json
{
  "codec": {
    "type": "h265",
    "profile": "main",
    "preset": "medium"
  },
  "video": {
    "width": 3840,
    "height": 2160,
    "fps": 60,
    "bitrate": 20000
  },
  "testing": {
    "qualityLevels": true,
    "resolutions": true,
    "encodingModes": true
  }
}
```

## Performance Metrics

### Protocol Testing Metrics
- **Throughput**: Actual vs target bandwidth
- **Latency**: Min/max/average packet latency
- **Packet Loss**: Loss rate and recovery
- **Jitter**: Latency variation
- **Error Rates**: Protocol and validation errors

### Format Validation Metrics
- **Success Rate**: Valid vs invalid formats
- **Processing Time**: Format parsing performance
- **Error Detection**: Corruption and malformation detection
- **Sample Quality**: Generated sample validation

### Codec Testing Metrics
- **Encoding Performance**: Frames per second, encoding time
- **Decoding Performance**: Decoding speed and efficiency
- **Quality Metrics**: PSNR, SSIM, visual quality
- **Compression Efficiency**: Bitrate vs quality ratio

### Container Testing Metrics
- **Validation Success**: Structure and metadata validation
- **Operation Performance**: Seeking, muxing, demuxing speed
- **Error Handling**: Corruption detection and recovery
- **Analysis Depth**: Deep structure analysis results

## Troubleshooting

### Common Issues

1. **Protocol Errors**
   - Check network configuration and permissions
   - Verify protocol support and parameters
   - Monitor packet loss and corruption

2. **Format Validation Failures**
   - Verify format specifications and headers
   - Check for corrupted or malformed data
   - Review codec compatibility

3. **Codec Performance Issues**
   - Check CPU and memory resources
   - Verify codec parameters and settings
   - Monitor encoding/decoding efficiency

4. **Container Validation Problems**
   - Verify container format specifications
   - Check for structural integrity
   - Review metadata consistency

### Debug Options
- Use `--verbose` flag for detailed logging
- Enable specific validation components
- Monitor system resources during testing
- Review output files for detailed analysis

## Integration

These Priority 5 applications integrate with the existing TestApp suite and can be combined with other priorities for comprehensive testing:

```bash
# Combined protocol and performance testing
./ProtocolTestApp --protocol udp --test-duration 300 --output protocol_results.txt
./PerformanceProfileApp --test-duration 300 --output performance_results.txt

# Format validation with error injection
./FormatValidationApp --format all --test-corruption
./ErrorInjectionApp --error-type all --validate-recovery

# Codec testing with memory validation
./CodecTestApp --codec h264 --test-quality-levels
./MemoryTestApp --test-duration 300 --detect-leaks
```

This completes Priority 5 implementation, providing comprehensive protocol and format coverage testing capabilities for the Media Communications Mesh SDK.
