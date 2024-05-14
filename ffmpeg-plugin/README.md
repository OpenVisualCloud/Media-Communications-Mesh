# FFmpeg plugin for MCM

## Build

### Prerequitites
Install dependencies and build MCM as described in the top level README.md, paragraph "Basic Installation".

### Build flow

1. Clone the FFmpeg repository and apply MCM patches
   ```bash
   ./clone-and-patch-ffmpeg.sh
   ```

1. Run the FFmpeg configuration tool
   ```bash
   ./configure-ffmpeg.sh
   ```

1. Build FFmpeg with the MCM plugin
   ```bash
   ./build-ffmpeg.sh
   ```

### Run

TBD
