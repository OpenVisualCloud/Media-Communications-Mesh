# Media Communications Mesh

Documentation for FFmpeg Media Communications Mesh (further: Mesh) plugin.

> 📝 _**Notice** Check more powerful FFmpeg build [Intel® Tiber™ Broadcast Suite](https://github.com/OpenVisualCloud/Intel-Tiber-Broadcast-Suite), as it includes Media Communication MEsh FFmpeg plugin and lots more_

> 📝 _**Notice** Not all options are yet available. The strikethrough ~~option~~ indicates being under development_

> ⚠️ _**Important** FFmpeg plugin requires Media Communications Mesh standalone and some additional steps to run properly._

## FFmpeg Media Communications Mesh Muxer Parameters Table

### Muxer plugin available params

| Parameter      | Description                                               | Type             | Default               | Possible Values                                  |
|----------------|-----------------------------------------------------------|------------------|-----------------------|--------------------------------------------------|
| `ip_addr`      | Set the remote IP address to which the media data will be sent. | String           | `"192.168.96.2"`      |                                                  |
| `port`         | Set the remote port to which the media data will be sent. | String           | `"9001"`              |                                                  |
| `payload_type` | Set the payload type for the media data.                 | String           | `"st20"`              | `"st20"`, `"st22"`, ~~`"st30"`~~, ~~`"st40"`~~, ~~`"rtsp"`~~ |
| `protocol_type`| Set the protocol type for the media data transmission.   | String           | `"auto"`              | `"memif"`, `"udp"`, `"tcp"`, ~~`"http"`~~, ~~`"grpc"`~~, `"auto"` |
| `video_size`   | Set the video frame size.                                | Image size (String) | `"1920x1080"`       |                                                  |
| `pixel_format` | Set the video pixel format.                              | Pixel format (Enum) | `AV_PIX_FMT_YUV422P10LE` | ~~`AV_PIX_FMT_NV12`~~, `AV_PIX_FMT_YUV422P`, `AV_PIX_FMT_YUV444P10LE`, `AV_PIX_FMT_RGB24`, `AV_PIX_FMT_YUV422P10LE` |
| `frame_rate`   | Set the video frame rate.                                | Video rate (String or Rational) | `"25"` |                                                  |
| `socket_name`  | Set the memif socket name for the media data transmission. | String           | `NULL`                |                                                  |
| `interface_id` | Set the interface ID for the media data transmission.    | Integer          | `0`                   | `-1` to `INT_MAX` |

### Demuxer plugin available params

| Parameter      | Description                                               | Type             | Default               | Possible Values                                  |
|----------------|-----------------------------------------------------------|------------------|-----------------------|--------------------------------------------------|
| `ip_addr`      | Set the remote IP address from which the media data will be received. | String           | `"192.168.96.1"`      |                                                  |
| `port`         | Set the local port on which the media data will be received. | String           | `"9001"`              |                                                  |
| `payload_type` | Set the payload type for the media data.                 | String           | `"st20"`              | `"st20"`, `"st22"`, ~~`"st30"`~~, ~~`"st40"`~~, ~~`"rtsp"`~~ |
| `protocol_type`| Set the protocol type for the media data reception.      | String           | `"auto"`              | `"memif"`, `"udp"`, `"tcp"`, `"http"`, `"grpc"`, `"auto"` |
| `video_size`   | Set the video frame size.                                | Image size (String) | `"1920x1080"`       |                                                  |
| `pixel_format` | Set the video pixel format.                              | Pixel format (Enum) | `AV_PIX_FMT_YUV422P10LE` | ~~`AV_PIX_FMT_NV12`~~, `AV_PIX_FMT_YUV422P`, `AV_PIX_FMT_YUV444P10LE`, `AV_PIX_FMT_RGB24`, `AV_PIX_FMT_YUV422P10LE` |
| `frame_rate`   | Set the video frame rate.                                | Video rate (String or Rational) | `"25"` |                                                  |
| `socket_name`  | Set the memif socket name for the media data reception.  | String           | `NULL`                |                                                  |
| `interface_id` | Set the interface ID for the media data reception.       | Integer          | `0`                   |                                                  |

## FFmpeg Media Communications Mesh Muxer Plugin Documentation

The Mesh's Muxer plugin for FFmpeg is designed to handle the transmission of media data over a network using various protocols and payload types. Below are the input parameters that can be configured for the Mesh's Muxer plugin.

### Input Parameters

#### `ip_addr`
- **Description**: Set the remote IP address to which the media data will be sent.
- **Type**: String
- **Default**: `"192.168.96.2"`
- **Flags**: Encoding parameter

#### `port`
- **Description**: Set the remote port to which the media data will be sent.
- **Type**: String
- **Default**: `"9001"`
- **Flags**: Encoding parameter

#### `payload_type`
- **Description**: Set the payload type for the media data.
- **Type**: String
- **Default**: `"st20"`
- **Flags**: Encoding parameter
- **Possible Values**: `"st20"`, `"st22"`, ~~`"st30"`~~, ~~`"st40"`~~, ~~`"rtsp"`~~

#### `protocol_type`
- **Description**: Set the protocol type for the media data transmission.
- **Type**: String
- **Default**: `"auto"`
- **Flags**: Encoding parameter
- **Possible Values**: `"memif"`, `"udp"`, `"tcp"`, ~~`"http"`~~, ~~`"grpc"`~~, `"auto"`

#### `video_size`
- **Description**: Set the video frame size using a string such as `"640x480"` or `"hd720"`.
- **Type**: Image size (String)
- **Default**: `"1920x1080"`
- **Flags**: Encoding parameter

#### `pixel_format`
- **Description**: Set the video pixel format.
- **Type**: Pixel format (Enum)
- **Default**: `AV_PIX_FMT_YUV422P10LE`
- **Flags**: Encoding parameter
- **Possible Values**: ~~`AV_PIX_FMT_NV12`~~, `AV_PIX_FMT_YUV422P`, `AV_PIX_FMT_YUV444P10LE`, `AV_PIX_FMT_RGB24`, `AV_PIX_FMT_YUV422P10LE`

#### `frame_rate`
- **Description**: Set the video frame rate.
- **Type**: Video rate (String or Rational)
- **Default**: `"25"`
- **Flags**: Encoding parameter

#### `socket_name`
- **Description**: Set the memif socket name for the media data transmission.
- **Type**: String
- **Default**: `NULL`
- **Flags**: Encoding parameter

#### `interface_id`
- **Description**: Set the interface ID for the media data transmission.
- **Type**: Integer
- **Default**: `0`
- **Flags**: Encoding parameter
- **Range**: `-1` to `INT_MAX`

### Example Usage

To use the Mesh's Muxer plugin with FFmpeg, pass the `-f mcm` flag and parameters. Here is an example command that sets some of the parameters:

```bash
ffmpeg -i input.mp4 -c:v rawvideo -f mcm -ip_addr 192.168.1.100 -port 8000 -payload_type st20 -pixel_format yuv422p10le -protocol_type auto -video_size hd720 -frame_rate 30 -socket_name my_socket -interface_id 2 output.mcm
```

This command takes an `input.mp4` input file, processes the video as `rawvideo`, uses the Mesh's Muxer to send the data to `192.168.1.100` IP address on port `8000` with `st20` payload type, protocol type `auto`, `hd720` video size, `30` frames per second (frame rate), onto socket name `my_socket` with socket's interface ID set to `2`. Output is saved to `output.mcm` file.

## FFmpeg Media Communications Mesh Demuxer Plugin

The Mesh's Demuxer plugin for FFmpeg is designed to handle the reception of media data over a network using various protocols and payload types. Below are the input parameters that can be configured for the Mesh's Demuxer plugin.

### Input Parameters

#### `ip_addr`
- **Description**: Set the remote IP address from which the media data will be received.
- **Type**: String
- **Default**: `"192.168.96.1"`
- **Flags**: Decoding parameter

#### `port`
- **Description**: Set the local port on which the media data will be received.
- **Type**: String
- **Default**: `"9001"`
- **Flags**: Decoding parameter

#### `payload_type`
- **Description**: Set the payload type for the media data.
- **Type**: String
- **Default**: `"st20"`
- **Flags**: Decoding parameter
- **Possible Values**: `"st20"`, `"st22"`, ~~`"st30"`~~, ~~`"st40"`~~, ~~`"rtsp"`~~

#### `protocol_type`
- **Description**: Set the protocol type for the media data reception.
- **Type**: String
- **Default**: `"auto"`
- **Flags**: Decoding parameter
- **Possible Values**: `"memif"`, `"udp"`, `"tcp"`, `"http"`, `"grpc"`, `"auto"`

#### `video_size`
- **Description**: Set the video frame size using a string such as `"640x480"` or `"hd720"`.
- **Type**: Image size (String)
- **Default**: `"1920x1080"`
- **Flags**: Decoding parameter

#### `pixel_format`
- **Description**: Set the video pixel format.
- **Type**: Pixel format (Enum)
- **Default**: `AV_PIX_FMT_YUV422P10LE`
- **Flags**: Decoding parameter
- **Possible Values**: ~~`AV_PIX_FMT_NV12`~~, `AV_PIX_FMT_YUV422P`, `AV_PIX_FMT_YUV444P10LE`, `AV_PIX_FMT_RGB24`, `AV_PIX_FMT_YUV422P10LE`

#### `frame_rate`
- **Description**: Set the video frame rate.
- **Type**: Video rate (String or Rational)
- **Default**: `"25"`
- **Flags**: Decoding parameter

#### `socket_name`
- **Description**: Set the memif socket name for the media data reception.
- **Type**: String
- **Default**: `NULL`
- **Flags**: Decoding parameter

#### `interface_id`
- **Description**: Set the interface ID for the media data reception.
- **Type**: Integer
- **Default**: `0`
- **Flags**: Decoding parameter
- **Range**: `-1` to `INT_MAX`

### Example Usage

To use the Mesh's Demuxer plugin with FFmpeg, pass the `-f mcm` flag and parameters. Here is an example command that sets some of the parameters:

```bash
ffmpeg -f mcm -ip_addr 192.168.1.100 -port 8000 -payload_type st22 -protocol_type udp -pixel_format yuv422p10le -video_size hd720 -frame_rate 30 -socket_name my_socket -interface_id 2 -i input.mcm -c:v copy output.mp4
```

This command receives media data from IP address `192.168.1.100` on port `8000` with `st22` payload type, using `udp` protocol, `hd720` video size, `30` frames per second (frame rate), onto socket named `my_socket` that has `2` set as an interface ID; and then copies the video stream to an output file `output.mp4`.
