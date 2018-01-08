SRC_SLAVE_SER = mb_rtubuld.c mbser_func.c mbser_slv.c mbus_common.c
SRC_MASTER_SER = mb_rtubuld.c mbser_func.c mbser_mstr.c mbus_common.c
SRC_SLAVE_TCP = mbtcp_func.c mbtcp_slv.c mbus_common.c
SRC_MASTER_TCP = mbtcp_func.c mbtcp_mstr.c mbus_common.c
SRC_SLAVE_ERROR_SER = mbser_slv_error.c mbser_func.c mb_rtubuld.c
SRC_SLAVETIMELAP = mbtcp_func.c mb_rtubuld.c mbser_func.c mbusGatewaySlaveTimeLap.c 
LDFLAGS = -l pthread
CFLAGS = -Wall -std=c99 -D_GNU_SOURCE

all: mbtcp_slv mbtcp_mstr mbser_mstr mbser_slv

mbser_mstr: $(SRC_MASTER_SER)
	gcc $(CFLAGS) -o $@ $(SRC_MASTER_SER)

mbser_slv: $(SRC_SLAVE_SER)
	gcc $(CFLAGS) -o $@ $(SRC_SLAVE_SER)

mbtcp_mstr: $(SRC_MASTER_TCP)
	gcc $(CFLAGS) -o $@ $(SRC_MASTER_TCP) $(LDFLAGS)

mbtcp_slv: $(SRC_SLAVE_TCP)
	gcc $(CFLAGS) -o $@ $(SRC_SLAVE_TCP) $(LDFLAGS)

clean:
	rm -f mbser_slv mbser_mstr mbtcp_mstr mbtcp_slv 
	 
