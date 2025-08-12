# Blob Integrity Checker

This module provides blob (binary data) integrity checking functionality for the Media Communications Mesh project. It's designed to work alongside the existing video integrity checker and integrates with the `integrity_runner.py` framework.

## Overview

The blob integrity checker compares binary files chunk by chunk using MD5 hashes to detect any data corruption or transmission errors. It supports two modes of operation:

1. **File mode**: For single blob files
2. **Stream mode**: For blob data segmented into multiple files

## Features

- **Chunk-based verification**: Compares files in configurable chunks for detailed error detection
- **Full file hash comparison**: Quick integrity check using complete file MD5 hashes
- **Multi-processing support**: Parallel processing for stream mode with configurable worker count
- **Detailed logging**: Comprehensive logging of integrity check results and errors
- **Flexible configuration**: Configurable chunk sizes, output paths, and processing options

## Files

- `blob_integrity.py`: Main integrity checking module
- `test_blob_integrity.py`: Test suite and usage examples
- `integrity_runner.py`: Extended with blob integrity runner classes

## Usage

### Command Line Interface

#### Single File Mode

```bash
python blob_integrity.py file source.bin output_name --chunk_size 1048576 --output_path /mnt/ramdisk
```

#### Stream Mode (for segmented files)

```bash
python blob_integrity.py stream source.bin output_prefix --chunk_size 1048576 --output_path /mnt/ramdisk --segment_duration 3 --workers 5
```

### Programmatic Usage with integrity_runner.py

#### File Blob Integrity

```python
from integrity_runner import FileBlobIntegrityRunner

runner = FileBlobIntegrityRunner(
    host=host_connection,
    test_repo_path="/path/to/repo",
    src_url="/path/to/source.bin",
    out_name="output.bin",
    chunk_size=1048576,  # 1MB chunks
    out_path="/mnt/ramdisk",
    delete_file=True
)

runner.setup()
success = runner.run()
```

#### Stream Blob Integrity

```python
from integrity_runner import StreamBlobIntegrityRunner

runner = StreamBlobIntegrityRunner(
    host=host_connection,
    test_repo_path="/path/to/repo",
    src_url="/path/to/source.bin",
    out_name="output_prefix",
    chunk_size=1048576,
    segment_duration=3,
    workers=5
)

runner.setup()
runner.run()
# ... later ...
success = runner.stop_and_verify()
```

### Direct Class Usage

```python
import logging
from blob_integrity import BlobFileIntegrator

logger = logging.getLogger(__name__)

integrator = BlobFileIntegrator(
    logger=logger,
    src_url="/path/to/source.bin",
    out_name="output.bin",
    chunk_size=1048576,
    out_path="/mnt/ramdisk",
    delete_file=False
)

success = integrator.check_blob_integrity()
```

## Generating Test Blob Data

You can generate test blob data using the `dd` command:

```bash
# Generate 100MB of random data
dd if=/dev/random of=test_blob.bin bs=1M count=100

# Generate 1GB of random data
dd if=/dev/random of=large_blob.bin bs=1M count=1024

# For faster generation, use /dev/urandom
dd if=/dev/urandom of=test_blob.bin bs=1M count=100
```

## Configuration Options

### Command Line Arguments

- `--chunk_size`: Size of chunks for processing in bytes (default: 1048576 = 1MB)
- `--output_path`: Directory where output files are located (default: /mnt/ramdisk)
- `--delete_file`/`--no_delete_file`: Whether to delete files after processing
- `--segment_duration`: Time to wait between file checks in stream mode (default: 3 seconds)
- `--workers`: Number of worker processes for stream mode (default: 5)

### Class Parameters

- `chunk_size`: Chunk size for processing (bytes)
- `delete_file`: Whether to delete output files after successful verification
- `out_path`: Output directory to monitor
- `segment_duration`: Polling interval for new files in stream mode
- `workers_count`: Number of parallel worker processes

## Testing

Run the test suite to verify functionality:

```bash
python test_blob_integrity.py
```

This will:
1. Generate test blob files
2. Test normal integrity checking
3. Test corruption detection
4. Report results

## Integration with Media Communications Mesh

The blob integrity checker is designed to work with the MCM validation framework:

1. **Stream Mode**: For continuous blob data transmission where data is segmented into files
2. **File Mode**: For single blob file transfers
3. **Runner Integration**: Works with the existing `integrity_runner.py` framework
4. **Logging**: Integrates with MCM logging standards
5. **Error Handling**: Follows MCM error reporting patterns

## Error Detection

The checker can detect:
- **Data corruption**: Changes in file content
- **Size mismatches**: Files with different sizes
- **Missing chunks**: Incomplete file transfers
- **Ordering issues**: Files with incorrect chunk sequences

## Performance Considerations

- **Chunk Size**: Larger chunks reduce memory usage but may miss small corruptions
- **Worker Count**: More workers speed up processing but increase resource usage
- **Hash Algorithm**: MD5 is used for speed; consider SHA256 for higher security requirements
- **File I/O**: Large files may require tuning of chunk sizes for optimal performance

## Dependencies

- Python 3.8+
- hashlib (standard library)
- multiprocessing (standard library)
- pathlib (standard library)

No external dependencies required (unlike video integrity which needs OpenCV and Tesseract).
