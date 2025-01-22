# TestApp

Application for utilizing MCM api.

*Disclaimer*  
TX/RXapp is prepared for new api (/docs/sdk-json-proposal) that is not implemented yet, so, so far it works with simple mocks, that move a file from TxApp to RxApp.

## Usage

1. Create dir:
```bash
mkdir /tmp/MCM_MOCK
```

2. Build binaries:
```bash
mkdir build && cd build
cmake ..
make
touch client.json 
touch connection.json
```

3. Run RxApp:
```bash
./RxApp
```
```text
launching RX App 
RX App PID: 956656
reading client configuration... 
reading connection configuration... 
waiting for frames..
```

4. Run TxApp:
```bash
./TxApp <abs path to file to transmit> <RX App PID>
```
