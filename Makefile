SRC = mb_rtubuld.c mb_func.c mbus_main.c
all: mbus_test
mbus_test: ${SRC}
	gcc -Wall -o $@ ${SRC}
clean:
	rm -f mbus_test
	 
