#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#include <asm-generic/termbits.h>

#include "mbus.h"

#define mb_printf(str, args...) printf("<Modbus RTU Master> "str, ##args)

uint8_t *wdata = 0;
int bytecnt = 0;
unsigned int delay = 0;

static void set_para(mbus_cmd_info *cmd)
{
	mb_printf("Enter Slave ID : ");
	scanf("%hhu", &cmd->info.unitID);
	printf("Function code :\n");
	printf("%hhu\t\tRead Coil Status\n", READCOILSTATUS);
	printf("%hhu\t\tRead Input Status\n", READINPUTSTATUS);
	printf("%hhu\t\tRead Holding Registers\n", READHOLDINGREGS);
	printf("%hhu\t\tRead Input Registers\n", READINPUTREGS);
	printf("%hhu\t\tForce Single Coil\n", FORCESINGLECOIL);
	printf("%hhu\t\tPreset Single Register\n", PRESETSINGLEREG);
	printf("%hhu\t\tForce multi Coil\n", FORCEMULTICOILS);
	printf("%hhu\t\tPreset multi Register\n", PRESETMULTIREGS);
	printf("Enter Function code : ");
	scanf("%hhu", &cmd->info.fc);
	switch(cmd->info.fc){
		case READCOILSTATUS:
		case READINPUTSTATUS:
		case READHOLDINGREGS:
		case READINPUTREGS:
			printf("Setting Start addr (start from 0): ");
			scanf("%hu", &cmd->query.addr);
			printf("Setting Data Length: ");
			scanf("%hu", &cmd->query.len);
			break;
		case FORCESINGLECOIL:
			printf("Setting Start addr (start from 0): ");
			scanf("%hu", &cmd->swrite.addr);
			cmd->swrite.data = htobe16(0);
			break;
		case PRESETSINGLEREG:
			printf("Setting Start addr (start from 0): ");
			scanf("%hu", &cmd->swrite.addr);
			cmd->swrite.data = htobe16(0);
			break;
		case FORCEMULTICOILS:
			printf("Setting Start addr (start from 0): ");
			scanf("%hu", &cmd->mwrite.addr);
			printf("Setting Data Length: ");
			scanf("%hu", &cmd->mwrite.len);
			bytecnt = carry(cmd->mwrite.len, 8);
			cmd->mwrite.bcnt = bytecnt;
			wdata = (uint8_t *)malloc(bytecnt);
			assert(wdata);
			memset(wdata, 0, bytecnt);
			cmd->mwrite.data = wdata;
			break;
		case PRESETMULTIREGS:
			printf("Setting Start addr (start from 0): ");
			scanf("%hu", &cmd->mwrite.addr);
			printf("Setting Data Length: ");
			scanf("%hu", &cmd->mwrite.len);
			bytecnt = cmd->mwrite.len * sizeof(short);
			cmd->mwrite.bcnt = bytecnt;
			wdata = (uint8_t *)malloc(bytecnt);
			assert(wdata);
			memset(wdata, 0, bytecnt);
			cmd->mwrite.data = wdata;
			break;
		default:
			printf("unknown function code");
			exit(0);
	}

	printf("Setup delay between each command (seconds): ");
	scanf("%u", &delay);
}

static void set_termois(int fd)
{
	int ret;
	struct termios2 tio;

	ret = ioctl(fd, TCGETS2, &tio);
	assert(ret > 0);
	
	tio.c_iflag &= ~(ISTRIP|IUCLC|IGNCR|ICRNL|INLCR|ICANON|IXON|IXOFF|PARMRK);
	tio.c_iflag |= (IGNBRK|IGNPAR);
	tio.c_lflag &= ~(ECHO|ICANON|ISIG);
	tio.c_cflag &= ~CBAUD;
	tio.c_cflag |= BOTHER;
	/*
	 * Close termios 'OPOST' flag, thus when write serial data
	 * which first byte is '0a', it will not add '0d' automatically !! 
	 * Becaues of '\n' in ASCII is '0a' and '\r' in ASCII is '0d'.
	 * On usual, \n\r is linked, if you only send 0a(\n), 
	 * Kernel will think you forget \r(0d), so add \r(0d) automatically.
	*/
	tio.c_oflag &= ~(OPOST);
	tio.c_ospeed = BAUDRATE;
	tio.c_ispeed = BAUDRATE;
	ret = ioctl(fd, TCSETS2, &tio);
	assert(ret > 0);
	
	mb_printf("Setting termios: ospeed %d ispeed %d\n", tio.c_ospeed, tio.c_ispeed);
}

int main(int argc, char **argv)
{	
	int fd;
	int retval;
	int ret;
	uint8_t tx_buf[1024];
	uint8_t rx_buf[1024];
	mbus_cmd_info cmd;
	mbus_resp_info resp;

	if(argc < 2){
		printf("%s [PATH]\n", argv[0]);
		exit(0);
	}
	
	set_para(&cmd);

	fd = open(argv[1], O_RDWR);
	if(fd < 0){
		mb_printf("Open : %s\n", strerror(errno));
		return -1;
	}

	set_termois(fd);

	int txlen;
	int rlen;
	fd_set rfds;
	struct timeval tv;
	do{
		txlen = rtu_build_cmd(tx_buf, 1024, &cmd);
		write(fd, tx_buf, txlen);
		
		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);		

		tv.tv_sec = 2;
		tv.tv_usec = 0;
		rlen = 0;

		do{
			retval = select(fd+1, &rfds, 0, 0, &tv);
			if(retval <= 0){
				mb_printf("Select nothing\n");
				break;
			}

			rlen += read(fd, &rx_buf[rlen], sizeof(rx_buf) - rlen);

			ret = rtu_get_respinfo(rx_buf, rlen, &resp);
			if(ret > 0) break;
			if(ret == MBUS_ERROR_DATALEN) continue;
			
			tcflush(fd, TCIOFLUSH);

			break;
		}while(1);

		if(rtu_get_respinfo(rx_buf, rlen, &resp) <= 0){
			mb_printf("Failed to get resp info\n");
			continue;
		}

		mb_printf("resp FC = %d\n", resp.info.fc);

		if(resp.info.fc & EXCPTIONCODE){
			switch(resp.excp.ec){
				case EXCPILLGFUNC:
					mb_printf("exception: illegal function\n");
					break;
				case EXCPILLGDATAADDR:
					mb_printf("exception: illegal data address\n");
					break;
				case EXCPILLGDATAVAL:
					mb_printf("exception: illegal data value\n");
					break;
				default:
					mb_printf("exception code: %hhx\n", resp.excp.ec);
			}
			return 0;
		}

		switch(resp.info.fc){
			case READCOILSTATUS:
			case READINPUTSTATUS:
			case READHOLDINGREGS:
			case READINPUTREGS:
				for(int i = 0; i < resp.query.bcnt; i++){
					printf("[%d] %hhx \n", i, resp.query.data[i]);
				}
				break;
			case PRESETSINGLEREG:
				cmd.swrite.data++;
				break;
			case FORCESINGLECOIL:
				cmd.swrite.data = (unsigned short)cmd.swrite.data ? 0x0000:0xff00;
				break;
			case PRESETMULTIREGS:
			case FORCEMULTICOILS:
				memset(wdata, ++(wdata[0]), bytecnt);
				break;
			default:
				mb_printf("ERROR: unknown command\n");
				return 0;
		};

		sleep(delay);
	}while(1);

	if(fd > 0)
		close(fd);

	return 0;
}
#undef mb_printf
