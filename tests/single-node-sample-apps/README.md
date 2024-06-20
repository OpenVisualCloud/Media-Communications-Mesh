# Test Single Node Sample Apps

## Prerequisites
1. The platform should have an MTL-compatible NIC installed (e.g. E810-C).
2. The MTL repository is expected to be present in the directory structure as shown below
   ```
   |
   |– Media-Communications-Mesh/
   |  |–– tests/
   |  |   |–– single-node-sampple-apps/
   |  |       |–– test.sh
   |  |       |–– README.md
   |  ...
   |
   |– Media-Transport-Library/
   |  |–– script/
   |  |   |–– nicctl.sh
   |  |   ...
   |  ...
   |
   ```

## How it works
1. Initial cleanup.
2. Initialize NIC PF and create VFs using the MTL `nicctl.sh` script.
3. Run Tx media_proxy in the background.
4. Run Rx media_proxy in the background.
5. Run recver_app in the background.
6. Wait for recver_app to connect to Rx media_proxy.
7. Run sender_app in the background.
8. Wait for sender_app to connect to Tx media_proxy.
9. Wait for transmission between sender_app and recver_app to complete.
10. Shutdown Tx and Rx media_proxy, sender_app and recver_app.
11. Check logs of Tx and Rx media_proxy, sender_app and recver_app to contain expected messages or error signatures.
12. Extract throughput data from Tx and Rx media_proxy logs.
13. Cleanup.
14. In case of success, repeat 3 times from Step 3.
15. Deinitialize NIC PF.

## Usage

```bash
sudo ./test.sh <test-option> <pf-bdf> <input-file> <duration> <frames_number> <width> <height> <fps> <pixel-format>
```

* `test-option`
   * `memif` – use a memif direct connection
   * `st20` – use an ST20 connection via media proxy
   * `st22` – use an ST22 connection via media proxy

* `pf-bdf` – NIC PF bus device function, default `0000:32:00.1`. VFs will be created on top of this PF.
* `input-file` – input video file path
* `duration` – video file duration in seconds, default `10`. Used to set a transmission timeout.
* `frames-count` – number of frames to transmit, default `300`
* `width` `height` – frame size, default `640` x `360`
* `fps` – frames per second, default `60`
* `pixel-format` – pixel format, default `yuv422p10le`

## Run test – memif

```bash
sudo ./test.sh memif 0000:32:00.1 video.yuv 30 300 640 360 60 yuv422p10le
```

```
2024-06-19 15:35:22 [INFO] Test MCM Tx/Rx for Single Node
2024-06-19 15:35:22 [INFO]   Binary directory: /home/user/Media-Communications-Mesh/out/bin
2024-06-19 15:35:22 [INFO]   Output directory: /home/user/Media-Communications-Mesh/test/single-node-sample-apps/out
2024-06-19 15:35:22 [INFO]   Input file path: /home/user/Media-Communications-Mesh/test/single-node-sample-apps/video.yuv
2024-06-19 15:35:22 [INFO]   Input file size: 276475800 byte(s)
2024-06-19 15:35:22 [INFO]   Frame size: 640 x 360, FPS: 60
2024-06-19 15:35:22 [INFO]   Pixel format: yuv422p10le
2024-06-19 15:35:22 [INFO]   Duration: 30s
2024-06-19 15:35:22 [INFO]   Frames number: 300
2024-06-19 15:35:22 [INFO]   Test option: memif
2024-06-19 15:35:22 [INFO]   NIC PF: 0000:32:00.1
2024-06-19 15:35:22 [INFO] Initial cleanup
2024-06-19 15:35:22 [INFO] Checking PF 0000:32:00.1 status
2024-06-19 15:35:24 [INFO] Network interface eth5 link is up
2024-06-19 15:35:24 [INFO] Disabling VFs on 0000:32:00.1
2024-06-19 15:35:27 [INFO] Creating VFs on 0000:32:00.1
2024-06-19 15:35:46 [INFO] Tx VF0 BDF: 0000:32:11.0
2024-06-19 15:35:46 [INFO] Rx VF1 BDF: 0000:32:11.1
2024-06-19 15:35:46 [INFO] --- Run #1 ---
2024-06-19 15:35:46 [INFO] Connection type: MEMIF
2024-06-19 15:35:46 [INFO] Starting Tx side media_proxy
2024-06-19 15:35:47 [INFO] Starting Rx side media_proxy
2024-06-19 15:35:48 [INFO] Starting sender_app
2024-06-19 15:35:49 [INFO] Starting recver_app
2024-06-19 15:35:49 [INFO] Waiting for transmission to complete (interval 35s)
2024-06-19 15:35:51 [INFO] Transmission completed
2024-06-19 15:35:52 [INFO] Shutting down apps
2024-06-19 15:35:53 [INFO] Input and output files are identical
2024-06-19 15:35:53 [INFO] Cleanup
2024-06-19 15:35:53 [INFO] --- Run #2 ---
2024-06-19 15:35:53 [INFO] Connection type: MEMIF
2024-06-19 15:35:53 [INFO] Starting Tx side media_proxy
2024-06-19 15:35:54 [INFO] Starting Rx side media_proxy
2024-06-19 15:35:55 [INFO] Starting sender_app
2024-06-19 15:35:56 [INFO] Starting recver_app
2024-06-19 15:35:56 [INFO] Waiting for transmission to complete (interval 35s)
2024-06-19 15:35:58 [INFO] Transmission completed
2024-06-19 15:35:59 [INFO] Shutting down apps
2024-06-19 15:36:00 [INFO] Input and output files are identical
2024-06-19 15:36:00 [INFO] Cleanup
2024-06-19 15:36:00 [INFO] --- Run #3 ---
2024-06-19 15:36:00 [INFO] Connection type: MEMIF
2024-06-19 15:36:00 [INFO] Starting Tx side media_proxy
2024-06-19 15:36:01 [INFO] Starting Rx side media_proxy
2024-06-19 15:36:02 [INFO] Starting sender_app
2024-06-19 15:36:03 [INFO] Starting recver_app
2024-06-19 15:36:03 [INFO] Waiting for transmission to complete (interval 35s)
2024-06-19 15:36:05 [INFO] Transmission completed
2024-06-19 15:36:06 [INFO] Shutting down apps
2024-06-19 15:36:07 [INFO] Input and output files are identical
2024-06-19 15:36:07 [INFO] Cleanup
2024-06-19 15:36:07 [INFO] Disabling VFs on 0000:32:00.1
2024-06-19 15:36:11 [INFO] Test completed successfully
```

## Run test – ST20

```bash
sudo ./test.sh st20 0000:32:00.1 video.yuv 30 300 640 360 60 yuv422p10le
```
TBD

## Run test – ST22

```bash
sudo ./test.sh st22 0000:32:00.1 video.yuv 30 300 640 360 60 yuv422p10le
```
TBD
