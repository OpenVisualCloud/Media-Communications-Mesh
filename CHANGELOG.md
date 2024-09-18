# Changelog

## Release version: Unreleased

### 1. New Features:

#### 1.1 Support for RDMA data transfer between media proxies

### 2. Changes to Existing Features:

### 3. Fixed Issues:

### 4. Known Issues:

|              Title              |             Component   |  Description |
|-------------------------------------------|---------------|--------------|
| RDMA performance |  Media Proxy  | RDMA performance is limited due to existing synchronisation issues |

## Release version: 24.06

### 1. New Features:

#### 1.1. Media Communications Mesh FFmpeg plugin:

- Video Input/output plugin for FFmpeg – video processing pipeline framework.
- Single or multiple instances of FFmpeg with Media Communications Mesh Plugin connect to selected Media Proxy instance.
- Supported video pixel formats:

 - YUV 422, 8bit packed
 - RGB 8 bit
 - NV12 – YUV 420 planar 8 bit
 - YUV 444 planar, 10 bit, little endian
 - YUV 422 planar, 10 bit, little endian
 - No support for audio streams

#### 1.2. Enablement of PTP Time synchronization to Media Communications Mesh.

This feature uses Media Transport Library PTP Time synchronization feature.

#### 1.3. Added support to Media Proxy/SDK for changing input and output streams Payload ID.

Payload IDs beside of having defined default values (according to RFC or SMPTE specification) are passed as parameters for each stream.

#### 1.4. Added support to Media Proxy/SDK for receiving video stream from multicast group/publishing video stream to a multicast group

Added parameter to stream configuration allowing to pass multicast group IP address.

### 2. Changes to Existing Features:

#### 2.1. Modified docker file to decrease docker image minimal runtime size.

### 3. Fixed Issues:

|              Title              |    Component  |                            Description                       |
|---------------------------------|---------------|--------------------------------------------------------------|
| Incorrect received frame number |  Media Proxy  | Fixed passing frame number to a structure passed to the SDK. |

### 4. Known Issues:

|              Title              |             Component   |  Description |
|-------------------------------------------|---------------|--------------|
| UDP packets are sent with TTL=1           |  Media Proxy  | In case of mapping Media Communications Mesh Media Proxy to Physical function all packets are sent with TTL=1 SDBQ-129 |
| "Segmentation Fault” crash of Media Proxy |  Media Proxy  | In case of providing pixel format for a stream not aligned with real video stream pixel format there can happen “Segmentation Fault” crash of Media Proxy application SDBQ-409 |
