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

#define mb_printf(str, args...) printf("<Modbus TCP Master> "str, ##args)

uint8_t *wdata = NULL;
int bytecnt = 0;
unsigned int delay = 0;

static void set_para(mbus_cmd_info *cmd)
{
	mb_printf("Enter Unit ID : ");
	scanf("%hhu", &cmd->info.unitID);
	printf("Function code :\n"
			"%hhu\t\tRead Coil Status\n"
			"%hhu\t\tRead Input Status\n"
			"%hhu\t\tRead Holding Register\n"
			"%hhu\t\tRead Input Register\n"
			"%hhu\t\tForce Single Coil\n"
			"%hhu\t\tPreset Single Register\n"
			"%hhu\t\tForce multi-Coil\n"
			"%hhu\t\tPreset multi-Register\n",
			READCOILSTATUS, READINPUTSTATUS, 
			READHOLDINGREGS, READINPUTREGS,
			FORCESINGLECOIL, PRESETSINGLEREG, 
			FORCEMULTICOILS, PRESETMULTIREGS);
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
			cmd->swrite.data = ntohs(0);
			break;
		case PRESETSINGLEREG:
			printf("Setting Start addr (start from 0): ");
			scanf("%hu", &cmd->swrite.addr);
			cmd->swrite.data = ntohs(0);
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
			bytecnt = cmd->mwrite.len*sizeof(uint16_t);
			cmd->mwrite.bcnt = bytecnt;
			wdata = (uint8_t *)malloc(bytecnt);
			assert(wdata);
			memset(wdata, 0, bytecnt);
			cmd->mwrite.data = wdata;
			break;
		default:
			printf("unknown function code");
	}

	printf("Setup delay between each command (seconds): ");
	scanf("%u", &delay);
}

static int create_sk_cli(const char *addr, const char *port)
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
		mb_printf("getaddrinfo : %s\n", gai_strerror(ret));
		return -1;
	}

	for(p = res; p != NULL; p = p->ai_next){
		skfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if(skfd == -1)
			continue;

		ret = connect(skfd, p->ai_addr, p->ai_addrlen);
		if(ret == -1){
			close(skfd);
			continue;
		}
		break;
	}

	if(!p){
		mb_printf("Fail to connect\n");
 		return -1;
	}

	freeaddrinfo(res);
	
	return skfd;
}

int main(int argc, char **argv)
{
	int retval;
	int skfd;
	uint8_t rx_buf[1024];
	uint8_t tx_buf[1024];
	mbus_cmd_info cmd;
	mbus_tcp_info tcp;
	mbus_tcp_info rtcp;
	mbus_resp_info resp;
	
	if(argc < 3){
		printf("%s [IP Address] [PORT]\n", argv[0]);
		exit(0);
	}

	skfd = create_sk_cli(argv[1], argv[2]);
	if(skfd < 0) return -1;

	set_para(&cmd);
	
	tcp.transID = 0;
	tcp.protoID = TCPMBUSPROTOCOL;

	fd_set rfds;
	struct timeval tv;
	int wlen, rlen;
	do{
		tcp.transID++;	

		wlen = tcp_build_cmd(tx_buf, 1024, &cmd, &tcp);
		retval = send(skfd, tx_buf, wlen, 0);
		if(retval <= 0){
			mb_printf("Send failed!\n");
			break;
		}

		FD_ZERO(&rfds);
		FD_SET(skfd, &rfds);
		tv.tv_sec = 2;
		tv.tv_usec = 0;
		
		retval = select(skfd+1, &rfds, NULL, NULL, &tv);
		if(retval < 0){
			mb_printf("Select nothing!\n");
			break;
		}
		
		if(retval == 0){
			mb_printf("Select nothing\n");
			sleep(1);
			continue;
		}
	
		rlen = recv(skfd, rx_buf, sizeof(rx_buf), 0);
		if(rlen <= 0){
			mb_printf("Recv failed!\n");
			break;
		}

		tcp_get_respinfos(rx_buf, rlen, &resp, &rtcp);

		mb_printf("rtid = %hu, mbus.fc = %hhu\n", rtcp.transID, resp.info.fc);

		if(resp.info.fc & EXCPTIONCODE){
			switch(resp.excp.ec){
				case EXCPILLGFUNC:
					mb_printf("exception : illegal function\n");
					break;
				case EXCPILLGDATAADDR:
					mb_printf("exception : illegal data address\n");
					break;
				case EXCPILLGDATAVAL:
					mb_printf("exception : illegal data value\n");
					break;
				default:
					printf("exception code: %hhx\n", resp.excp.ec);
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
				cmd.swrite.data = (uint16_t)cmd.swrite.data ? 0x0000 : 0xff00;
				break;
			case PRESETMULTIREGS:
			case FORCEMULTICOILS:
				memset(wdata, ++(wdata[0]), bytecnt);
				break;
			default:
				printf("unknown command\n");
				return 0;
		}
		sleep(delay);
	}while(1);
	
	if(skfd > 0)
		close(skfd);
	
	return 0;
}	
#undef mb_printf
