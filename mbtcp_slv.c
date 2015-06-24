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
#include <sched.h>
#include <pthread.h>

#include "mbus.h" 

#define PORT 		"8000"
#define BACKLOG		10

int _set_para(struct tcp_frm_para *tsfpara){
	int tmp;
	int cmd;	
	unsigned short straddr;
	
	printf("Modbus TCP Slave !\nEnter Unit ID : ");
	scanf("%hhu", &tsfpara->unitID);
	tsfpara->potoID = (unsigned char)TCPMBUSPROTOCOL;
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
	printf("Setting Start addr : ");
	scanf("%hu", &straddr);
	tsfpara->straddr = straddr - 1;
	if(cmd == 1 || cmd == 2){
		printf("Setting address shift length : ");
		scanf("%hu", &tsfpara->len);
	}else if(cmd == 3 || cmd == 4){
		printf("Setting address shift length : ");
		scanf("%d", &tmp);
		if(tmp > 110 || tmp < 0){
			printf("Please DO NOT exceed 110 !\n");
			printf("Come on, dude. That's just a testing progam ...\n");
			exit(0);
		}
		tsfpara->len = (unsigned short)tmp;
	}
	return 0;
}

int _choose_resp_frm(unsigned char *tx_buf, struct thread_pack *tpack, int ret, int *lock)
{
	int txlen;
	struct tcp_frm_para *tsfpara;
	
	tsfpara = tpack->tsfpara;	
	if(!ret){
		switch(tsfpara->fc){
			case READCOILSTATUS:
				txlen = tcp_build_resp_read_status((struct tcp_frm_rsp *)tx_buf, tpack, READCOILSTATUS);
				break;
			case READINPUTSTATUS:
				txlen = tcp_build_resp_read_status((struct tcp_frm_rsp *)tx_buf, tpack, READINPUTSTATUS);
				break;
			case READHOLDINGREGS:
				txlen = tcp_build_resp_read_regs((struct tcp_frm_rsp *)tx_buf, tpack, READHOLDINGREGS);
				break;
			case READINPUTREGS:
				txlen = tcp_build_resp_read_regs((struct tcp_frm_rsp *)tx_buf, tpack, READINPUTREGS);
				break;
			case FORCESIGLEREGS:
				txlen = tcp_build_resp_set_single((struct tcp_frm *)tx_buf, tpack, FORCESIGLEREGS);
				break;
			case PRESETEXCPSTATUS:
				txlen = tcp_build_resp_set_single((struct tcp_frm *)tx_buf, tpack, PRESETEXCPSTATUS);
				break;
			default:
				printf("<Modbus TCP Slave> unknown function code : %d\n", tpack->tsfpara->fc);
				return -1;
			}
	}else if(ret == -1){
		txlen = tcp_build_resp_excp((struct tcp_frm_excp *)tx_buf, tsfpara, EXCPILLGFUNC);
	}else if(ret == -2){
		txlen = tcp_build_resp_excp((struct tcp_frm_excp *)tx_buf, tsfpara, EXCPILLGDATAADDR);
	}else if(ret == -3){
		txlen = tcp_build_resp_excp((struct tcp_frm_excp *)tx_buf, tsfpara, EXCPILLGDATAVAL);
	}

	return txlen;
}

int _create_sk_svr(void)
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

void *work_thread(void *data)
{
	int i;
	int wlen;
	int txlen;
	int rlen;
	int retval;
	int ret;
	int rskfd;
	int lock;	
	fd_set rfds;
	fd_set wfds;
	struct timeval tv;
	struct thread_pack *tpack;
	struct tcp_frm_para *tsfpara;
	unsigned char rx_buf[FRMLEN];
	unsigned char tx_buf[FRMLEN];

	tpack = (struct thread_pack *)data;
	rskfd = tpack->rskfd;
	tsfpara = tpack->tsfpara;

	lock = 0;
	printf("<Modbus Tcp Slave> Create work thread, connect fd = %d | thread ID = %lu\n",
			 rskfd, pthread_self());
	pthread_detach(pthread_self());

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
			if(rlen < 1){
				printf("<Modbus Tcp Slave> disconnect(rlen = %d) thread ID = %lu\n", rlen, pthread_self());
				close(rskfd);
				pthread_exit(NULL);
			}

			ret = tcp_chk_pack_dest((struct tcp_frm *)rx_buf, tsfpara);
			if(ret == -1){
				memset(rx_buf, 0, FRMLEN);
				continue;
			}   
			printf("<Modbus TCP Slave> Recv : ");
			for(i = 0; i < rlen; i++){
				printf("%x | ", rx_buf[i]);
			}
			printf(" ## rlen = %d ##\n", rlen );
			
			ret = tcp_query_parser((struct tcp_frm *)rx_buf, tpack);
			lock = 1;
		}
			
		if(FD_ISSET(rskfd, &wfds) && lock){
			txlen = _choose_resp_frm(tx_buf, tpack, ret, &lock);
			if(txlen == -1){
				break;
			}
			/* show send respond *//*
			printf("send resp :");
			for(i = 0; i < txlen; i++){
				printf(" %x |", tx_buf[i]);
			}
			printf(" ## txlen = %d ##\n", txlen);
			*//* show end */
			wlen = send(rskfd, tx_buf, txlen, 0);
			if(wlen != txlen){
				printf("<Modbus TCP Slave> send respond incomplete !!\n");
				break;
			}
			printf("txlen = %d\n", wlen);
		}

		lock = 0;
		sleep(1);
	}while(1);

	pthread_exit(NULL);
}

int main()
{
	int skfd;
	int rskfd;
	int ret;
	pthread_t tid;
	pthread_attr_t attr;
	struct sched_param param;
	struct thread_pack tpack;
	struct tcp_frm_para tsfpara;
	struct tcp_tmp_frm tmpara;

	ret = _set_para(&tsfpara);
	if(ret == -1){
		printf("<Modbus Tcp Slave> set parameter fail !!\n");
		exit(0);
	}

	skfd = _create_sk_svr();
	if(skfd == -1){
		printf("<Modbus Tcp Slave> god damn wried !!\n");
		exit(0);
	}
	
	tpack.tmpara = &tmpara;
	tpack.tsfpara = &tsfpara;
	pthread_mutex_init(&(tpack.mutex), NULL);
	pthread_attr_init(&attr);
	pthread_attr_setschedpolicy(&attr, SCHED_RR);	// use RR scheduler
	param.sched_priority = 10;						// set priority 10
	pthread_attr_setschedparam(&attr, &param);
	
	do{	
		rskfd = _sk_accept(skfd);
		if(rskfd == -1){
			printf("<Modbus Tcp Slave> god damn wried !!\n");
			break;
		}
	
		tpack.rskfd = rskfd;
		tpack.tsfpara = &tsfpara;
		
		ret = pthread_create(&tid, NULL, work_thread, (void *)&tpack);	
		if(ret != 0){
			handle_error_en(ret, "pthread_create");
		}	
	}while(1);	
	close(skfd);
	pthread_mutex_destroy(&(tpack.mutex));
	pthread_attr_destroy(&attr);
	return 0;
}



















