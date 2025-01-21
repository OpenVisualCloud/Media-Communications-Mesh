# MCM Control Plane Agent

Agent is the core component of MCM that manages creation of SDK connections, multipoint groups, and bridges. Media Proxy instances connect to Agent to be managed
by business logic rules implemented in it.

## Compile

The Agent is built and installed in the system along with other MCM software
components by the repo build system. See README.md in the root folder.

## Run

```bash
mesh-agent
```
```text
INFO[0000] Mesh Control Plane Agent started             
DEBU[0000] JSON business logic manifest
...
INFO[0000] Server starts listening at :50051 - Media Proxy API (gRPC) 
INFO[0000] Server starts listening at :8100 - Control Plane API (REST) 
```

Optional argument list is the following
```bash
mesh-agent -h
```
```text
Usage of mesh-agent:
  -c uint
    	Control API port number (default 8100)
  -p uint
    	Proxy API port number (default 50051)
```
