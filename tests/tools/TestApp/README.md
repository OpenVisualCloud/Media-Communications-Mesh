# TestApp

Application for utilizing MCM api.

*Disclaimer*  
TX/RXapp is prepared for new api (/docs/sdk-json-proposal) that is not implemented yet, so, so far it works with simple mocks, that move a file from TxApp to RxApp.

# Usage
0. create dir:
```
mkdir /tmp/MCM_MOCK
```
1. build binaries:
```
mkdir build && cd build
cmake ..
make
touch client.json 
touch connection.json
```

2. run RxApp:
```
./RxApp
```
```text
launching RX App 
RX App PID: 956656
reading client configuration... 
reading connection configuration... 
waiting for frames..
```

3. run TxApp:
```
./TxApp <abs path to file to transmit> <RX App PID>
```
