# TestApp

Application for utilizing Media Communications Mesh SDK API.

## Usage
1. Build binaries:
    ```shell
    mkdir build && cd build
    cmake ..
    make
    ```

2. Prepare client and conneciton files for sender (TxApp) and receiver (RxApp)
    ```shell
    touch client_tx.json
    touch connection_tx.json
    touch client_rx.json
    touch connection_rx.json
    ```
    > Note: The names of the files can differ from the ones presented above, but they must be reflected in the commands as in the following points.  
    Exemplary contents of those files can be found in [`client_example.json`](client_example.json) and [`connection_example.json`](connection_example.json). For more, check [appropriate documentation](../../../docs/sdk-json-proposal/SDK_API_WORKFLOW.md).

3. Run RxApp:
    ```shell
    ./RxApp <client_cfg.json> <connection_cfg.json> <path_to_output_file>
    ```
    For example:
    ```shell
    ./RxApp client_rx.json connection_rx.json output_video.yuv
    ```

4. Run TxApp:
    ```shell
    ./TxApp <client_cfg.json> <connection_cfg.json> <path_to_input_file>
    ```
    For example:
    ```shell
    ./RxApp client_tx.json connection_tx.json input_video.yuv
    ```
