# Topology Configuration File Guide

This document describes the structure and fields of the `topology.yaml` and `test_config.yaml` files used for Media Communications Mesh (MCM) validation. Use this as a reference when creating or editing your topology configuration.

---

## Topology Structure Overview

```yaml
metadata:
  version: '2.4'
hosts:
  - name: mesh-agent
    instantiate: true
    role: sut
    network_interfaces:
      - interface_name: <interface name>
    connections:
      - ip_address: <ip address>
        connection_type: SSHConnection
        connection_options:
          port: {{ port|default(22) }}
          username: <username>
          password: <password>
    extra_info:
      mtl_path: /opt/intel/mtl
      nicctl_path: /opt/intel/mtl/script
      media_proxy:
        st2110: true
        sdk_port: 8002
        st2110_dev: <st2110_dev vf pci address>
        st2110_ip: <st2110 vf ip address>
        rdma: true
        rdma_ports: 9100-9999
        rdma_ip: <rdma pf ip address>
      mesh_agent:
        control_port: 8100
        proxy_port: 50051
```

---

## Field Descriptions

### metadata
- **version**: Version of the topology file format.

### hosts
A list of host definitions. Each host can have the following fields:

- **name**: Name of the host. 
    If you want to run `mesh-agent` in your test, name one host as `mesh-agent`or specify in test_config.yaml name of one of your hosts as `mesh_agent_name: name_of_chosen_host`. This is required for the system to recognize it as the mesh-agent host. 
    If you want to use external mesh-agent set its IP address and ports in test_config.yaml.
- **instantiate**: Set to `true` to instantiate this host and create a connection.
- **role**: Role of the host (supported only `sut` or `client`).
- **network_interfaces**: List of network interfaces used in the test.
  - **interface_name**: Name of the network interface (e.g., `eth1`). You can also specify by PCI device ID or address. Please read MFD Config readme for more details on how to specify the interface. mfd-config repo: https://github.com/intel-innersource/libraries.python.mfd.pytest-mfd-config
- **connections**: List of connections for the host.
  - **ip_address**: IP address for the connection.
  - **connection_type**: Type of connection (e.g., `SSHConnection`) please read MFD Connect readme for more details: https://github.com/intel-innersource/libraries.python.mfd.mfd-connect/tree/main/mfd_connect
  - **connection_options**: Options for the connection.
    - **port**: SSH port (default: 22).
    - **username**: SSH username.
    - **password**: SSH password.
- **extra_info**: Additional configuration for the host.
  - **integrity_path**: Path for integrity scripts on the host if you want to run integrity tests (optional). 
  - **mtl_path**: Custom path to the MTL repo (optional) default is /mcm_path/_build/mtl.
  - **nicctl_path**: Path to `nicctl.sh` script (optional).
  - **media_proxy**: (Optional) Media proxy configuration. DO NOT set this if you don't want to run media proxy process.
    - **st2110**: Set to `true` to use the ST2110 bridge.
    - **sdk_port**: Port for media proxy SDK.
    - **st2110_dev**: (Optional) PCI device ID of the ST2110 bridge.
    - **st2110_ip**: (Optional) IP address of the ST2110 bridge.
    - **rdma**: Set to `true` to use RDMA in your test.
    - **rdma_ports**: (Optional) Range of RDMA ports for media proxy.
    - **rdma_ip**: (Optional) IP address of the RDMA interface.
  - **mesh_agent**: (Optional) Override default mesh-agent values.
    - **control_port**: Control port for mesh-agent (default: 8100).
    - **proxy_port**: Proxy port for mesh-agent (default: 50051).

---

## Notes

- Fields marked as optional can be omitted if default values are sufficient.
- You can add more hosts or network interfaces as needed.
- If a field is not set, the system will use default values or those set in the OS.

---

--- 
## Test config Structure Overview

```yaml
mesh_agent_name: dev8
mesh_ip: <ip> 
mesh_port: 50051
control_port: 8100
```


## Field Descriptions
- **mesh_agent_name**: Name of the mesh-agent host. If not specified, defaults to `mesh-agent`. This is the host designated in the topology file to run the mesh-agent.
- **mesh_ip**: IP address of the mesh-agent. If you want to use an external mesh-agent, specify its IP address here.
- **mesh_port**: Port of the mesh-agent. If you want to use an external mesh-agent, specify its port here. Default is `50051`.
- **control_port**: Control port of the mesh-agent. If you want to use an external mesh-agent, specify its control port here. Default is `8100`.
