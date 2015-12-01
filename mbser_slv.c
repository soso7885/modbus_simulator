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

extern struct mbus_serial_func ser_func;

int _set_para(struct frm_para *sfpara)
{
	int cmd;
	int tmp;
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
	if(cmd == 1 || cmd == 2){
		printf("Setting address shift length : ");
		scanf("%d", &sfpara->len);
	}else if(cmd == 3 || cmd == 4){
		printf("Setting address shift length : ");
		scanf("%d", &tmp);
		if(tmp > 110 || tmp < 0){
			printf("Please DO NOT exceed 110 !\n");                                                   
			printf("Come on, dude. That's just a testing progam ...\n");
			exit(0);
		}
		sfpara->len = (unsigned int)tmp;
	}
		
	return 0;
}
int _choose_resp_frm(unsigned char *tx_buf, struct frm_para *sfpara, int ret, int *lock)
{
	int txlen;

	if(!ret){
		switch(sfpara->fc){
			case READCOILSTATUS:
				txlen = ser_func.build_0102_resp(tx_buf, sfpara, READCOILSTATUS);
				break;
			case READINPUTSTATUS:
				txlen = ser_func.build_0102_resp(tx_buf, sfpara, READINPUTSTATUS);
				break;
			case READHOLDINGREGS:
				txlen = ser_func.build_0304_resp(tx_buf, sfpara, READHOLDINGREGS);
				break;
			case READINPUTREGS:
				txlen = ser_func.build_0304_resp(tx_buf, sfpara, READINPUTREGS);
				break;
			case FORCESIGLEREGS:
				txlen = ser_func.build_0506_resp(tx_buf, sfpara, FORCESIGLEREGS);
				break;
			case PRESETEXCPSTATUS:
				txlen = ser_func.build_0506_resp(tx_buf, sfpara, PRESETEXCPSTATUS);
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
		txlen = ser_func.build_excp(tx_buf, sfpara, EXCPILLGFUNC);
		print_data(tx_buf, txlen, SENDEXCP);
	}else if(ret == -3){
		txlen = ser_func.build_excp(tx_buf, sfpara, EXCPILLGDATAVAL);
		print_data(tx_buf, txlen, SENDEXCP); 
	}else if(ret == -4){
		txlen = ser_func.build_excp(tx_buf, sfpara, EXCPILLGDATAADDR);
		print_data(tx_buf, txlen, SENDEXCP); 
	}

	return txlen;
}

int _set_termios(int fd, struct termios2 *newtio)
{
	int ret;

	ret = ioctl(fd, TCGETS2, newtio);
	if(ret < 0){
		printf("<Modbus Serial Slave> ioctl : %s\n", strerror(errno));
		return -1;
	}
	printf("<Modbus Serial Slave> BEFORE setting : ospeed %d ispeed %d\n", newtio->c_ospeed, newtio->c_ispeed);
	newtio->c_iflag &= ~(ISTRIP|IUCLC|IGNCR|ICRNL|INLCR|ICANON|IXON|IXOFF|IXANY|PARMRK);
	newtio->c_iflag |= (IGNBRK|IGNPAR);
	newtio->c_lflag &= ~(ECHO|ICANON|ISIG);
	newtio->c_cflag &= ~CBAUD;
	newtio->c_cflag |= BOTHER;
	/*
	 * Close termios 'OPOST' flag, thus when write serial data
	 * which first byte is '0a', it will not add '0d' automatically !! 
	 * Becaues of '\n' in ASCII is '0a' and '\r' in ASCII is '0d'.
	 * On usual, \n\r is linked, if you only send 0a(\n), 
	 * Kernel will think you forget \r(0d), so add \r(0d) automatically.                       
	*/
	newtio->c_oflag &= ~(OPOST);
	newtio->c_ospeed = 9600;
	newtio->c_ispeed = 9600;
	ret = ioctl(fd, TCSETS2, newtio);
	if(ret < 0){
		printf("<Modbus Serial Slave> ioctl : %s\n", strerror(errno));
		return -1;
	}
	printf("<Modbus Serial Slave> AFTER setting : ospeed %d ispeed %d\n", newtio->c_ospeed, newtio->c_ispeed);
	
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

	fd = open(path, O_RDWR);
	if(fd == -1){
		printf("<Modbus Serial Slave> open : %s\n", strerror(errno));
		return -1;
	}
	printf("<Modbus Serial Slave> Com port fd = %d\n", fd);

	if(_set_termios(fd, &newtio) == -1){
		printf("<Modbus Serial Slave> set termios fail\n");
		return -1;
	} 

	do{
		FD_ZERO(&wfds);
		FD_ZERO(&rfds);
		if(lock){
			FD_SET(fd, &wfds);
		}
		FD_SET(fd, &rfds);		
		tv.tv_sec = 5;
		tv.tv_usec = 0;

		retval = select(fd+1, &rfds, &wfds, 0, &tv);
		if(retval == 0){
			printf("<Modbus Serial Slave> Watting for query\n");
			sleep(3);
			continue;
		}
		/* Recv Query */
		if(FD_ISSET(fd, &rfds)){
			rlen = read(fd, rx_buf, SERRECVQRYLEN);
			if(rlen != SERRECVQRYLEN){
				printf("<Modbus Serial Slave> recv Query length != 8 (rlen = %d)!!\n", rlen);
				print_data(rx_buf, rlen, RECVINCOMPLT);
				continue;
			}
			ret = ser_chk_dest(rx_buf, &sfpara);
			if(ret == -1){
				memset(rx_buf, 0, FRMLEN);
				continue;
			}
			lock = 1;
			debug_print_data(rx_buf, rlen, RECVQRY);
			ret = ser_query_parser(rx_buf, &sfpara);
		}
		/* Send Respond */
		if(FD_ISSET(fd, &wfds)){
			txlen = _choose_resp_frm(tx_buf, &sfpara, ret, &lock);
			if(txlen == -1){
				continue;
			}
			wlen = write(fd, tx_buf, txlen);			
			if(wlen != txlen){
				printf("<Modbus Serial Slave> write incomplete !!\n");
				print_data(tx_buf, wlen, SENDINCOMPLT);
				break;
			}
			debug_print_data(tx_buf, wlen, SENDRESP);
			poll_slvID(sfpara.slvID);
			lock = 0;
		}	
		sleep(1);
	}while(1);
	
	if(fd == -1){
		close(fd);
	}
	
	return 0;
}

