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
#include <endian.h>

#include "mbus.h"

#define mb_printf(str, args...) printf("<Modbus RTU Slave> "str, ##args);

uint8_t func_coil[] = {
	READCOILSTATUS,
	FORCESINGLECOIL,
	FORCEMULTICOILS
};

uint8_t func_reg[] = {
	READHOLDINGREGS,
	PRESETSINGLEREG,
	PRESETMULTIREGS
};

uint8_t func_istat[] = {
	READINPUTSTATUS
};

uint8_t func_ireg[] = {
	READINPUTREGS
};

struct thread_pack {
	uint8_t  uid;
	int32_t  saddr;
	int32_t  len;
	uint8_t *data;
	int func_cnt;
	uint8_t *func_list;
	int rskfd;
};

static void *_prepare_data(int cmd, int len)
{
	void *data;
	int datalen = 0;

	switch(cmd){
		case 1:
		case 2:
			datalen = len;
			break;
		case 3:
		case 4:
			datalen = 2*len;
			break;
	}

	if(datalen){
		data = malloc(datalen);
		assert(data);
		memset(data, 0, datalen);
	}

	return data;
}

static void set_para(struct thread_pack *tpack)
{
	int cmd;
	
	mb_printf("Enter Unit ID : ");
	scanf("%hhu", &tpack->uid);
	printf("Node type :\n");
	printf("1)			Coil Status\n");
	printf("2)			Input Status\n");
	printf("3)			Holding Registers\n");
	printf("4)			Input Registers\n");
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
			printf("Unknow command!\n");
			exit(0);
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
}

static int set_termois(int fd)
{
	int ret;
	struct termios2 tio;

	ret = ioctl(fd, TCGETS2, &tio);
	assert(ret > 0);
	
	tio.c_iflag &= ~(ISTRIP|IUCLC|IGNCR|ICRNL|INLCR|ICANON|IXON|IXOFF|IXANY|PARMRK);
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
	
	return 0;
}

static uint8_t _handle_mbus_info(mbus_cmd_info *mbusinfo, struct thread_pack *info, uint8_t *tmp_coil, uint8_t **ptr)
{
	int i;
	uint16_t *ptr_reg;
	int bitshift;
	int byteshift;

	// is function code supported?
	for(i = 0; i < info->func_cnt; i++){
		if(mbusinfo->info.fc == info->func_list[i])
			break;
	}
	if(i == info->func_cnt)
		return EXCPILLGFUNC;

	switch(mbusinfo->info.fc){
		case READCOILSTATUS:
		case READINPUTSTATUS:
			if(mbusinfo->query.addr < info->saddr)
				return EXCPILLGDATAADDR;
			if((mbusinfo->query.len + mbusinfo->query.addr) > (info->saddr + info->len))
				return EXCPILLGDATAADDR;
			memset(tmp_coil, 0, 32);
			for(i = 0; i < mbusinfo->query.len; i++){
				bitshift = i % 8;
				byteshift = i / 8;
				if(info->data[mbusinfo->query.addr + i])
					tmp_coil[byteshift] |= 0x1 << bitshift;
			}
			*ptr = tmp_coil;
			break;
		case FORCESINGLECOIL:
			if(mbusinfo->swrite.addr < info->saddr)
				return EXCPILLGDATAADDR;
			if(mbusinfo->swrite.addr > (info->saddr + info->len))
				return EXCPILLGDATAADDR;
			info->data[mbusinfo->swrite.addr] = mbusinfo->swrite.data ? 1 : 0;
			break;
		case FORCEMULTICOILS:
			if(mbusinfo->mwrite.addr < info->saddr)
				return EXCPILLGDATAADDR;
			if((mbusinfo->mwrite.addr + mbusinfo->mwrite.len)> (info->saddr + info->len))
				return EXCPILLGDATAADDR;
			memcpy(&info->data[mbusinfo->mwrite.addr], 
					mbusinfo->mwrite.data, 
					mbusinfo->mwrite.bcnt);
			break;
		case READHOLDINGREGS:
		case READINPUTREGS:
			if(mbusinfo->query.addr < info->saddr)
				return EXCPILLGDATAADDR;
			if((mbusinfo->query.len + mbusinfo->query.addr) > (info->saddr + info->len))
				return EXCPILLGDATAADDR;
			*ptr = &info->data[mbusinfo->query.addr * sizeof(int16_t)];
			break;
		case PRESETSINGLEREG:
			if(mbusinfo->swrite.addr < info->saddr)
				return EXCPILLGDATAADDR;
			if(mbusinfo->swrite.addr > (info->saddr + info->len))
				return EXCPILLGDATAADDR;
			mb_printf("write to address %d\n", mbusinfo->swrite.addr);
			ptr_reg = (uint16_t *)info->data;
			ptr_reg[mbusinfo->swrite.addr] = htobe16(mbusinfo->swrite.data);
			break;
		case PRESETMULTIREGS:
			if(mbusinfo->mwrite.addr < info->saddr)
				return EXCPILLGDATAADDR;
			if((mbusinfo->mwrite.addr + mbusinfo->mwrite.len) > (info->saddr + info->len))
				return EXCPILLGDATAADDR;
			ptr_reg = (uint16_t *)info->data;
			for(i = 0; i < mbusinfo->mwrite.bcnt; i++)
				printf("[%d]", mbusinfo->mwrite.addr + 1);
			memcpy(&ptr_reg[mbusinfo->mwrite.addr], 
					mbusinfo->mwrite.data,
					(int)mbusinfo->mwrite.bcnt);
			break;
		default:
			mb_printf("%s unknown command\n", __func__);
			return -1;
	}

	return 0;
}

int main(int argc, char **argv)
{
	int fd;
	int retval;
	int ret;
	uint8_t tx_buf[1024];
	uint8_t rx_buf[1024];
	uint8_t tmp_coil[128];
	struct thread_pack tpack;
	mbus_cmd_info cmdinfo;
	mbus_resp_info respinfo;

	if(argc < 2){
		printf("%s [PATH]\n", argv[0]);
		exit(0);
	}

	set_para(&tpack);

	fd = open(argv[1], O_RDWR);
	if(fd == -1){
		mb_printf("open : %s\n", strerror(errno));
		return -1;
	}

	set_termois(fd);

	fd_set rfds;
	struct timeval tv;
	int rlen;
	int wlen;
	do{
		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);		
		tv.tv_sec = 2;
		tv.tv_usec = 0;
	
		rlen = 0;
		
		do{
			retval = select(fd+1, &rfds, 0, 0, &tv);
			if(retval <= 0) break;

			// TODO: read failed check
			rlen += read(fd, &rx_buf[rlen], sizeof(rx_buf)-rlen);
			// check command
			ret = rtu_get_cmdinfo(rx_buf, rlen, &cmdinfo);
			if(ret > 0) break;
			if(ret == MBUS_ERROR_DATALEN) continue;
			tcflush(fd, TCIOFLUSH);
			break;
		}while(1);

		// ckeck data		
		ret = rtu_get_cmdinfo(rx_buf, rlen, &cmdinfo);
		if(ret <= 0){
			for(int i = 0; i < rlen; i++)
				printf("%02x", rx_buf[i]);
			if(rlen > 0)
				printf("\n");
			continue;
		}

		uint8_t *ptr = NULL;
		ret = _handle_mbus_info(&cmdinfo, &tpack, tmp_coil, &ptr);
		switch(ret){
			case 0:
				mbus_cmd_resp(&cmdinfo, &respinfo, ptr);
				wlen = rtu_build_resp(tx_buf, sizeof(tx_buf), &respinfo);
				break;
			case EXCPILLGFUNC:
			case EXCPILLGDATAADDR:
				wlen = rtu_build_excp(tx_buf, sizeof(tx_buf), &cmdinfo, ret);
				break;
			default:
				mb_printf("unknown ret = %hhu\n", ret);
				wlen = 0;
				break;
		}

		if(wlen > 0)
			write(fd, tx_buf, wlen);
	}while(1);

	if(fd > 0)
		close(fd);

	return 0;
}
#undef mb_printf	
