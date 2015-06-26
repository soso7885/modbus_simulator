#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include "mbus.h"

int ser_query_parser(unsigned char *rx_buf, struct frm_para *sfpara)
{
	unsigned char qfc;
	unsigned char rfc;
	unsigned int qstraddr;
	unsigned int rstraddr;
	unsigned int qact;
	unsigned int rlen;
	
	qfc = *(rx_buf+1);
	qstraddr = *(rx_buf+2)<<8 | *(rx_buf+3);
	qact = *(rx_buf+4)<<8 | *(rx_buf+5);
	rfc = sfpara->fc;
	rstraddr = sfpara->straddr;
	rlen = sfpara->len;

	if(rfc != qfc){
		printf("<Modbus Serial Slave> Function code improper");
		printf(" Respond fc = %x | Query fc = %x\n", rfc, qfc);
		return -2;
	}
	
	if(!(rfc ^ FORCESIGLEREGS)){				// FC = 0x05, get the status to write(on/off)
		if(!qact || qact == 0xff<<8){
			sfpara->act = qact;
		}else{
			printf("<Modbus Serial Slave> Query set write status FUCKIN WRONG (fc = 0x05)\n");
			return -3;							// the other fuckin respond excp code?
		}
		if(qstraddr != rstraddr){
			printf("<Modbus Serial Slave> Query register address wrong (fc = 0x05)");
			printf(", query addr : %x | resp addr : %x\n", qstraddr, rstraddr);
			return -2;
		} 
	}else if(!(rfc ^ PRESETEXCPSTATUS)){		// FC = 0x06, get the value to write
		if(qstraddr != rstraddr){
			printf("<Modbus Serial Slave> Query register address wrong (fc = 0x06)");
			printf(", query addr : %x | resp addr : %x\n", qstraddr, rstraddr);
			return -2;
		}
		sfpara->act = qact;
	}else{
		if((qstraddr + qact <= rstraddr + rlen) && (qstraddr >= rstraddr)){	// Query addr+shift len must smaller than the contain we set in addr+shift len
			sfpara->straddr = qstraddr;
			sfpara->len = qact;
		}else{
			printf("<Modbus Serial Slave> The address have no contain\n");
			return -4;
		}
	 }
	
	return 0;
}
/*
 * Check query Slave ID correct or not
 * If wrong, return -1 the throw away it
 */
int ser_chk_dest(unsigned char *rx_buf, struct frm_para *fpara)
{
	unsigned int qslvID;
	unsigned int rslvID;
	
	qslvID = *(rx_buf);
	rslvID = fpara->slvID; 
	
	if(qslvID != rslvID){
		printf("<Modbus Serial> Slave ID improper, slaveID from query : %d | slaveID from setting : %d\n", qslvID, rslvID);
		return -1;
	}
	
	return 0;
}

int ser_resp_parser(unsigned char *rx_buf, struct frm_para *mfpara, int rlen)
{
	int i;
	int act_byte;
	unsigned char qfc;
	unsigned int qact;
	unsigned int qlen;
	unsigned int raddr;
	unsigned char rfc;
	unsigned int rrlen;
	unsigned int ract;
	
	qfc = mfpara->fc;
	qlen = mfpara->len;	
	rfc = *(rx_buf+1);
	rrlen = *(rx_buf+2);

	if(qfc ^ rfc){	
		if(rfc == READCOILSTATUS_EXCP){
			printf("<Modbus Serial Master> Read Coil Status (FC=01) exception !!\n");
			return -1;
		}
		if(rfc == READINPUTSTATUS_EXCP){
			printf("<Modbus Serial Master> Read Input Status (FC=02) exception !!\n");
			return -1;
		}
		if(rfc == READHOLDINGREGS_EXCP){
			printf("<Modbus Serial Master> Read Holding Registers (FC=03) exception !!\n");
			return -1;
		}
		if(rfc == READINPUTREGS_EXCP){
			printf("<Modbus Serial Master> Read Input Registers (FC=04) exception !!\n");
			return -1;
		}
		if(rfc == FORCESIGLEREGS_EXCP){
			printf("<Modbus Serial Master> Force Single Coil (FC=05) exception !!\n");
			return -1;
		}
		if(rfc == PRESETEXCPSTATUS_EXCP){
			printf("<Modbus Serial Master> Preset Single Register (FC=06) exception !!\n");
			return -1;
		}
		printf("<Modbus Serial Master> Uknow respond function code !!\n");
		return -1;
	}

	if(!(rfc ^ READCOILSTATUS) || !(rfc ^ READINPUTSTATUS)){	// fc = 0x01/0x02, get data len
		act_byte = carry((int)qlen, 8);

		if(rrlen != act_byte){
			printf("<Modbus Serial Master> length fault !!\n");
			return -1;
		}
		printf("<Modbus Serial Master> Data :");
		for(i = 3; i < rlen-2; i++){
			printf(" %x |", *(rx_buf+i));
		}
		printf("\n");
	}else if(!(rfc ^ READHOLDINGREGS) || !(rfc ^ READINPUTREGS)){	//fc = 0x03/0x04, get data byte
		if(rrlen != qlen<<1){
			printf("<Modbus Serial Master> byte fault !!\n");
			return -1;
		}
		printf("<Modbus Serial Master> Data : ");
		for(i = 3; i < rlen-2; i+=2){
			printf(" %x%x", *(rx_buf+i), *(rx_buf+i+1));
			printf("|");
		}
		printf("\n");
	}else if(!(rfc ^ FORCESIGLEREGS)){		//fc = 0x05, get action
		ract = *(rx_buf+4);
		raddr = *(rx_buf+2)<<8 | *(rx_buf+3);
		if(ract == 255){
			printf("<Modbus Serial Master> addr : %x The status to wirte on (FC:0x04)\n", raddr);
		}else if(!ract){
			printf("<Modbus Serial Master> addr : %x The status to wirte off (FC:0x04)\n", raddr);
		}else{
			printf("<Modbus Serial Master> Unknow action !!\n");
			return -1;
		}
	}else if(!(rfc ^ PRESETEXCPSTATUS)){	// fc = 0x06, get action
		qact = mfpara->act;
		raddr = *(rx_buf+2)<<8 | *(rx_buf+3);
		ract = *(rx_buf+4)<<8 | *(rx_buf+5);
		if(qact != ract){
			printf("<Modbus Serial Master> Action wrong !!\n");
			return -1;
		}
		printf("<Modbus Serial Master> addr : %x Action code = %x\n", raddr, ract);
	}else{
		printf("<Modbus Serial Master> Unknow respond function code = %x\n", rfc);
		return -1;
	}
	
	return 0;
}
/*
 * build modbus serial master mode Query
 */
int ser_build_query(unsigned char *tx_buf, struct frm_para *mfpara)
{
	int srclen;
	unsigned char src[FRMLEN];
	unsigned int slvID;
	unsigned int straddr;
	unsigned int act;
	unsigned char fc;
	
	slvID = mfpara->slvID;
	straddr = mfpara->straddr;
	act = mfpara->act;
	fc = mfpara->fc;

	src[0] = slvID;
	src[2] = straddr >> 8;
	src[3] = straddr & 0xff;
	src[4] = act >> 8;
	src[5] = act & 0xff;
	srclen = 6;
	
	switch(fc){
		case READCOILSTATUS:	// 0x01
			src[1] = READCOILSTATUS;
			printf("<Modbus Serial Master> build Read Coil Status query\n");
			break;
		case READINPUTSTATUS:	// 0x02
			src[1] = READINPUTSTATUS;
			printf("<Modbus Serial Master> build Read Input Status query\n");
			break;
		case READHOLDINGREGS:	// 0x03
			src[1] = READHOLDINGREGS;
			printf("<Modbus Serial Master> build Read Holding Register query\n");
			break;
		case READINPUTREGS:		// 0x04
			src[1] = READINPUTREGS;
			printf("<Modbus Serial Master> build Read Input Register query\n");
			break;
		case FORCESIGLEREGS:	// 0x05
			src[1] = FORCESIGLEREGS;
			printf("<Modbus Serial Master> build Force Sigle Coil query\n");
			break;
		case PRESETEXCPSTATUS:	// 0x06	
			src[1] = PRESETEXCPSTATUS;
			printf("<Modbus Serial Master> build Preset Single Register query\n");
			break;
		default:
			printf("<Modbus Serial Master> Unknow Function Code\n");
			break;
	}

	build_rtu_frm(tx_buf, src, srclen);
	srclen += 2;
	
	return srclen;
}
/* 
 * build modbus serial respond exception
 */
int ser_build_resp_excp(unsigned char *tx_buf, struct frm_para *sfpara, unsigned char excp_code)
{
	int src_num;
	unsigned int slvID;
	unsigned char fc;
	unsigned char src[FRMLEN];
	
	slvID = sfpara->slvID;
	fc = sfpara->fc;

	src[0] = slvID;
	src[1] = fc | EXCPTIONCODE;
	src[2] = excp_code;
	src_num = 3;

	printf("<Modbus Serial Slave> respond Excption Code\n");
	build_rtu_frm(tx_buf, src, src_num);
	src_num += 2;

	return src_num;
}

/* 
 * FC 0x01 Read Coil Status respond / FC 0x02 Read Input Status
 */
int ser_build_resp_read_status(unsigned char *tx_buf, struct frm_para *sfpara, unsigned char fc)
{
	int i;
	int byte;
	int src_len;
	unsigned int slvID;
	unsigned int len;	
	unsigned char src[FRMLEN];
	
	slvID = sfpara->slvID;
	len = sfpara->len; 
	byte = carry(len, 8);
	src_len = byte + 3;

	src[0] = slvID;	
	src[1] = fc;
	src[2] = byte & 0xff;	  
	for(i = 3; i < src_len; i++){	
		src[i] = 0;
	}
	
	printf("<Modbus Serial Slave> respond Read %s Status\n", fc==READCOILSTATUS?"Coil":"Input");

	build_rtu_frm(tx_buf, src, src_len);
	src_len += 2;
	
	return src_len;
} 
/* 
 * FC 0x03 Read Holding Registers respond / FC 0x04 Read Input Registers respond
 */
int ser_build_resp_read_regs(unsigned char *tx_buf, struct frm_para *sfpara, unsigned char fc)
{
	int i;
	int byte;
	int src_len;
	unsigned int slvID;
	unsigned int num_regs;
	unsigned char src[FRMLEN];

	slvID = sfpara->slvID;
	num_regs = sfpara->len;
	byte = num_regs * 2;
	src_len = byte + 3;

	src[0] = slvID;				 
	src[1] = fc;
	src[2] = byte & 0xff;
	for(i = 3; i < src_len; i++){   
		src[i] = 0;
	}

	printf("<Modbus Serial Slave> respond Read %s Registers\n", fc==READHOLDINGREGS?"Holding":"Input");
	
	build_rtu_frm(tx_buf, src, src_len);
	src_len += 2;
	
	return src_len;
}
/*
 * FC 0x05 Force Single Coli respond / FC 0x06 Preset Single Register respond
 */
int ser_build_resp_set_single(unsigned char *tx_buf, struct frm_para *sfpara, unsigned char fc)
{
	int src_len;
	unsigned int slvID;
	unsigned int straddr;
	unsigned int act;
	unsigned char src[FRMLEN];
	
	slvID = sfpara->slvID;
	straddr = sfpara->straddr;
	act = sfpara->act;
	
	/* init data, fill in slave addr & function code ##Start Addr */
	src[0] = slvID;
	src[1] = fc;
	src[2] = straddr >> 8;			// data addr Hi
	src[3] = straddr;				// data addr Lo
	src[4] = act >> 8;				// active Hi
	src[5] = act;					// active Lo
	src_len = 6;

	printf("<Modbus Serial Slave> respond %s\n", fc==FORCESIGLEREGS?"Force Single Coli":"Preset Single Register");
	
	build_rtu_frm(tx_buf, src, src_len);
	src_len += 2;
	
	return src_len;
}
