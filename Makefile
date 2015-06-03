SRC_SLAVE = mb_rtubuld.c mb_func.c mbus_slv.c
SRC_MASTER = mb_rtubuld.c mb_func.c mbus_mstr.c
all: mbus_slv mbus_mstr
mbus_mstr: ${SRC_MASTER}
	gcc -Wall -o $@ ${SRC_MASTER}
mbus_slv: ${SRC_SLAVE}
	gcc -Wall -o $@ ${SRC_SLAVE}
clean:
	rm -f mbus_slv mbus_mstr
	 
