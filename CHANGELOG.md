# Changelog

## Release version: 24.09

### 1. New Features:
1.1 Support for RDMA data transfer between media proxies
* Added RDMA implementation and rdma_args in payload_args; Created script to prepare environment for RDMA (#187)
* Build and install libfabric, libxdp and libbpf (#188) and updated codeql.yml, coverity.yml to include changes from ubuntu build: libfabric, libxdp and libbpf; Added LIBFABRIC_DIR to Media Proxy Dockerfile
* Updated README with libfabric dependencies
* Added test-rdma.sh with RDMA functional tests

1.2 FFmpeg plugin
* Added FFmpeg 7.0 support (#182) and set it as a default version (with a possibility to run 6.1)
* Add MTL audio ptime options and compatibility check in FFmpeg plugin
* Added FFmpeg Media Communications Mesh audio plugin with documentation and configuration arguments - number of audio channels (1, 2, etc.), sample rate (44.1, 48, or 96 kHz), MTL PCM format (16-bit or 24-bit Big Endian), MTL packet time

1.3 Sample app
* Added throughput printing in sample apps

1.4 AF_XDP/eBPF
* Run basic implementation of AF_XDP/eBPF (#180) using mtl native functions and MtlManager: The enablement of `native_af_xdp` for interfaces used in Media Proxy workloads required minor impact changes in Dockerfile build sequence as well as adjustments to proxy_context

1.5 Containers (Docker)
* Dockerfile size shrink to 265MB + Mtl-Manager (#168) added by default to the container
* Updates to Dockerfile, packages: autoconf, automake, autotools-dev, libtool

1.6 Documentation and tests
* Updated readme with supported audio argument options (#208)
* Update documentation on audio ptime options
* Added sequence diagrams of Media Communications Mesh FFmpeg plugin
* Added option to conditionally build unit tests: To disable compilation of unit tests one can invoke "BUILD_UNIT_TESTS=OFF ./build.sh"
* Parameters guide and kubernetes readme modifications (#169)
* Added Sphinx documentation and Github action automation to build it
* Added sketch for unit tests
* Update licenses and added copyright notices

1.7 Other
* Added memif_buffer_alloc_timeout()

### 2. Changes to Existing Features:

* Changed how threads are cancelled: Resources may not have been deallocated at all or deallocated while the thread was still running
* Not printing error if pthread_join() returns ESRCH: ESRCH means the thread has already exited
* Removed unnecessary pacing from sender_app
* Media Communications Mesh st22 fix to media_proxy segfault: added fix for media_proxy exit with segfault
* Improved pthread_cancel() err handling: pthread_cancel can only return ESRCH or success. ESRCH means that the thread is not running, which is not an error for us.
* Fixed error handling
* Changed audio buffer size in memif protocol mode
* Reworked PCM16/24 support (+FFmpeg plugin)
* Cleanup ffmpeg plugin build scripts (#183): JPEG XS is not required as direct ffmpeg-plugin, but used internally by MTL (kahawai.json), inside Media Proxy
* Memif native library based functional tests (#177): Functional tests for inter-process communication have been added, as an extension to existing tests
* Ext_frame: Corrupt frame output fix (#174): Frame size corrected, check for transport and frame size added, MTL memory alignment added
* Fixes to test.sh to include custom interface names
* Remove metadata from sent buffer (#173): Video data was overwritten with metadata, which resulted in not enough data being sent
* Styling and indentation fixes in documentation
* Updated udp_impl.c and fix the ssize_t overflow issue.
* Updated CMakeLists.txt append "-fPIC" flag
* Full code for library version control and load from tag
    - Libraries libraisr and libmcm_dp use position code independent properties from now on.
    - Added scripting for version from git parsing (exactly from tag) or if this fails default one is being set.
* Changed default patch actions for ffmpeg-plugin
* Dockerfiles adjustments to meet Trivy requirements: ports exposed, default user, entrypoint, other minor
* Various dependency bumps from Dependabot suggestions

### 3. Fixed Issues:

|              Title              |    Component  |                            Description                       |
|---------------------------------|---------------|--------------------------------------------------------------|
| Media Proxy crashes with segfault when receiving 1st frame in recver_app |  Media Proxy  | Fixed. |
| Input and output files differ in size when sent between sample apps in ST20 mode |  Sample apps  | Fixed. |
| Segmentation Fault crash of Media Proxy application (for not aligned configuration pixel format with a stream format) |  Media Proxy  | Fixed. |
| Media Proxy on Rx side exits unexpectedly when user stops Tx FFmpeg |  Media Proxy  | Fixed. |
| Frames sent with ext_frame feature are corrupted |  Media Communications Mesh  | Fixed. |
| Media Proxy crashes with Segfault when closing st22 session |  Media Proxy  | Fixed. |

### 4. Known Issues:

|              Title              |             Component   |  Description |
|-------------------------------------------|---------------|--------------|
| Sometimes starting new ST22 stream fails  |  Media Communications Mesh  | Issue detected on a single system. Not reproduced elsewhere as of now. |
| ST22 720p 60FPS failure/instability  |  Media Communications Mesh  | Issue detected on a single system. Not reproduced elsewhere as of now. |
| New iavf driver (4.12.5) is causing instabilities to Media Communications Mesh Media Proxy  | Media Proxy | N/A |
| Audio transmission of length not divisible by ptime is padded with zeros at the end  |  Media Communications Mesh  | N/A |
| Senders/Receivers must be started in proper order, or the transmission does not happen  | Media Proxy | For MEMIF transmissions Sender must be started first, and only then Receiver, in order for the transmission to happen. Reversed situation happens for ST30 transmission using MTL; Receiver first, then Sender. |
| FFmpeg st30 audio 96K 125us, instability, received stream differ  |  FFmpeg plugin  | May be connected with length/ptime issues |
| FFmpeg st30 audio 48K 1ms, received stream differ  |  FFmpeg plugin  | May be connected with length/ptime issues |


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
