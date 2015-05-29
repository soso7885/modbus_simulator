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

struct frm_para fpara;

int _build_resp_frm(unsigned char *tx_buf)
{
	int txlen;

	switch(fpara.fc){
		case READCOILSTATUS:
			txlen = build_resp_Rcoil_status(fpara.slvID, tx_buf, fpara.straddr, fpara.len);
			break;
		case READINPUTSTATUS:
			txlen = build_resp_Rinput_status(fpara.slvID, tx_buf, fpara.straddr, fpara.len);
			break;
		case READHOLDINGREGS:
			txlen = build_resp_Rholding_regs(fpara.slvID, tx_buf, fpara.straddr, fpara.len);
			break;
		case READINPUTREGS:
			txlen = build_resp_Rinput_regs(fpara.slvID, tx_buf, fpara.straddr, fpara.len);
			break;
		case FORCESIGLEREGS:
			txlen = build_resp_force_single_coli(fpara.slvID, tx_buf, fpara.straddr, fpara.act);
			break;
		case PRESETEXCPSTATUS:
			txlen = build_resp_preset_single_regs(fpara.slvID, tx_buf, fpara.straddr, fpara.val);
		default:
			printf("unknow Function code : %x\n", fpara.fc);
			sleep(2);
			return -1;
	}
	return txlen;
}

int _set_termois(int fd, struct termios2 *newtio)
{
	int ret;

	/* get termios setting */
    ret = ioctl(fd, TCGETS2, newtio);                                                                                                                                                                    
    if(ret < 0){
        printf("ioctl : %s\n", strerror(errno));
        return -1;
    }
    printf("BEFORE setting : ospeed %d ispeed %d ret = %d\n", newtio->c_ospeed, newtio->c_ispeed, ret);
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
        printf("ioctl : %s\n", strerror(errno));
        return -1;
    }
	printf("AFTER setting : ospeed %d ispeed %d ret = %d\n", newtio->c_ospeed, newtio->c_ispeed, ret);
	
	return 0;
}

int main()
{
	int fd;
	int retval;
	int wlen;
	int rlen;
	int txlen;
	int i;
	int lock = 0;
	fd_set wfds;
	fd_set rfds;
	struct termios2 newtio;
	struct timeval tv;
	unsigned char tx_buf[FRMLEN];
	unsigned char rx_buf[FRMLEN];
		
	/* open com port */
	fd = open("/dev/ttyUSB0", O_RDWR);
	if(fd == -1){
		printf("open : %s\n", strerror(errno));
		return -1;
	}
	printf("Com port fd = %d\n", fd);

	if(_set_termois(fd, &newtio) == -1){
		printf("set termios fail\n");
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
			printf("select nothing\n");
			continue;
		}
		/* Recv Query */
		if(FD_ISSET(fd, &rfds)){
			rlen = read(fd, rx_buf, RECVLEN);
			if(rlen != 8){
				printf("read incomplete!!\n");
				continue;
			}
			lock = 1;
			printf("Recv query :");
			for(i = 0; i < RECVLEN; i++){
				printf(" %x |", rx_buf[i]);
			}
			printf("## rlen = %d ##\n", rlen);

			paser_query(rx_buf, RECVLEN);
		}
		/* Send Respond */
		if(FD_ISSET(fd, &wfds)){
			if(!lock){
				printf("wating for query ...\n");
				sleep(3);
				continue;
			}
			txlen = _build_resp_frm(tx_buf);
			if(txlen == -1){
				continue;
			}
			wlen = write(fd, tx_buf, txlen);
			printf("Resp query, wlen = %d\n", wlen);	
			if(wlen != txlen){
				printf("write incomplete !!\n");
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
	
	

	
	
	

	
	
	

