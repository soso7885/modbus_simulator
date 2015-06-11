#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/select.h> 
#include <arpa/inet.h>

#include "mbus.h" 

#define PORT 		"8000"
#define BACKLOG		10

int _set_para(struct tcp_frm_para *tsfpara){
	int cmd;	
	
	printf("Enter Transaction ID : ");
	scanf("%hu", &tsfpara->transID);
	tsfpara->potoID = (unsigned char)TCPMBUSPROTOCOL;
	printf("Enter Unit ID : ");
	scanf("%hhu", &tsfpara->unitID);
	printf("Enter Function code : ");
	scanf("%d", &cmd);
	switch(cmd){
		case 1:
			tsfpara->fc = READCOILSTATUS;
			break;
		case 2:
			tsfpara->fc = READINPUTSTATUS;
			break;
		case 3:
			tsfpara->fc = READHOLDINGREGS;
			break;
		case 4:
			tsfpara->fc = READINPUTREGS;
			break;
		case 5:
			tsfpara->fc = FORCESIGLEREGS;
			break;
		case 6:
			tsfpara->fc = PRESETEXCPSTATUS;
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
	printf("Enter Start addr : ");
	scanf("%hu", &tsfpara->straddr);
	printf("Enter regs number : ");
	if(cmd == 5 || cmd == 6){
		scanf("%hu", &tsfpara->act);
	}else{
		scanf("%hu", &tsfpara->len);
	}

	return 0;
}

int _choose_resp_frm(unsigned char *tx_buf, struct tcp_frm_para *tsfpara, int ret, int *lock)
{
	int txlen;
	
	if(!ret){
		switch(tsfpara->fc){
			case READCOILSTATUS:
				txlen = tcp_build_resp_read_status(tx_buf, tsfpara, READCOILSTATUS);
				break;
			case READINPUTSTATUS:
				txlen = tcp_build_resp_read_status(tx_buf, tsfpara, READINPUTSTATUS);
				break;
			case READHOLDINGREGS:
				txlen = tcp_build_resp_read_regs(tx_buf, tsfpara, READHOLDINGREGS);
				break;
			case READINPUTREGS:
				txlen = tcp_build_resp_read_regs(tx_buf, tsfpara, READINPUTREGS);
				break;
			case FORCESIGLEREGS:
				txlen = tcp_build_resp_set_single(tx_buf, tsfpara, FORCESIGLEREGS);
				break;
			case PRESETEXCPSTATUS:
				txlen = tcp_build_resp_set_single(tx_buf, tsfpara, PRESETEXCPSTATUS);
				break;
			default:
				printf("<Modbus TCP Slave> unknown function code : %d\n", tsfpara->fc);
				sleep(2);
				return -1;
			}
	}else if(ret == -1){
		txlen = tcp_build_resp_excp(tx_buf, tsfpara, EXCPILLGFUNC);
	}else if(ret == -2){
		txlen = tcp_build_resp_excp(tx_buf, tsfpara, EXCPILLGDATAADDR);
	}else if(ret == -3){
		txlen = tcp_build_resp_excp(tx_buf, tsfpara, EXCPILLGDATAVAL);
	}
	
	return txlen;
}

int _create_sk_srvr(void)
{
	int skfd;
	int ret;
	int opt;
	struct addrinfo hints;	
	struct addrinfo *res;	
	struct addrinfo *p;
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = 0;
	
	ret = getaddrinfo(NULL, PORT, &hints, &res);
	if(ret != 0){
		printf("<Modbus Tcp Slave> getaddrinfo : %s\n", gai_strerror(ret));
		exit(0);
	}
	
	for(p = res; p != NULL; p = p->ai_next){
		skfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if(skfd == -1){
			continue;
		}else{
			printf("<Modbus Tcp Slave> sockFD = %d\n", skfd);
		}
		
		opt = 1;
		ret = setsockopt(skfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
		if(ret == -1){
			printf("<Modbus Tcp Slave> setsockopt : %s\n", strerror(errno));
			close(skfd);
			exit(0);
		}
	
		ret = bind(skfd, p->ai_addr, p->ai_addrlen);
		if(ret == -1){
			printf("<Modbus Tcp Slave> bind : %s\n", strerror(errno));
			close(skfd);
			continue;
		}
		printf("<Modbus Tcp Slave> bind success\n");
	
		break;
	}
	
	if(p == NULL){
		printf("<Modbus Tcp Slave> create socket fail ...\n");
		close(skfd);
		exit(0);
	}
	
	ret = listen(skfd, BACKLOG);
	if(ret == -1){
		printf("<Modbus Tcp Slave> listen : %s\n", strerror(errno));
		close(skfd);
		exit(0);
	}

	freeaddrinfo(res);
	printf("<Modbus Tcp Slave> Waiting for connect ...\n");
	
	return skfd;
}

int _sk_accept(int skfd)
{
	int rskfd;
	char addr[INET6_ADDRSTRLEN];
	socklen_t addrlen;
	struct sockaddr_storage acp_addr;
	struct sockaddr_in *p;
	struct sockaddr_in6 *s;
	
	addrlen = sizeof(acp_addr);
	
	rskfd = accept(skfd, (struct sockaddr*)&acp_addr, &addrlen);
	if(rskfd == -1){
		close(rskfd);
		printf("<Modbus Tcp Slave> accept : %s\n", strerror(errno));
		exit(0);
	}
	
	if(acp_addr.ss_family == AF_INET){
		p = (struct sockaddr_in *)&acp_addr;
		inet_ntop(AF_INET, &p->sin_addr, addr, sizeof(addr));
		printf("<Modbus Tcp Slave> recv from IP : %s\n", addr);
	}else if(acp_addr.ss_family == AF_INET6){
		s = (struct sockaddr_in6 *)&acp_addr;
		inet_ntop(AF_INET6, &s->sin6_addr, addr, sizeof(addr));
		printf("<Modbus Tcp Slave> recv from IP : %s\n", addr);
	}else{
		printf("<Modbus Tcp Slave> fuckin wried ! What is the recv addr family?");
		return -1;
	}
	
	return rskfd;
}
	
int main()
{
	int i;
	int skfd;
	int rskfd;
	int ret;
	int retval;
	int wlen;
	int txlen;
	int rlen;
	int lock;
	unsigned char rx_buf[FRMLEN];
	unsigned char tx_buf[FRMLEN];
	fd_set rfds;
	fd_set wfds;
	struct timeval tv;	
	struct tcp_frm_para tsfpara;

	ret = _set_para(&tsfpara);
	if(ret == -1){
		printf("<Modbus Tcp Slave> set parameter fail !!\n");
		exit(0);
	}

	skfd = _create_sk_srvr();
	if(skfd == -1){
		printf("<Modbus Tcp Slave> god damn wried !!\n");
		exit(0);
	}
	
	rskfd = _sk_accept(skfd);
	if(rskfd == -1){
		printf("<Modbus Tcp Slave> god damn wried !!\n");
		exit(0);
	}
	
	lock = 0;
	
	do{
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		FD_SET(rskfd, &rfds);
		FD_SET(rskfd, &wfds);
	
		tv.tv_sec = 2;
		tv.tv_usec = 0;
	
		retval = select(rskfd + 1, &rfds, &wfds, 0, &tv);
		if(retval <= 0){
			printf("<Modbus Tcp Slave> select nothing ...\n");
			continue;
		}

		if(FD_ISSET(rskfd, &rfds)){
			rlen = recv(rskfd, rx_buf, sizeof(rx_buf), 0);
			if(rlen < 0){
				printf("<Modbus Tcp Slave> Recv query fail ...\n");
				continue;
			}
			ret = tcp_chk_pack_dest(rx_buf, &tsfpara);
			if(ret == -1){
				memset(rx_buf, 0, FRMLEN);
				continue;
			}
			printf("<Modbus Tcp Slave> Recv query len = %d\n", rlen);
			
			printf("recv : ");
            for(i = 0; i < rlen; i++){
                printf("%x | ", rx_buf[i]);
            }
            printf(" ## rlen = %d ##\n", rlen );

			ret = tcp_query_parser(rx_buf, &tsfpara);
			lock = 1;
		}
	
		if(FD_ISSET(rskfd, &wfds)){
			if(!lock){
				printf("recv query first !\n");
				sleep(3);
				continue;
			}
			txlen = _choose_resp_frm(tx_buf, &tsfpara, ret, &lock);
			if(txlen == -1){
				continue;
			}
			/* show send respond */
			printf("send resp :");
			for(i = 0; i < txlen; i++){
				printf(" %x |", tx_buf[i]);
			}
				printf(" ## txlen = %d ##\n", txlen);
			/* show end */
			wlen = send(rskfd, tx_buf, txlen, 0);
			if(wlen != txlen){
				printf("send error!\n");
			}
			printf("txlen = %d\n", wlen);
		}
		lock = 0;
		sleep(2);
	}while(1);

	close(rskfd);
	close(skfd);
	
	return 0;
}



















