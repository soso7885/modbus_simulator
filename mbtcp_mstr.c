#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/select.h>

#include "mbus.h"

int _set_para(struct tcp_frm_para *tmfpara)
{
	int cmd;
	int tmp;
	unsigned short straddr;

	printf("Modbus TCP Master mode !\nEnter Transaction ID : ");
	scanf("%hu", &tmfpara->transID);
	tmfpara->potoID = (unsigned char)TCPMBUSPROTOCOL;
	tmfpara->msglen = (unsigned char)TCPQUERYMSGLEN;
	printf("Enter Unit ID : ");
	scanf("%hhu", &tmfpara->unitID);
	printf("Enter Function code : ");
	scanf("%d", &cmd);
	switch(cmd){
		case 1:
			tmfpara->fc = READCOILSTATUS;
			break;
		case 2:
			tmfpara->fc = READINPUTSTATUS;
			break;
		case 3:
			tmfpara->fc = READHOLDINGREGS;
			break;
		case 4:
			tmfpara->fc = READINPUTREGS;
			break;
		case 5:
			tmfpara->fc = FORCESIGLEREGS;
			break;
		case 6:
			tmfpara->fc = PRESETEXCPSTATUS;
			break;
		default:
			printf("Function code :\n");
			printf("1        Read Coil Status\n");
			printf("2        Read Input Status\n");
			printf("3        Read Holding Registers\n");
			printf("4        Read Input Registers\n");
			printf("5        Force Single Coil\n");
			printf("6        Preset Single Register\n");
			return -1;
	}
	printf("Setting Start addr : ");
	scanf("%hu", &straddr);
	tmfpara->straddr = straddr - 1;
	if(cmd == 5){
		printf("Setting register write status (1 : on/0 : off) : ");
		scanf("%d", &tmp);
		if(tmp){
			tmfpara->act = 0xff<<8;
		}else if(!tmp){
			tmfpara->act = 0;
		}else{
			printf("Setting register write status Fail (1 : on/0 : off)\n");
			exit(0);
		}
	}else if(cmd == 6){
		printf("Setting register action : ");
		scanf("%hu", &tmfpara->act);
	}else if(cmd == 3 || cmd == 4){
		printf("Setting register shift length : ");
		scanf("%d", &tmp);
		if(tmp > 110 || tmp < 0){
			printf("Please DO NOT exceed 110 ! ");
			printf("Come on, dude. That's just a testing progam ...\n");
			exit(0);
		}else{
			tmfpara->len = (unsigned short)tmp;
		}
	}else{
		printf("Setting shift length : ");
		scanf("%hu", &tmfpara->len);
	}
	
	return 0;
}

int _create_sk_cli(char *addr, char *port)
{
	int skfd;
	int ret;
	struct addrinfo hints;
	struct addrinfo *res;
	struct addrinfo *p;
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;

	ret = getaddrinfo(addr, port, &hints, &res);
	if(ret != 0){
		printf("<Modbus Tcp Master> getaddrinfo : %s\n", gai_strerror(ret));
		exit(0);
	}

	for(p = res; p != NULL; p = p->ai_next){
		skfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if(skfd == -1){
			continue;
		}
		printf("<Modbus Tcp Master> fd = %d\n", skfd);

		ret = connect(skfd, p->ai_addr, p->ai_addrlen);
		if(ret == -1){
			close(skfd);
			continue;
		}
		break;
	}

	if(p == NULL){
		printf("<Modbus Tcp Master> Fail to connect\n");
 		exit(0);
	}

	freeaddrinfo(res);
	
	return skfd;
}

int main(int argc, char **argv)
{
	int skfd;
	int ret;
	int retval;
	int wlen;
	int rlen;
	int lock;
	char *port;
	char *addr;
	unsigned char rx_buf[FRMLEN];
	unsigned char tx_buf[FRMLEN];	
	fd_set rfds;
	fd_set wfds;
	struct timeval tv;
	struct tcp_frm_para tmfpara;
	
	if(argc < 3){
		printf("Usage : ./mbtcp_mstr <IP Address> <PORT>\n");
		exit(0);
	}	
	addr = argv[1];
	port = argv[2];
	
	skfd = _create_sk_cli(addr, port);
	if(skfd < 0){
		close(skfd);
		printf("<Modbus Tcp Master> Fail to connect\n");
		exit(0);
	}

	ret = _set_para(&tmfpara);
	if(ret == -1){
		printf("<Modbus TCP Master> Set parameter fail\n");
		exit(0);
	}
	
	lock = 0;
	
	do{	
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		FD_SET(skfd, &rfds);
		FD_SET(skfd, &wfds);
		
		tv.tv_sec = 5;
		tv.tv_usec = 0;
		
		retval = select(skfd + 1, &rfds, &wfds, 0, &tv);
		if(retval <= 0){
			printf("<Modbus Tcp Master> select nothing ...\n");
			continue;
		}
	
		if(FD_ISSET(skfd, &wfds) && !lock){		
			tcp_build_query((struct tcp_frm *)tx_buf, &tmfpara);
            if(tmfpara.unitID == 10 || tmfpara.unitID == 9 || tmfpara.unitID == 11){
				print_data(tx_buf, TCPSENDQUERYLEN, SENDQRY);
			}
			wlen = send(skfd, &tx_buf, TCPSENDQUERYLEN, MSG_NOSIGNAL);
			if(wlen != TCPSENDQUERYLEN){
				printf("<Modbus TCP Master> send incomplete !!\n");
				break;
			}
			printf("<Modbus TCP Master> send query len = %d\n", wlen);

			tmfpara.transID += 1<<2;
			lock = 1;
		}

		if(FD_ISSET(skfd, &rfds) && lock){
			rlen = recv(skfd, rx_buf, FRMLEN, 0);
			if(rlen < 1){
				printf("<Modbus TCP Master> fuckin recv empty !!\n");
				continue;
			}

			ret = tcp_chk_pack_dest((struct tcp_frm *)rx_buf, &tmfpara); 
			if(ret == -1){
				memset(rx_buf, 0, FRMLEN);
				continue;
			}
			ret = tcp_resp_parser(rx_buf, &tmfpara, rlen);
			if(ret == -1){
				print_data(rx_buf, TCPRESPEXCPFRMLEN, RECVEXCP);
//				continue;
			}

//			print_data(rx_buf, rlen, RECVRESP);

	 		/* Polling slvID test */
			if(tmfpara.unitID == 32){
				tmfpara.unitID = 1;
			}else{
				tmfpara.unitID++;
			}
			printf("Now slvID = %d\n", tmfpara.unitID);
			/* test end */
			lock = 0;
		}
		sleep(1);
	}while(1);
	
	close(skfd);
	
	return 0;
}	
