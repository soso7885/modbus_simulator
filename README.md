# Modbus Simulator
---

* Support protocols:
```
Modbus RTU
Modbus TCP
```
* Support function code:
```
0x01        Read Coil Status
0x02        Read Input Status
0x03        Read Holding Registers
0x04        Read Input Registers
0x05        Force Single Coil
0x06        Preset Single Register
0x15        Force multi Coil
0x16        Preset multi Register
```
* Usage:
```
1. Modbus RTU
    ./mbser_mstr [comport path]
    ./mbser_slv  [comport path]
2. Modbus TCP
    ./mbtcp_mstr [IP] [port]
    ./mbtcp_slv  [port]
```
