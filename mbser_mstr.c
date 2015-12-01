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

int _set_para(struct frm_para *mfpara)
{
	int cmd;
	int tmp;
	unsigned int straddr;
	
	printf("RTU Master mode !\nEnter slave ID : ");
	scanf("%d", &mfpara->slvID);
	printf("Enter function code : ");
	scanf("%d", &cmd);
	switch(cmd){
		case 1:
			mfpara->fc = READCOILSTATUS;
			break;
		case 2:
			mfpara->fc = READINPUTSTATUS;
			break;
		case 3:
			mfpara->fc = READHOLDINGREGS;
			break;
		case 4:
			mfpara->fc = READINPUTREGS;
			break;
		case 5:
			mfpara->fc = FORCESIGLEREGS;
			break;
		case 6:
			mfpara->fc = PRESETEXCPSTATUS;
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
	printf("Enter start address : ");
	scanf("%d", &straddr);
	mfpara->straddr = straddr - 1;
	if(cmd == 5){
		printf("Setting register write status (on/off) : ");
		scanf("%d", &tmp);
		if(tmp){
			mfpara->act = 0xff<<8;
		}else if(!tmp){
			mfpara->act = 0;
		}else{
			printf("Setting register write status FAIL !! (1 ; on / 0 : off)\n");
			exit(0);
		}
	}else if(cmd == 6){
		printf("Setting register action : ");
		scanf("%d", &mfpara->act);
	}else if(cmd == 3 || cmd == 4){
		printf("Setting shift len : ");
		scanf("%d", &tmp);
		if(tmp > 110 || tmp < 0){
			printf("Please DO NOT exceed 110 ! ");
			printf("Come on, dude. That's just a testing progam.");
			exit(0);
		}
		mfpara->len = (unsigned int)tmp;
	}else{
		printf("Setting contain/shift len : ");
		scanf("%d", &mfpara->len);
	}
	
	return 0;
}

int _set_termois(int fd, struct termios2 *newtio)
{
	int ret;

	ret = ioctl(fd, TCGETS2, newtio);
	if(ret < 0){
		printf("<Modbus Serial Master> ioctl : %s\n", strerror(errno));
		return -1;
	}
	printf("<Modbus Serial Master> BEFORE setting : ospeed %d ispeed %d\n", newtio->c_ospeed, newtio->c_ispeed);
	newtio->c_iflag &= ~(ISTRIP|IUCLC|IGNCR|ICRNL|INLCR|ICANON|IXON|IXOFF|PARMRK);
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
		printf("<Modbus Serial Master> ioctl : %s\n", strerror(errno));
		return -1;
	}
	printf("<Modbus Serial Master> AFTER setting : ospeed %d ispeed %d\n", newtio->c_ospeed, newtio->c_ispeed);
	
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
	fd_set wfds;
	fd_set rfds;
	struct termios2 newtio;
	struct timeval tv;
	unsigned char tx_buf[FRMLEN];
	unsigned char rx_buf[FRMLEN];
	int lsr;
	struct frm_para mfpara;

	if(argc < 2){
		printf("./mbus_mstr [PATH]\n");
		printf("default : /dev/ttyUSB0\n");
		exit(0);
	}
	
	path = argv[1];

	if(_set_para(&mfpara) == -1){
		printf("<Modbus Serial Master> Set parameters fail\n");
		exit(0);
	}	
	fd = open(path, O_RDWR);
	if(fd == -1){
		printf("<Modbus Serial Master> open : %s\n", strerror(errno));
		return -1;
	}
	printf("<Modbus Serial Master> Com port fd = %d\n", fd);

	if(_set_termois(fd, &newtio) == -1){
		printf("<Modbus Serial Master> Set termios fail\n");
		return -1;
	} 

	txlen = ser_func.build_qry(tx_buf, &mfpara);
	print_data(tx_buf, txlen, SENDQRY);
	wlen = 0;

	do{
		FD_ZERO(&wfds);
		FD_ZERO(&rfds);
		FD_SET(fd, &wfds);
		if(wlen != 0){
			FD_SET(fd, &rfds);		
		}
		tv.tv_sec = 2;
		tv.tv_usec = 0;

		retval = select(fd+1, &rfds, &wfds, 0, &tv);
		if(retval <= 0){
			wlen = 0;
			printf("<Modbus Serial Master> Select nothing\n");
			sleep(3);
			continue;
		}

		if(FD_ISSET(fd, &rfds) && lsr && wlen != 0){
			rlen = read(fd, rx_buf, FRMLEN);			
			ret = ser_func.chk_dest(rx_buf, &mfpara);
			if(ret == -1){
				memset(rx_buf, 0, FRMLEN);
				continue;
			}
			wlen = 0;
			ret = ser_func.resp_parser(rx_buf, &mfpara, rlen);
			if(ret == -1){
				print_data(rx_buf, rlen, RECVEXCP);
				continue;
			}
			debug_print_data(rx_buf, rlen, RECVRESP);
			poll_slvID(mfpara.slvID);
		}
		/* Send Query */
		if(FD_ISSET(fd, &wfds) && !wlen){
			wlen = write(fd, tx_buf, txlen);
			ret = ioctl(fd, TIOCSERGETLSR, &lsr);
			if(ret == -1){ 
				printf("<Modbus Serial Master> TIOCSERGETLSR : %s\n", strerror(errno));	
				lsr = 1;
			}else{
				while(lsr == 0){
					ret = ioctl(fd, TIOCSERGETLSR, &lsr);
				}
			}
			if(wlen != txlen){
				printf("<Modbus Serial Master> write query incomplete !!\n");
				print_data(tx_buf, wlen, SENDINCOMPLT);
				continue;
			}
		}
		sleep(1);
	}while(1);
	
	if(fd == -1){
		close(fd);
	}
	
	return 0;
}
	
	

	
	
	

	
	
	

