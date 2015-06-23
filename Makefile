SRC_SLAVE_SER = mb_rtubuld.c mbser_func.c mbser_slv.c
SRC_MASTER_SER = mb_rtubuld.c mbser_func.c mbser_mstr.c
SRC_SLAVE_TCP = mbtcp_func.c mbtcp_slv.c
SRC_MASTER_TCP = mbtcp_func.c mbtcp_mstr.c
FLAG = -l pthread

all: mbser_slv mbser_mstr mbtcp_mstr mbtcp_slv

mbser_mstr: ${SRC_MASTER_SER}
	gcc -Wall -o $@ ${SRC_MASTER_SER}
mbser_slv: ${SRC_SLAVE_SER}
	gcc -Wall -o $@ ${SRC_SLAVE_SER}
mbtcp_mstr: $(SRC_MASTER_TCP)
	gcc -Wall -o $@ ${SRC_MASTER_TCP} ${FLAG}
mbtcp_slv: ${SRC_SLAVE_TCP}
	gcc -Wall -o $@ -g ${SRC_SLAVE_TCP} ${FLAG}
clean:
	rm -f mbser_slv mbser_mstr mbtcp_mstr mbtcp_slv
	 
