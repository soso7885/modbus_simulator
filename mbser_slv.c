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

#define RECVLEN 8

int _set_para(struct frm_para *sfpara)
{
	int cmd;
	unsigned int straddr;	
	
	printf("RTU Modbus Slave mode !\nEnter Slave ID : ");
	scanf("%d", &sfpara->slvID);
	printf("Enter function code : ");
	scanf("%d", &cmd);
	switch(cmd){
		case 1:
			sfpara->fc = READCOILSTATUS;
			break;
		case 2:
			sfpara->fc = READINPUTSTATUS;
			break;
		case 3:
			sfpara->fc = READHOLDINGREGS;
			break;
		case 4:
			sfpara->fc = READINPUTREGS;
			break;
		case 5:
			sfpara->fc = FORCESIGLEREGS;
			break;
		case 6:
			sfpara->fc = PRESETEXCPSTATUS;
			break;
		default:
			printf("Function code :\n");
			printf("1		Read Coil Status\n");
			printf("2		Read Input Status\n");
			printf("3		Read Holding Registers\n");
			printf("4		Read Input Registers\n");
			printf("5		Force Single Coil\n");
			printf("6		Preset Single Register\n");
			return -1;
	}
	printf("Setting contain, Start addr : ");
	scanf("%d", &straddr);
	sfpara->straddr = straddr - 1;
	if(cmd < 5){
		printf("Setting contain/ Shift len : ");
		scanf("%d", &sfpara->len);
	}
		
	return 0;
}
int _choose_resp_frm(unsigned char *tx_buf, struct frm_para *sfpara, int ret, int *lock)
{
	int txlen;

	if(!ret){
		switch(sfpara->fc){
			case READCOILSTATUS:
				txlen = ser_build_resp_read_status(tx_buf, sfpara, READCOILSTATUS);
				break;
			case READINPUTSTATUS:
				txlen = ser_build_resp_read_status(tx_buf, sfpara, READINPUTSTATUS);
				break;
			case READHOLDINGREGS:
				txlen = ser_build_resp_read_regs(tx_buf, sfpara, READHOLDINGREGS);
				break;
			case READINPUTREGS:
				txlen = ser_build_resp_read_regs(tx_buf, sfpara, READINPUTREGS);
				break;
			case FORCESIGLEREGS:
				txlen = ser_build_resp_set_single(tx_buf, sfpara, FORCESIGLEREGS);
				break;
			case PRESETEXCPSTATUS:
				txlen = ser_build_resp_set_single(tx_buf, sfpara, PRESETEXCPSTATUS);
				break;
			default:
				printf("<Modbus Serial Slave> unknown function code : %d\n", sfpara->fc);
				sleep(2);
				return -1;
		}
	}else if(ret == -1){
		*lock = 0;
		return -1;
	}else if(ret == -2){
		txlen = ser_build_resp_excp(tx_buf, sfpara, EXCPILLGFUNC);
	}else if(ret == -3){
		txlen = ser_build_resp_excp(tx_buf, sfpara, EXCPILLGDATAVAL);
	}else if(ret == -4){
		txlen = ser_build_resp_excp(tx_buf, sfpara, EXCPILLGDATAADDR);
	}

	return txlen;
}

int _set_termois(int fd, struct termios2 *newtio)
{
	int ret;

	/* get termios setting */
	ret = ioctl(fd, TCGETS2, newtio);
	if(ret < 0){
		printf("<Modbus Serial Slave> ioctl : %s\n", strerror(errno));
		return -1;
	}
	printf("<Modbus Serial Slave> BEFORE setting : ospeed %d ispeed %d ret = %d\n", newtio->c_ospeed, newtio->c_ispeed, ret);
	/* set termios setting */
	newtio->c_iflag &= ~(ISTRIP|IUCLC|IGNCR|ICRNL|INLCR|ICANON|IXON|PARMRK);
	newtio->c_iflag |= (IGNBRK|IGNPAR);
	newtio->c_lflag &= ~(ECHO|ICANON|ISIG);
	newtio->c_cflag &= ~CBAUD;
	newtio->c_cflag |= BOTHER;
	newtio->c_ospeed = 9600;
	newtio->c_ispeed = 9600;
	ret = ioctl(fd, TCSETS2, newtio);
	if(ret < 0){
		printf("<Modbus Serial Slave> ioctl : %s\n", strerror(errno));
		return -1;
	}
	printf("<Modbus Serial Slave> AFTER setting : ospeed %d ispeed %d ret = %d\n", newtio->c_ospeed, newtio->c_ispeed, ret);
	
	return 0;
}

int main(int argc, char **argv)
{
	int fd;
	int retval;
	int ret;
	int wlen;
	int rlen;
	int txlen;
	char *path;
	int lock = 0;
	fd_set wfds;
	fd_set rfds;
	struct termios2 newtio;
	struct timeval tv;
	unsigned char tx_buf[FRMLEN];
	unsigned char rx_buf[FRMLEN];
	struct frm_para sfpara;	
	
	if(argc < 2){
		printf("In slave mode usage : ");
		printf("./mbus_test [PATH]\n");
		printf("default path : /dev/ttyUSB0\n");
		exit(0);
	}
	path = argv[1];

	if(_set_para(&sfpara) == -1){
		printf("<Modbus Serial Slave> Set parameters fail\n");
		return 0;
	}

	/* open com port */
	fd = open(path, O_RDWR);
	if(fd == -1){
		printf("<Modbus Serial Slave> open : %s\n", strerror(errno));
		return -1;
	}
	printf("<Modbus Serial Slave> Com port fd = %d\n", fd);

	if(_set_termois(fd, &newtio) == -1){
		printf("<Modbus Serial Slave> set termios fail\n");
		return -1;
	} 

	do{
		FD_ZERO(&wfds);
		FD_ZERO(&rfds);
		FD_SET(fd, &wfds);
		FD_SET(fd, &rfds);		
		tv.tv_sec = 2;
		tv.tv_usec = 0;

		retval = select(fd+1, &rfds, &wfds, 0, &tv);
		if(retval == 0){
			printf("<Modbus Serial Slave> select nothing\n");
			continue;
		}
		/* Recv Query */
		if(FD_ISSET(fd, &rfds)){
			rlen = read(fd, rx_buf, RECVLEN);
			if(rlen != 8){
				printf("<Modbus Serial Slave> read incomplete !!\n");
				continue;
			}
	
			ret = ser_chk_dest(rx_buf, &sfpara);
			if(ret == -1){
				memset(rx_buf, 0, FRMLEN);
				continue;
			}
			lock = 1;
			/* Show recv query *//*
			int i;
			printf("<Modbus Serial Slave> Recv query :");
			for(i = 0; i < RECVLEN; i++){
				printf(" %x |", rx_buf[i]);
			}
			printf("## rlen = %d ##\n", rlen);
			*//* show end */
			ret = ser_query_parser(rx_buf, &sfpara);
		}
		/* Send Respond */
		if(FD_ISSET(fd, &wfds)){
			if(!lock){
				printf("<Modbus Serial Slave> wating for query ...\n");
				sleep(3);
				continue;
			}
			txlen = _choose_resp_frm(tx_buf, &sfpara, ret, &lock);
			if(txlen == -1){
				continue;
			}
			wlen = write(fd, tx_buf, txlen);
			printf("<Modbus Serial Slave> respond, wlen = %d\n", wlen);	
			if(wlen != txlen){
				printf("<Modbus Serial Slave> write incomplete !!\n");
				continue;
			}
			lock = 0;
		}	
		sleep(2);
	}while(1);
	
	if(fd == -1){
		close(fd);
	}
	
	return 0;
}
	
	

	
	
	

	
	
	

