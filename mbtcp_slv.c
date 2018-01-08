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

#define BACKLOG	10
#define mb_printf(str, args...) printf("<Modbus TCP Slave> "str, ##args)

unsigned char func_coil[] = {
	READCOILSTATUS,
	FORCESINGLECOIL,
	FORCEMULTICOILS
};

unsigned char func_reg[] = {
	READHOLDINGREGS,
	PRESETSINGLEREG,
	PRESETMULTIREGS
};

unsigned char func_istat[] = {
	READINPUTSTATUS
};

unsigned char func_ireg[] = {
	READINPUTREGS
};

struct thread_pack {
	uint8_t uid;
	int saddr;
	int len;
	uint8_t *data;
	int func_cnt;
	uint8_t *func_list;
	int rskfd;
	pthread_mutex_t mutex;
};

void * _prepare_data(int cmd, int len)
{
	void *data;
	int datalen;

	switch(cmd){
		case 1:
		case 2:
			datalen = len;
			break;
		case 3:
		case 4:
			datalen = 2 * len;
			break;
	}
	data = malloc(datalen);
	memset(data, 0, datalen);

	return data;
}

int set_param(struct thread_pack *tpack)
{
	int cmd;
	
	printf("Modbus TCP Slave !\nEnter Unit ID : ");
	scanf("%hhu", &tpack->uid);
	printf("node type :\n");
	printf("1        Coil Status\n");
	printf("2        Input Status\n");
	printf("3        Holding Registers\n");
	printf("4        Input Registers\n");
	printf("Enter Node Type : ");
	scanf("%d", &cmd);
	switch(cmd){
		case 1:
			tpack->func_cnt = sizeof(func_coil);
			tpack->func_list = func_coil;
			break;
		case 2:
			tpack->func_cnt = sizeof(func_istat);
			tpack->func_list = func_istat;
			break;
		case 3:
			tpack->func_cnt = sizeof(func_reg);
			tpack->func_list = func_reg;
			break;
		case 4:
			tpack->func_cnt = sizeof(func_ireg);
			tpack->func_list = func_ireg;
			break;
		default:
		
			return -1;
	}

	tpack->saddr = 0;
	do{	
		if(tpack->saddr < 0){
			printf("start address must be a positive value!\n");
		}
		printf("Set Start addr (start from 0): ");
		scanf("%d", &tpack->saddr);
	}while(tpack->saddr < 0);

	tpack->len = 1;
	do{
		if(tpack->saddr < 1){
			printf("data length must be larger than 1!\n");
		}
		printf("Set Data Length: ");
		scanf("%d", &tpack->len);
	}while(tpack->len < 1);


	tpack->data = _prepare_data(cmd, tpack->len);

	return 0;
}


static int create_sk_svr(char *port)
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
	
	ret = getaddrinfo(NULL, port, &hints, &res);
	if(ret != 0){
		mb_printf("getaddrinfo : %s\n", gai_strerror(ret));
		return -1;
	}
	
	for(p = res; p != NULL; p = p->ai_next){
		skfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if(skfd == -1)
			continue;
		
		opt = 1;
		ret = setsockopt(skfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
		if(ret == -1){
			mb_printf("setsockopt : %s\n", strerror(errno));
			close(skfd);
			return -1;
		}
	
		ret = bind(skfd, p->ai_addr, p->ai_addrlen);
		if(ret == -1){
			mb_printf("bind : %s\n", strerror(errno));
			close(skfd);
			continue;
		}
		break;
	}
	
	if(p == NULL){
		mb_printf("create socket failed\n");
		close(skfd);
		return -1;
	}
	
	ret = listen(skfd, BACKLOG);
	if(ret == -1){
		mb_printf("listen : %s\n", strerror(errno));
		close(skfd);
		return -1;
	}
	freeaddrinfo(res);
	mb_printf("Waiting for connect ...\n");
	
	return skfd;
}

static int sk_accept(int skfd)
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
		mb_printf("accept : %s\n", strerror(errno));
		return -1;
	}
	
	if(acp_addr.ss_family == AF_INET){
		p = (struct sockaddr_in *)&acp_addr;
		inet_ntop(AF_INET, &p->sin_addr, addr, sizeof(addr));
		mb_printf("recv from IP : %s\n", addr);
	}else if(acp_addr.ss_family == AF_INET6){
		s = (struct sockaddr_in6 *)&acp_addr;
		inet_ntop(AF_INET6, &s->sin6_addr, addr, sizeof(addr));
		mb_printf("recv from IP : %s\n", addr);
	}else{
		mb_printf("fuckin wried ! What is the recv addr family?");
		return -1;
	}
	
	return rskfd;
}

char _handle_mbus_info(mbus_cmd_info *mbusinfo, struct thread_pack *info, uint8_t *tmp_coil, uint8_t **dataptr)
{
	int i;
//	const struct mbus_tcp_frm * rx_frame = rx_buf;
	unsigned short * ptr_reg;
	int bitshift;
	int byteshift;

	for(i = 0; i < info->func_cnt; i++){
		if(mbusinfo->info.fc == info->func_list[i]){
			break;
		}
	}

	if(i == info->func_cnt){
		return EXCPILLGFUNC;
	}

	switch(mbusinfo->info.fc){
		case READCOILSTATUS:
		case READINPUTSTATUS:
			if(mbusinfo->query.addr < info->saddr){
				return EXCPILLGDATAADDR;
			}
			if((mbusinfo->query.len + mbusinfo->query.addr) > (info->saddr + info->len)){
				return EXCPILLGDATAADDR;
			}
			memset(tmp_coil, 0, 32);
			//pthread_mutex_lock(info->mutex);
			for(i = 0; i < mbusinfo->query.len; i++){
				bitshift = i % 8;
				byteshift = i / 8;
				if(info->data[mbusinfo->query.addr + i]){
					tmp_coil[byteshift] |= 0x1 << bitshift;
				}

			}
			//pthread_mutex_unlock(info->mutex);
			*dataptr = tmp_coil;
			break;
		case FORCESINGLECOIL:

			if(mbusinfo->swrite.addr < info->saddr){
				return EXCPILLGDATAADDR;
			}
			if(mbusinfo->swrite.addr > (info->saddr + info->len)){
				return EXCPILLGDATAADDR;
			}
			//pthread_mutex_lock(info->mutex);
			if(mbusinfo->swrite.data){
				info->data[mbusinfo->swrite.addr] = 1;
			}else{
				info->data[mbusinfo->swrite.addr] = 0;
			}
			//pthread_mutex_unlock(info->mutex);
			break;
		case FORCEMULTICOILS:
			if(mbusinfo->mwrite.addr < info->saddr){
				return EXCPILLGDATAADDR;
			}
			if((mbusinfo->mwrite.addr + mbusinfo->mwrite.len)> (info->saddr + info->len)){
				return EXCPILLGDATAADDR;
			}
			//pthread_mutex_lock(info->mutex);
			memcpy(&info->data[mbusinfo->mwrite.addr], 
				mbusinfo->mwrite.data, 
				mbusinfo->mwrite.bcnt);

			//pthread_mutex_unlock(info->mutex);
			break;
		case READHOLDINGREGS:
		case READINPUTREGS:
			if(mbusinfo->query.addr < info->saddr){
				return EXCPILLGDATAADDR;
			}
			if((mbusinfo->query.len + mbusinfo->query.addr) > (info->saddr + info->len)){
				return EXCPILLGDATAADDR;
			}
			//pthread_mutex_lock(info->mutex);
			*dataptr = &info->data[mbusinfo->query.addr * sizeof(short)];

			//pthread_mutex_unlock(info->mutex);
			break;
		case PRESETSINGLEREG:
			if(mbusinfo->swrite.addr < info->saddr){
				return EXCPILLGDATAADDR;
			}
			if(mbusinfo->swrite.addr > (info->saddr + info->len)){
				return EXCPILLGDATAADDR;
			}
			printf("write to address %d\n", mbusinfo->swrite.addr);
			//pthread_mutex_lock(info->mutex);
			ptr_reg = (unsigned short *)info->data;
			ptr_reg[mbusinfo->swrite.addr] = htons(mbusinfo->swrite.data);

			break;
		case PRESETMULTIREGS:
			if(mbusinfo->mwrite.addr < info->saddr){
				return EXCPILLGDATAADDR;
			}
			if((mbusinfo->mwrite.addr + mbusinfo->mwrite.len)> (info->saddr + info->len)){
				return EXCPILLGDATAADDR;
			}
			ptr_reg = (unsigned short *)info->data;
			
			for(i = 0; i < mbusinfo->mwrite.bcnt; i++){
				printf("[%d]", mbusinfo->mwrite.addr + 1);
			}
/*			printf("addr = %d ptr = %x bcnt = %hhu\n",mbusinfo->mwrite.addr, &ptr_reg[mbusinfo->mwrite.addr], mbusinfo->mwrite.bcnt);
			for(i = 0; i < mbusinfo->mwrite.bcnt; i++){
				printf("data [%d] %02x\n", i, *mbusinfo->mwrite.data);//rx_frame->mbus.mwrite.data[i]);
			}*/
			memcpy(&ptr_reg[mbusinfo->mwrite.addr], 
					mbusinfo->mwrite.data,
					(int)mbusinfo->mwrite.bcnt);

/*			for(i = 0; i < mbusinfo->mwrite.len; i++){
				printf("pool [%d] %04x\n", i,  ptr_reg[i + mbusinfo->mwrite.addr]);
			}*/

			break;
		default:
			printf("%s unknown command\n", __func__);
			return -1;
			break;
	}

	return 0;
}

#define BUF_LEN		1024
void *work_thread(void *data)
{
	int ret;
	int retval;
	uint8_t rx_buf[BUF_LEN];
	uint8_t tx_buf[BUF_LEN];
	uint8_t tmp_coil[128];
	struct thread_pack *tpack = (struct thread_pack *)data;
	int rskfd = tpack->rskfd;

//	mb_printf("Create work thread, connect fd = %d | thread ID = %lu\n", rskfd, pthread_self());
	pthread_detach(pthread_self());

	int tlen, rlen;
	fd_set rfds;
	mbus_cmd_info cmdinfo;
	mbus_tcp_info tcpinfo;
	mbus_resp_info respinfo;
	do{
		FD_ZERO(&rfds);
		FD_SET(rskfd, &rfds);

		retval = select(rskfd + 1, &rfds, 0, 0, 0);
		if(retval < 0){
			mb_printf("Select failed!\n");
			break;
		}
		
		if(retval == 0) continue;

		rlen = recv(rskfd, rx_buf, sizeof(rx_buf), 0);
		if(rlen < 1){
//			mb_printf("disconnect(rlen = %d) thread ID = %lu\n", rlen, pthread_self());
			break;
		}
			
		tcp_get_cmdinfos(rx_buf, rlen, &cmdinfo, &tcpinfo);

		//printf("tid = %u fc = %hhu uid = %u\n", tcpinfo.transID, cmdinfo.info.fc, cmdinfo.info.unitID);
		uint8_t *dataptr = NULL;
		pthread_mutex_lock(&tpack->mutex);
		ret = _handle_mbus_info(&cmdinfo, tpack, tmp_coil, &dataptr);
		switch(ret){
			case 0:
				//printf("__handle seccuess dataptr = %x tmp_coil = %x datapool = %x\n", dataptr, tmp_coil, tpack->data);
				mbus_cmd_resp(&cmdinfo, &respinfo, dataptr);
				tlen = tcp_build_resp(tx_buf, BUF_LEN, &respinfo, &tcpinfo);
				break;
			case EXCPILLGFUNC:
			case EXCPILLGDATAADDR:
				tlen = tcp_build_excp(tx_buf, BUF_LEN, &cmdinfo, &tcpinfo, ret);
				break;
			default:
				mb_printf("unknown ret = %d\n", ret);
				tlen = 0;
				break;
		}
		pthread_mutex_unlock(&tpack->mutex);
		if(tlen > 0)
			send(rskfd, tx_buf, tlen, 0);

	}while(1);

	if(rskfd > 0)
		close(rskfd);
	
	pthread_exit(NULL);
}

int main(int argc, char **argv)
{
	int skfd;
	int rskfd;
	int ret;
	pthread_t tid;
	struct thread_pack tpack;
	
	if(argc < 2){
		printf("%s [PORT]\n", argv[0]);
		exit(0);
	}

	set_param(&tpack);

	skfd = create_sk_svr(argv[1]);
	if(skfd == -1)
		return -1;
	
	pthread_mutex_init(&(tpack.mutex), NULL);
	
	do{	
		rskfd = sk_accept(skfd);
		if(rskfd == -1)
			break;
	
		tpack.rskfd = rskfd;
		
		ret = pthread_create(&tid, NULL, work_thread, (void *)&tpack);	
		if(ret != 0)
			handle_error_en(ret, "pthread_create");
	}while(1);

	close(skfd);
	pthread_mutex_destroy(&(tpack.mutex));

	return 0;
}
#undef mb_printf
