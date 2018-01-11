SRC_SLAVE_RTU = mb_rtubuld.c mbrtu_func.c mbrtu_slv.c mbus_common.c
SRC_MASTER_RTU = mb_rtubuld.c mbrtu_func.c mbrtu_mstr.c mbus_common.c

SRC_SLAVE_TCP = mbtcp_func.c mbtcp_slv.c mbus_common.c
SRC_MASTER_TCP = mbtcp_func.c mbtcp_mstr.c mbus_common.c

LDFLAGS = -l pthread
CFLAGS = -Wall -std=c99 -D_GNU_SOURCE

TARGET = mbtcp_slv mbtcp_mstr mbrtu_mstr mbrtu_slv

all: $(TARGET)

mbrtu_mstr: $(SRC_MASTER_SER)
	gcc $(CFLAGS) -o $@ $(SRC_MASTER_RTU)

mbrtu_slv: $(SRC_SLAVE_SER)
	gcc $(CFLAGS) -o $@ $(SRC_SLAVE_RTU)

mbtcp_mstr: $(SRC_MASTER_TCP)
	gcc $(CFLAGS) -o $@ $(SRC_MASTER_TCP) $(LDFLAGS)

mbtcp_slv: $(SRC_SLAVE_TCP)
	gcc $(CFLAGS) -o $@ $(SRC_SLAVE_TCP) $(LDFLAGS)

clean:
	rm -f $(TARGET)
	 
