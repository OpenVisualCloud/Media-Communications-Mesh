#TestApp

Application for utilizing MCM api,

*Disclaimer*  
TX/RXapp is prepared for new api (/docs/sdk-json-proposal) that is not implemented yet, so, so far it works with simple mocks, that move a file from TxApp to RxApp.

# usage
1. build binaries:
```
   $ mkdir build && cd build
   $ cmake ..
   $ make
```

2. run RXApp:
```
  $ ./RxApp
  launching RX App 
  RX App PID: 956656
  reading client configuration... 
  reading connection configuration... 
  waiting for frames..

```

3. run TxApp:
```
  $ ./TxApp <abs path to file to transmit> <RX App PID>
```


