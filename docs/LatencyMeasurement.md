# End To End Latency Measurement — Media Communications Mesh

Before reading this document, please read [FFmpeg Plugin](FFmpegPlugin.md) to familiarize yourself with FFmpeg usage, as this document provides an extension. 

**Note:** Currently, latency measurement is available only for video transport.

## Build and install steps 

1. Please follow build and install steps from [FFmpeg Plugin](FFmpegPlugin.md). with one exception, FFmpeg configuration tool requires additional parameters.
   ```bash
   ./configure-ffmpeg.sh ${FFMPEG_VER} --enable-libfreetype --enable-libharfbuzz --enable-libfontconfig
   ```
## Time synchronization between hosts

__host-1 Controller clock__
```bash
sudo ptp4l -i <network_interface_1> -m 2 
```
__host-2 Worker clock__
```bash
sudo ptp4l -i <network_interface_2> -m 2 -s
sudo -s phc2sys -s <network_interface_2> -c CLOCK_REALTIME -O 0 -m
```
*Please note that `network_interface_1` and `network_interface_2` have to be physically connected to the same network


## Example – Run video transmission between 2 FFmpeg instances on the same host

This example demonstrates sending a video file from the 1st FFmpeg instance to the 2nd FFmpeg instance via Media Communications Mesh on the same host, with timestamps to measure end-to-end latency using the `drawtext` feature in FFmpeg. The `drawtext` filter allows you to overlay timestamps directly onto the video stream, providing a visual representation of latency as the video is processed and transmitted between instances/hosts.

### Run Mesh Agent
   ```bash
   mesh-agent
   ```

### Run Media Proxy

   ```bash
   sudo media_proxy        \
        -d 0000:32:01.1    \
        -i 192.168.96.11   \
        -r 192.168.97.11   \
        -p 9200-9299       \
        -t 8002
   ```

### Receiver side setup

   ```bash
   sudo MCM_MEDIA_PROXY_PORT=8002 ffmpeg                                     \
      -f mcm                                                                 \
          -conn_type multipoint-group                                        \
          -frame_rate 60                                                     \
          -video_size 1920x1080                                              \
          -pixel_format yuv422p10le                                          \
          -i -                                                               \
      -vf                                                                    \
          "drawtext=fontsize=50:                                             \
          text='Rx timestamp %{localtime\\:%H\\\\\:%M\\\\\:%S\\\\\:%3N}':    \
          x=10: y=70: fontcolor=white: box=1: boxcolor=black: boxborderw=10" \
      -vcodec mpeg4 -qscale:v 3 recv.mp4
   ```
### Sender side setup

   ```bash
   sudo MCM_MEDIA_PROXY_PORT=8002 ffmpeg -i <video-file-path>             \
   -vf                                                                    \
       "drawtext=fontsize=50:                                             \
       text='Tx timestamp %{localtime\\:%H\\\\\:%M\\\\\:%S\\\\\:%3N}':    \
       x=10: y=10: fontcolor=white: box=1: boxcolor=black: boxborderw=10" \
   -f mcm                                                                 \
      -conn_type multipoint-group                                         \
      -frame_rate 60                                                      \
      -video_size 1920x1080                                               \
      -pixel_format yuv422p10le -
   ```

   When sending a raw video file that lack metadata, you must explicitly provide FFmpeg
   with the necessary video frame details. This includes specifying the format
   `-f rawvideo`, pixel format `-pix_fmt`, and resolution `-s WxH`. Example:

   ```bash
   ffmpeg -f rawvideo -pix_fmt yuv422p10le -s 1920x1080 -i <video-file-path> ...
   ```
   To get meaningful latency measurement it is also recommended to provide rate `-readrate` at which FFmpeg will read frames from file. Example:
   ```bash
   ffmpeg -f rawvideo -readrate 2.4 -pix_fmt yuv422p10le -s 1920x1080 -i <video-file-path> ...
   ```
   Please note that `-readrate` value have to be calculated based on `-frame_rate` parameter, simple equation is as follow: 
   `readrate = frame_rate / 25`, for example:
   | frame_rate |      readrate     |
   |------------|-------------------|
   |    25      |   25 / 25 = 1     |
   |    50      |   50 / 25 = 2     |
   |    60      |  60 / 25 = 2.4    |


## Example – Run video transmission between 2 FFmpeg instances on different hosts

This example demonstrates sending a video file from the 1st FFmpeg instance to the 2nd FFmpeg instance via Media Communications Mesh, with timestamps to measure end-to-end latency using the `drawtext` feature in FFmpeg. The `drawtext` filter allows you to overlay timestamps directly onto the video stream, providing a visual representation of latency as the video is processed and transmitted between instances/hosts.

### Run Mesh Agent
   ```bash
   mesh-agent
   ```

### Receiver side setup

1. Start Media Proxy

   ```bash
   sudo media_proxy        \
        -d 0000:32:01.1    \
        -i 192.168.96.11   \
        -r 192.168.97.11   \
        -p 9200-9299       \
        -t 8002
   ```

1. Start FFmpeg to receive frames from Media Communications Mesh and save received data on hard drive

   ```bash
   sudo MCM_MEDIA_PROXY_PORT=8002 ffmpeg                                     \
      -f mcm                                                                 \
          -conn_type st2110                                                  \
          -transport st2110-20                                               \
          -ip_addr 192.168.96.10                                             \
          -port 9001                                                         \
          -frame_rate 60                                                     \
          -video_size 1920x1080                                              \
          -pixel_format yuv422p10le                                          \
          -i -                                                               \
      -vf                                                                    \
          "drawtext=fontsize=50:                                             \
          text='Rx timestamp %{localtime\\:%H\\\\\:%M\\\\\:%S\\\\\:%3N}':    \
          x=10: y=70: fontcolor=white: box=1: boxcolor=black: boxborderw=10" \
      -vcodec mpeg4 -qscale:v 3 recv.mp4
   ```

### Sender side setup

1. Start Media Proxy

   ```bash
   sudo media_proxy        \
        -d 0000:32:01.0    \
        -i 192.168.96.10   \
        -r 192.168.97.10   \
        -p 9100-9199       \
        -t 8001
   ```

2. Start FFmpeg to stream a video file to the receiver via Media Communications Mesh

   ```bash
   sudo MCM_MEDIA_PROXY_PORT=8001 ffmpeg -i <video-file-path>             \
   -vf                                                                    \
       "drawtext=fontsize=50:                                             \
       text='Tx timestamp %{localtime\\:%H\\\\\:%M\\\\\:%S\\\\\:%3N}':    \
       x=10: y=10: fontcolor=white: box=1: boxcolor=black: boxborderw=10" \
   -f mcm                                                                 \
      -conn_type st2110                                                   \
      -transport st2110-20                                                \
      -ip_addr 192.168.96.11                                              \
      -port 9001                                                          \
      -frame_rate 60                                                      \
      -video_size 1920x1080                                               \
      -pixel_format yuv422p10le -
   ```

## Stream postprocessing
The generated stream can be analyzed manually, but it is a long process. To accelerate it, there is a small sript written in Python that automatically extracts and plots latency.

1. install Install Tesseract OCR
   ```bash
      apt install tesseract-ocr
   ```
2. Install Python packages
   ```bash
      pip install opencv-python
      pip install pytesseract
      pip install matplotlib
   ```
2. Postprocess stream with command
   ```bash
      python text-detection.py <input_video_file> <output_image_name>
   ```
   ```bash
      python text-detection.py recv.mp4 latency_chart.jpg
   ```
When preparing FFmpeg command if you change parameters of `drawtext` filter, especialy `fontsize`, `x`, `y` or `text`, you have to adjust script __text-detection.py__ too, please refer to function `extract_text_from_region(image, x, y, font_size, length)`


<!-- References -->
[license-img]: https://img.shields.io/badge/License-BSD_3--Clause-blue.svg
[license]: https://opensource.org/license/bsd-3-clause
