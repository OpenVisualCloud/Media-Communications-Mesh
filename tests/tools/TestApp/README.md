# TestApp

Validation application, mocking the proposal of new Media Communications Mesh SDK API from `/docs/sdk-json-proposal`.

# Usage
1. Create temporary directory
    ```
    mkdir /tmp/MCM_MOCK
    ```
2. Build binaries
    ```
    mkdir build && cd build
    cmake ..
    make
    ```
3. Prepare necessary configuration files
    ```
    touch client.json 
    touch connection.json
    ```
4. Run RXApp
    ```
    ./RxApp
    ```
    ```
    launching RX App 
    RX App PID: 956656
    reading client configuration... 
    reading connection configuration... 
    waiting for frames..
    ```
5. Run TxApp
    ```
    ./TxApp <abs path to file to transmit> <RX App PID>
    ```
    e.g.
    ```
    ./TxApp $(pwd)/../1080p_sample_422p10le.yuv $(ps -aux | awk '/\.\/RxApp/{print $2}')
    ```
