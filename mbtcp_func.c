#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <endian.h>

#include "mbus.h"
/*
 * Analyze modebus TCP query
 */
int tcp_query_parser(struct tcp_frm *rx_buf, struct tcp_frm_para *tsfpara)
{
	unsigned short qtransID;
	unsigned short qmsglen;
	unsigned char qfc, rfc;
	unsigned short qstraddr, rstraddr;
	unsigned short qact, rlen;
	
	qtransID = be16toh(rx_buf->transID);
	tsfpara->transID = qtransID;

	qmsglen = be16toh(rx_buf->msglen);
	qfc = rx_buf->fc;
	qstraddr = be16toh(rx_buf->straddr);
	qact = be16toh(rx_buf->act);

	rfc = tsfpara->fc;
	rstraddr = tsfpara->straddr;
	rlen = tsfpara->len;

	if(qmsglen != TCPQUERYMSGLEN){
		printf("<Modbus TCP Slave> Modbus TCP message length should be 6 byte !!\n");
		return -4;
	}

	if(qfc != rfc){
		printf("<Modbus TCP Slave> Modbus TCP function code improper !!\n");
		return -1;
	}
	
	if(!(rfc ^ FORCESIGLEREGS)){                // FC = 0x05, get the status to write(on/off)
		if(!qact || qact == 0xff<<8){
			tsfpara->act = qact;
		}else{
			printf("<Modbus TCP Slave> Query set the status to write fuckin worng(fc = 0x05)\n");
			return -3;						  
		}
		if(qstraddr != rstraddr){
			printf("<Modbus TCP Slave> Query register address wrong (fc = 0x05)");
			printf(", query addr : %x | resp addr : %x\n", qstraddr, rstraddr);
			return -2;
		}
	}else if(!(rfc ^ PRESETEXCPSTATUS)){        // FC = 0x06, get the value to write
		if(qstraddr != rstraddr){
			printf("<Modbus TCP Slave> Query register address wrong (fc = 0x06)");
			printf(", query addr : %x | resp addr : %x\n", qstraddr, rstraddr);
			return -2;
		}
		tsfpara->act = qact;
	}else{
		if((qstraddr + qact <= rstraddr + rlen) && (qstraddr >= rstraddr)){ // Query addr+shift len must smaller than the contain we set in addr+shift len
			tsfpara->straddr = qstraddr;
			tsfpara->len = qact;
		}else{
			printf("<Modbus TCP Slave> The address have no contain\n");
			printf("Query addr : %x, shift len : %x | Respond addr: %x, shift len : %x\n",
					 qstraddr, qact, rstraddr, rlen);
			return -2;
		}
	}

	return 0;
}
/* 
 * Check query transaction ID/Portocol ID/Unit ID correct or not. 
 * If wrong, return -1 then throw away it !
 */
int tcp_chk_pack_dest(struct tcp_frm *rx_buf, struct tcp_frm_para *tfpara)
{
	unsigned short qpotoID;
	unsigned char qunitID, runitID;
	
	qpotoID = be16toh(rx_buf->potoID);
	qunitID = rx_buf->unitID;

	runitID = tfpara->unitID;
		
	if(qpotoID != (unsigned short)TCPMBUSPROTOCOL){
		printf("<Modbus TCP Slave> recv query protocol ID wrong !!\n");
		return -1;
	}
	
	if(qunitID != runitID){
		printf("<Modbus TCP Slave> the destination of recv query wrong !!(unit ID) : ");
		printf("Query unitID : %x | Respond unitID : %x\n", qunitID, runitID);
		return -1;
	}
	
	return 0;
}
/*
 * Analyze modbus TCP respond
 */
int tcp_resp_parser(unsigned char *rx_buf, struct tcp_frm_para *tmfpara, int rlen)
{
	int i;
	int act_byte;
	unsigned short tmp16;
	unsigned char qfc, rfc;
	unsigned short qact, ract;
	unsigned short qlen;
	unsigned short raddr;
	unsigned short rrlen;
	
	qfc = tmfpara->fc;
	rfc = *(rx_buf+7);
	qlen = tmfpara->len;
	rrlen = *(rx_buf+8);
	
	if(qfc != rfc){
		if(rfc == READCOILSTATUS_EXCP){
			printf("<Modbus TCP Master> Read Coil Status (FC=01) exception !!\n");
			return -1;
		}else if(rfc == READINPUTSTATUS_EXCP){
			printf("<Modbus TCP Master> Read Input Status (FC=02) exception !!\n");
			return -1;
		}else if(rfc == READHOLDINGREGS_EXCP){
			printf("<Modbus TCP Master> Read Holding Registers (FC=03) exception !!\n");
			return -1;
		}else if(rfc == READINPUTREGS_EXCP){
			printf("<Modbus TCP Master> Read Input Registers (FC=04) exception !!\n");
			return -1;
		}else if(rfc == FORCESIGLEREGS_EXCP){
			printf("<Modbus TCP Master> Force Single Coil (FC=05) exception !!\n");
			return -1;
		}else if(rfc == PRESETEXCPSTATUS_EXCP){
			printf("<Modbus TCP Master> Preset Single Register (FC=06) exception !!\n");
			return -1;
		}else{
			printf("<Modbus TCP Master> unknown respond function code : %x !!\n", rfc);
			return -1;
		}
	}
	
	if(!(rfc ^ READCOILSTATUS) || !(rfc ^ READINPUTSTATUS)){		// fc = 0x01/0x02, get data len
		act_byte = carry((int)qlen, 8);

		if(rrlen != act_byte){
			printf("<Modbus TCP Master> recv respond length wrong (rlen = %d | qlen = %d)\n", rrlen, act_byte);
			return -1;
		}
		printf("<Modbus TCP Master> Data : ");
		for(i = 9; i < rlen; i++){
			printf(" %x |", *(rx_buf+i));
		}
		printf("\n");
	}else if(!(rfc ^ READHOLDINGREGS) || !(rfc ^ READINPUTREGS)){	// fc = 0x03/0x04, get data byte
		if(rrlen != qlen << 1){
			printf("<Modbus TCP Master> recv respond byte wrong !!\n");
			return -1;
		}
		printf("<Modbus TCP Master> Data : ");
		for(i = 9; i < rlen; i+=2){
			printf(" %x%x |", *(rx_buf+i), *(rx_buf+i+1));
		}
		printf("\n");
	}else if(!(rfc ^ FORCESIGLEREGS)){								// fc = 0x05, get write on/off status
		memcpy(&tmp16, rx_buf+8, sizeof(tmp16));
		raddr = ntohs(tmp16);
		ract = *(rx_buf+10);
		if(ract == 255){
			printf("<Modbus TCP Master> addr : %x The status to wirte on (FC:0x04)\n", raddr);
		}else if(!ract){
			printf("<Modbus TCP Master> addr : %x The status to wirte off (FC:0x04)\n", raddr);
		}else{
			printf("<Modbus TCP Master> Unknown status (FC:0x04)\n");
			return -1;
		}
	}else if(!(rfc ^ PRESETEXCPSTATUS)){							// fc = 0x06, get status on register
		qact = tmfpara->act;
		memcpy(&tmp16, rx_buf+8, sizeof(tmp16));
		raddr = ntohs(tmp16);
		memcpy(&tmp16, rx_buf+10, sizeof(tmp16));
		ract = ntohs(tmp16);
		if(qact != ract){
			printf("<Modbus TCP Master> Action fail (FC:0x06)\n");
			return -1;
		}
		printf("<Modbus TCP Master> addr : %x Action code : %x\n", raddr, ract);
	}else{															// fc = Unknown 
		printf("<Modbus TCP Master> Unknown Function code %x !!\n", rfc);
		return -1;
	}
	
	return 0;
}
/*
 * build Modbus TCP query
 */			
int tcp_build_query(struct tcp_frm *tx_buf, struct tcp_frm_para *tmfpara)
{
	tx_buf->transID = htons(tmfpara->transID);
	tx_buf->potoID = htons(tmfpara->potoID);
	tx_buf->msglen = htons((unsigned short)TCPQUERYMSGLEN);
	tx_buf->unitID = tmfpara->unitID;
	tx_buf->fc = tmfpara->fc;
	tx_buf->straddr = htons(tmfpara->straddr);
	if(tmfpara->fc == 5 || tmfpara->fc == 6){
		tx_buf->act = htons(tmfpara->act);
	}else{
		tx_buf->act = htons(tmfpara->len);
	}

	return 0;
}
/*
 * build modbus TCP respond exception
 */
int tcp_build_resp_excp(struct tcp_frm_excp *tx_buf, struct tcp_frm_para *tsfpara, unsigned char excp_code)
{
	int txlen;
	unsigned short msglen;
	unsigned char excpfc;	

	msglen = (unsigned short)TCPRESPEXCPMSGLEN;

	tx_buf->transID = htons(tsfpara->transID);
	tx_buf->potoID = htons(tsfpara->potoID);
	tx_buf->msglen = htons(msglen);
	tx_buf->unitID = tsfpara->unitID;
	excpfc = tsfpara->fc | EXCPTIONCODE;
	tx_buf->fc = excpfc;	
    tx_buf->ec = excp_code;	

	txlen = TCPRESPEXCPFRMLEN; 

	printf("<Modbus TCP Slave> respond Excption Code");
        
	return txlen;
}
/*
 * FC 0x01 Read Coil Status respond / FC 0x02 Read Input Status
 */
int tcp_build_resp_read_status(struct tcp_frm_rsp *tx_buf, struct tcp_frm_para *tsfpara, unsigned char fc)
{
	int byte;
	int txlen;
	unsigned short msglen;
	unsigned short len;   
	
	len = tsfpara->len;
	byte = carry((int)len, 8);
	txlen = byte + 9;
	msglen = byte + 2;	
	tsfpara->msglen = msglen;

	tx_buf->transID = htons(tsfpara->transID);
	tx_buf->potoID = htons(tsfpara->potoID);
	tx_buf->msglen = htons(tsfpara->msglen);
	tx_buf->unitID = tsfpara->unitID;
	tx_buf->fc = fc;
	tx_buf->byte = (unsigned char)byte;

	memset(tx_buf+1, 0, byte);		// tx_buf+1 shift a size of sturct "tcp_frm_rsp" (9 byte)

	if(fc == READCOILSTATUS){
		printf("<Modbus TCP Slave> respond Read Coil Status\n");
	}else{
		printf("<Modbus TCP Slave> respond Read Input Status\n");
	}
   
	return txlen;
}
/*
 * FC 0x03 Read Holding Registers respond / FC 0x04 Read Input Registers respond
 */ 
int tcp_build_resp_read_regs(struct tcp_frm_rsp *tx_buf, struct tcp_frm_para *tsfpara, unsigned char fc)
{
	int byte;
	int txlen;
	unsigned int num_regs;
	unsigned short msglen;

	num_regs = tsfpara->len;
	byte = num_regs * 2;

	txlen = byte + 9;
	msglen = byte + 2;  
	tsfpara->msglen = msglen;

	tx_buf->transID = htons(tsfpara->transID);
	tx_buf->potoID = htons(tsfpara->potoID);
	tx_buf->msglen = htons(tsfpara->msglen);
	tx_buf->unitID = tsfpara->unitID;
	tx_buf->fc = fc;
	tx_buf->byte = (unsigned char)byte;
		
	memset(tx_buf+1, 0, byte);		// tx_buf+1 shift a size of struct "tcp_frm_rsp" (9 byte)
	

	
	if(fc == READHOLDINGREGS){
		printf("<Modbus TCP Slave> respond Read Holding Registers\n");
	}else{
		printf("<Modbus TCP Slave> respond Read Input Registers\n");
	}
   
	return txlen;
}
/* 
 * FC 0x05 Force Single Coli respond / FC 0x06 Preset Single Register respond
 */ 
int tcp_build_resp_set_single(struct tcp_frm *tx_buf, struct tcp_frm_para *tsfpara, unsigned char fc)
{
	int txlen;

	tsfpara->msglen = (unsigned short)TCPRESPSETSIGNALLEN;
	
	txlen = TCPRESPSETSIGNALLEN + 6;

	tx_buf->transID = htons(tsfpara->transID);
	tx_buf->potoID = htons(tsfpara->potoID);
	tx_buf->msglen = htons(tsfpara->msglen);
	tx_buf->unitID = tsfpara->unitID;
	tx_buf->fc = fc;
	tx_buf->straddr = htons(tsfpara->straddr);
	tx_buf->act = htons(tsfpara->act);
	
	if(fc == FORCESIGLEREGS){
		printf("<Modbus TCP Slave> respond Force Single Coli\n");
	}else{
		printf("<Modbus TCP Slave> respond Preset Single Register\n");
	}  
 
	return txlen;
}








