# Scenario Set 2 – RDMA Transmission

## Scenario 2.2 – 2x Nodes / Single Receiver

### Configuration

```mermaid
flowchart LR
    subgraph Node A
        tx((Tx App))
        proxy1(Media Proxy A)
    end
    subgraph Node B
        rxB1((Rx App))
        proxy2(Media Proxy B)
    end
    sw(["Network
        Switch"])
    tx --> proxy1
    proxy1 -- RDMA --> sw
    sw -- RDMA --> proxy2
    proxy2 --> rxB1
```

### Payload Options

* Blob
* Video – Uncompressed
* Audio

### Test Cases

For detailed test cases, refer to the centralized [Test Cases documentation](../SCENARIO.md#test-cases).

#### Standalone

#### Blob

#### Node A

1. Start the `mesh-agent`:
    ```bash
    mesh-agent
    ```

2. Start the media proxy:
    ```bash
    sudo media_proxy -r <IP_A> -p 9300-9399 -t 8003
    ```

3. Create the client configuration file (`client_tx.json`):
    ```json
    {
        "apiVersion": "v1",
        "apiConnectionString": "Server=127.0.0.1; Port=8003",
        "apiDefaultTimeoutMicroseconds": 100000,
        "maxMediaConnections": 32
    }
    ```

4. Create the connection configuration file (`connection_tx.json`):
    ```json
    {
        "bufferQueueCapacity": 16,
        "maxMetadataSize": 8192,
        "connection": {
            "multipointGroup": {
                "urn": "ipv4:224.0.0.1:9003"
            }
        },
        "payload": {
            "blob": {}
        },
        "maxPayloadSize": 102400,
        "connCreationDelayMilliseconds": 100
    }
    ```

5. Start the transmitter application:
    ```bash
    sudo MCM_MEDIA_PROXY_PORT=8003 ./TxBlobApp client_tx.json connection_tx.json /mnt/media/random_data.bin
    ```

#### Node B

1. Start the media proxy:
    ```bash
    sudo NO_PROXY=<IP_A> media_proxy -r <IP_B> -p 9300-9399 -t 8003 --agent=<IP_A>:50051
    ```

2. Create the client configuration file (`client_rx.json`):
    ```json
    {
        "apiVersion": "v1",
        "apiConnectionString": "Server=127.0.0.1; Port=8003",
        "apiDefaultTimeoutMicroseconds": 100000,
        "maxMediaConnections": 32
    }
    ```

3. Create the connection configuration file (`connection_rx.json`):
    ```json
    {
        "bufferQueueCapacity": 16,
        "maxMetadataSize": 8192,
        "connection": {
            "multipointGroup": {
                "urn": "ipv4:224.0.0.1:9003"
            }
        },
        "payload": {
            "blob": {}
        },
        "maxPayloadSize": 102400,
        "connCreationDelayMilliseconds": 100
    }
    ```

4. Start the receiver application:
    ```bash
    sudo NO_PROXY=<IP_A> MCM_MEDIA_PROXY_PORT=8003 ./RxBlobApp client_rx.json connection_rx.json ./output.bin
    ```

#### Video

#### Node A

1. Start the `mesh-agent`:
    ```bash
    mesh-agent
    ```

2. Start the media proxy:
    ```bash
    sudo media_proxy -r <IP_A> -p 9300-9399 -t 8003
    ```

3. Create the client configuration file (`client_tx.json`):
    ```json
    {
        "apiVersion": "v1",
        "apiConnectionString": "Server=127.0.0.1; Port=8003",
        "apiDefaultTimeoutMicroseconds": 100000,
        "maxMediaConnections": 32
    }
    ```

4. Create the connection configuration file (`connection_tx.json`):
    ```json
    {
        "bufferQueueCapacity": 16,
        "maxMetadataSize": 8192,
        "connection": {
            "multipointGroup": {
                "urn": "ipv4:224.0.0.1:9003"
            }
        },
        "payload": {
            "video": {
                "width": <WIDTH>,
                "height": <HEIGHT>,
                "fps": <FPS>,
                "pixelFormat": <PIXEL_FORMAT>
            }
        }
    }
    ```

5. Start the transmitter application:
    ```bash
    sudo MCM_MEDIA_PROXY_PORT=8003 ./TxVideoApp client_tx.json connection_tx.json input_video.yuv
    ```

#### Node B

1. Start the media proxy:
    ```bash
    sudo NO_PROXY=<IP_A> media_proxy -r <IP_B> -p 9300-9399 -t 8003 --agent=<IP_A>:50051
    ```

2. Create the client configuration file (`client_rx.json`):
    ```json
    {
        "apiVersion": "v1",
        "apiConnectionString": "Server=127.0.0.1; Port=8003",
        "apiDefaultTimeoutMicroseconds": 100000,
        "maxMediaConnections": 32
    }
    ```

3. Create the connection configuration file (`connection_rx.json`):
    ```json
    {
        "bufferQueueCapacity": 16,
        "maxMetadataSize": 8192,
        "connection": {
            "multipointGroup": {
                "urn": "ipv4:224.0.0.1:9003"
            }
        },
        "payload": {
            "video": {
                "width": <WIDTH>,
                "height": <HEIGHT>,
                "fps": <FPS>,
                "pixelFormat": <PIXEL_FORMAT>
            }
        }
    }
    ```

4. Start the receiver application:
    ```bash
    sudo NO_PROXY=<IP_A> MCM_MEDIA_PROXY_PORT=8003 ./RxVideoApp client_rx.json connection_rx.json output_new.yuv
    ```

#### Audio

#### Node A

1. Start the `mesh-agent`:
    ```bash
    mesh-agent
    ```

2. Start the media proxy:
    ```bash
    sudo media_proxy -r <IP_A> -p 9300-9399 -t 8003
    ```

3. Create the client configuration file (`client_tx.json`):
    ```json
    {
        "apiVersion": "v1",
        "apiConnectionString": "Server=127.0.0.1; Port=8003",
        "apiDefaultTimeoutMicroseconds": 100000,
        "maxMediaConnections": 32
    }
    ```

4. Create the connection configuration file (`connection_tx.json`):
    ```json
    {
        "bufferQueueCapacity": 16,
        "maxMetadataSize": 8192,
        "connection": {
            "multipointGroup": {
                "urn": "ipv4:224.0.0.1:9003"
            }
        },
        "payload": {
            "audio": {
                "channels": <CHANNELS>,
                "sampleRate": <SAMPLE_RATE>,
                "format": "<AUDIO_ENCODING>",
                "packetTime": "<PACKET_TIME>"
            }
        }
    }
    ```

5. Start the transmitter application:
    ```bash
    sudo MCM_MEDIA_PROXY_PORT=8003 ./TxAudioApp client_tx.json connection_tx.json input_audio.pcm
    ```

#### Node B

1. Start the media proxy:
    ```bash
    sudo NO_PROXY=<IP_A> media_proxy -r <IP_B> -p 9300-9399 -t 8003 --agent=<IP_A>:50051
    ```

2. Create the client configuration file (`client_rx.json`):
    ```json
    {
        "apiVersion": "v1",
        "apiConnectionString": "Server=127.0.0.1; Port=8003",
        "apiDefaultTimeoutMicroseconds": 100000,
        "maxMediaConnections": 32
    }
    ```

3. Create the connection configuration file (`connection_rx.json`):
    ```json
    {
        "bufferQueueCapacity": 16,
        "maxMetadataSize": 8192,
        "connection": {
            "multipointGroup": {
                "urn": "ipv4:224.0.0.1:9003"
            }
        },
        "payload": {
            "audio": {
                "channels": <CHANNELS>,
                "sampleRate": <SAMPLE_RATE>,
                "format": "<AUDIO_ENCODING>",
                "packetTime": "<PACKET_TIME>"
            }
        }
    }
    ```

4. Start the receiver application:
    ```bash
    sudo NO_PROXY=<IP_A> MCM_MEDIA_PROXY_PORT=8003 ./RxAudioApp client_rx.json connection_rx.json output_audio.pcm
    ```

#### FFmpeg

#### Blob

Not supported.

#### Video

#### Node A
```bash
mesh-agent
sudo media_proxy -r <IP_A> -p 9300-9399 -t 8003
```
```bash
sudo MCM_MEDIA_PROXY_PORT=8003 ffmpeg -re -video_size <WIDTH>x<HEIGHT> \
    -pixel_format <PIXEL_FORMAT> \
    -i <VIDEO_INPUT_FILE_PATH> \
    -r <FPS>\
    -f mcm \
    -conn_type multipoint-group \
    -frame_rate <FPS> \
    -video_size <WIDTH>x<HEIGHT> \
    -pixel_format <PIXEL_FORMAT> \
    -
```

#### Node B
```bash
sudo NO_PROXY=$NO_PROXY,<IP_A> media_proxy -r <IP_B> -p 9300-9399 -t 8003 --agent=<IP_A>:50051
```
```bash
sudo MCM_MEDIA_PROXY_PORT=8003 ffmpeg -f mcm \
    -conn_type multipoint-group \
    -frame_rate <FPS> \
    -video_size <WIDTH>x<HEIGHT> \
    -pixel_format <PIXEL_FORMAT> \
    -i - \
    <VIDEO_OUTPUT_FILE_PATH> -y
```

#### Audio

#### Node A
```bash
mesh-agent
sudo media_proxy -r <IP_A> -p 9300-9399 -t 8003
```
```bash
sudo MCM_MEDIA_PROXY_PORT=8003 ffmpeg -re -i <AUDIO_INPUT_FILE_PATH> \
    -f mcm_audio_pcm<AUDIO_ENCODING> \
    -conn_type multipoint-group \
    -channels <CHANNELS> \
    -sample_rate <SAMPLE_RATE> \
    -ptime <PACKET_TIME> \
    -
```

#### Node B
```bash
sudo NO_PROXY=$NO_PROXY,<IP_A> media_proxy -r <IP_B> -p 9300-9399 -t 8003 --agent=<IP_A>:50051
```
```bash
sudo MCM_MEDIA_PROXY_PORT=8003 ffmpeg -f mcm_audio_pcm<AUDIO_ENCODING> \
    -conn_type multipoint-group \
    -channels <CHANNELS> \
    -sample_rate <SAMPLE_RATE> \
    -ptime <PACKET_TIME> \
    -i - \
    <AUDIO_OUTPUT_FILE_PATH> -y
```
