# Changelog

Summary of Changes:

## 1. New Features:

* Added RDMA implementation and rdma_args in payload_args; Created script to prepare environment for RDMA (#187)
* Added FFmpeg 7.0 support (#182) and set it as a default version (with a possibility to run 6.1)
* Added throughput printing in sample apps
* Updated readme with supported audio argument options (#208)
* Updated README with libfabric dependencies
* Build and install libfabric, libxdp and libbpf (#188) and updated codeql.yml, coverity.yml to include changes from ubuntu build: libfabric, libxdp and libbpf; Added LIBFABRIC_DIR to Media_Proxy Dockerfile
* Added memif_buffer_alloc_timeout()
* Updates to Dockerfile, packages: autoconf, automake, autotools-dev, libtool
* Update documentation on audio ptime options
* Add MTL audio ptime options and compatibility check in FFmpeg plugin
* Run basic implementation of AF_XDP/eBPF (#180) using mtl native functions and MtlManager: The enablement of `native_af_xdp` for interfaces used in media-proxy workloads required minor impact changes in Dockerfile build sequence as well as adjustments to proxy_context.cc
* Added FFmpeg MCM audio plugin with documentation and configuration arguments - number of audio channels (1, 2, etc.), sample rate (44.1, 48, or 96 kHz), MTL PCM format (16-bit or 24-bit Big Endian), MTL packet time
* Added option to conditionally build unit tests: To disable compilation of unit tests one can invoke "BUILD_UNIT_TESTS=OFF ./build.sh"
* Added sketch for unit tests
* Parameters guide and kubernetes readme modifications (#169)
* Added Sphinx documentation and Github action automation to build it
* Added sequence diagrams of MCM FFmpeg plugin
* Dockerfile size shrink to 265MB + Mtl-Manager (#168) added by default to the container

## 2. Changes to Existing Features:

* Changed how threads are cancelled: Resources may not have been deallocated at all or deallocated while the thread was still running
* Not printing error if pthread_join() returns ESRCH: ESRCH means the thread has already exited
* Removed unnecessary pacing from sender_app
* Increased ring size for RDMA
* Improved test-rdma.sh
* Made memif_buffer_alloc_timeout() more readable
* MCM st22 fix to media_proxy segfault: added fix for media_proxy exit with segfault
* Reverted memif_buffer_alloc_timeout usage in mtl.c
* Improved pthread_cancel() err handling: pthread_cancel can only return ESRCH or success. ESRCH means that the thread is not running, which is not an error for us.
* Update licenses and added copyright notices
* Corrected how frame size is calculated for RDMA in sender_app
* Fixed error handling
* Changed audio buffer size in memif protocol mode
* Reworked PCM16/24 support (+FFmpeg plugin)
* Cleanup ffmpeg plugin build scripts (#183): JPEG XS is not required as direct ffmpeg-plugin, but used internally by MTL (kahawai.json), inside media-proxy
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

## 3. Fixed Issues:

|              Title              |    Component  |                            Description                       |
|---------------------------------|---------------|--------------------------------------------------------------|
| Media Proxy crashes with segfault when receiving 1st frame in recver_app |  Media Proxy  | Fixed. |
| Input and output files differ in size when sent between sample apps in ST20 mode |  Sample apps  | Fixed. |
| Segmentation Fault crash of Media Proxy application (for not aligned configuration pixel format with a stream format) |  Media Proxy  | Fixed. |
| Media Proxy on Rx side exits unexpectedly when user stops Tx FFmpeg |  Media Proxy  | Fixed. |
| Frames sent with ext_frame feature are corrupted |  Media Communications Mesh  | Fixed. |
| Media Proxy crashes with Segfault when closing st22 session |  Media Proxy  | Fixed. |

## 4. Known Issues:

|              Title              |             Component   |  Description |
|-------------------------------------------|---------------|--------------|
| Sometimes starting new ST22 stream fails  |  Media Communications Mesh  | Issue detected on a single system. Not reproduced elsewhere as of now. |
| ST22 720p 60FPS failure/instability  |  Media Communications Mesh  | Issue detected on a single system. Not reproduced elsewhere as of now. |
| New iavf driver (4.12.5) is causing instabilities to MCM media proxy  | Media Proxy | N/A |
| Audio transmission of length not divisible by ptime is padded with zeros at the end  |  Media Communications Mesh  | N/A |
| Senders/Receivers must be started in proper order, or the transmission does not happen  | Media Proxy | For MEMIF transmissions Sender must be started first, and only then Receiver, in order for the transmission to happen. Reversed situation happens for ST30 transmission using MTL; Receiver first, then Sender. |
| FFmpeg st30 audio 96K 125us, instability, received stream differ  |  FFmpeg plugin  | May be connected with length/ptime issues |
| FFmpeg st30 audio 48K 1ms, received stream differ  |  FFmpeg plugin  | May be connected with length/ptime issues |


## 5. Other:

Release version: 24.09
