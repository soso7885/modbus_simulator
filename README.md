# modbus
modbus RTU serial & modbus TCP
Support function code 0x01 ~ 0x06

modbus RTU ->
Master mode : ./mbser_mstr <port>
Slave mode : ./mbser_slv <port>

modbus TCP ->
Master mode : ./mbtcp_mstr <IP address> <port number>
Slave mode : ./mbtcp_slv <port number>

note->
Open slave ID polling(1~32), remove comment #define POLL_SLVID in mbus.h
Open debug print data, remove comment #define DEBUGMSG in mubus.h


