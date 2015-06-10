SRC_SLAVE_SER = mb_rtubuld.c mbser_func.c mbus_slv.c
SRC_MASTER_SER = mb_rtubuld.c mbser_func.c mbus_mstr.c
SRC_SLAVE_TCP = mbtcp_func.c mbtcp_slv.c
SRC_MASTER_TCP = mbtcp_func.c mbtcp_mstr.c

all: mbus_slv mbus_mstr mbtcp_mstr mbtcp_slv

mbus_mstr: ${SRC_MASTER_SER}
	gcc -Wall -o $@ ${SRC_MASTER_SER}
mbus_slv: ${SRC_SLAVE_SER}
	gcc -Wall -o $@ ${SRC_SLAVE_SER}
mbtcp_mstr: $(SRC_MASTER_TCP)
	gcc -Wall -o $@ ${SRC_MASTER_TCP}
mbtcp_slv: ${SRC_SLAVE_TCP}
	gcc -Wall -o $@ ${SRC_SLAVE_TCP}
clean:
	rm -f mbus_slv mbus_mstr mbtcp_mstr mbtcp_slv
	 
