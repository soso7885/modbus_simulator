#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <endian.h> 

#include "mbus.h"
/*
 * Analyze modebus TCP query
 */
int tcp_query_parser(unsigned char *rx_buf, struct tcp_frm_para *tsfpara)
{
	unsigned short tmp16;
	unsigned char tmp8;
	unsigned short qtransID;
	unsigned short qmsglen;
	unsigned char qfc, rfc;
	unsigned short qstraddr, rstraddr;
	unsigned short qact, rlen;

	memcpy(&tmp16, rx_buf, sizeof(tmp16));
	qtransID = ntohs(tmp16);
	tsfpara->transID = qtransID;
	memcpy(&tmp16, rx_buf+4, sizeof(tmp16));
	qmsglen = ntohs(tmp16);
	memcpy(&tmp8, rx_buf+7, sizeof(tmp8));
	qfc = le16toh(tmp8);
	memcpy(&tmp16, rx_buf+8, sizeof(tmp16));
	qstraddr = ntohs(tmp16);
	memcpy(&tmp16, rx_buf+10, sizeof(tmp16));
	qact = ntohs(tmp16);

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
        if(!qact || qact == 255){
			tsfpara->act = qact;
        }else{
            printf("<Modbus TCP Slave> Query set the status to write fuckin worng\n");
			return -3;                          
        }
    }else if(!(rfc ^ PRESETEXCPSTATUS)){        // FC = 0x06, get the value to write
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
 * Check query transaction ID/Portocol ID/Unit ID correct or not. If wrong, return -1 then throw away it !
 */
int tcp_chk_pack_dest(unsigned char *rx_buf, struct tcp_frm_para *tfpara)
{
	unsigned short tmp16;
	unsigned char tmp8;
	unsigned short qpotoID;
	unsigned char qunitID, runitID;
	
	memcpy(&tmp16, rx_buf+2, sizeof(tmp16));
	qpotoID = ntohs(tmp16);
	
	memcpy(&tmp8, rx_buf+6, sizeof(tmp8));
	qunitID = le16toh(tmp8);
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
	unsigned char tmp8;
	unsigned short tmp16;
	unsigned char qfc, rfc;
	unsigned short qact, ract;
	unsigned short qlen;
	unsigned short raddr;
	unsigned short rrlen;
	
	qfc = tmfpara->fc;
	memcpy(&tmp8, rx_buf+7, sizeof(tmp8));
	rfc = le16toh(tmp8);
	qlen = tmfpara->len;
	memcpy(&tmp8, rx_buf+8, sizeof(tmp8));
	rrlen = le16toh(tmp8);
	
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
		memcpy(&tmp8, rx_buf+10, sizeof(tmp8));
		ract = le16toh(tmp8);
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
int tcp_build_query(unsigned char *tx_buf, struct tcp_frm_para *tmfpara)
{
	unsigned short tmp16; 	
	unsigned char tmp8;

	tmfpara->msglen = TCPQUERYMSGLEN; 
	
	tmp16 = htons(tmfpara->transID);
	memcpy(tx_buf, &tmp16, sizeof(tmp16));
	
	tmp16 = htons(tmfpara->potoID);
	memcpy(tx_buf+2, &tmp16, sizeof(tmp16));
	
	tmp16 = htons(tmfpara->msglen);
	memcpy(tx_buf+4, &tmp16, sizeof(tmp16));
	
	tmp8 = htole16(tmfpara->unitID);
	memcpy(tx_buf+6, &tmp8, sizeof(tmp8));

	tmp8 = htole16(tmfpara->fc);
	memcpy(tx_buf+7, &tmp8, sizeof(tmp8));

	tmp16 = htons(tmfpara->straddr);
	memcpy(tx_buf+8, &tmp16, sizeof(tmp16));

	if(tmfpara->fc == 5 || tmfpara->fc == 6){
		tmp16 = htons(tmfpara->act);
		memcpy(tx_buf+10, &tmp16, sizeof(tmp16));
	}else{
		tmp16 = htons(tmfpara->len);
		memcpy(tx_buf+10, &tmp16, sizeof(tmp16));
	}
	memset(tx_buf+12, '\0', 1);

	return 0;
}
/*
 * build modbus TCP respond exception
 */
int tcp_build_resp_excp(unsigned char *tx_buf, struct tcp_frm_para *tsfpara, unsigned char excp_code)
{
	int txlen;
	unsigned char excpfc;
	unsigned char tmp8;
	unsigned short tmp16;

	tsfpara->msglen = TCPQUERYMSGLEN;
	
	tmp16 = htons(tsfpara->transID);
	memcpy(tx_buf, &tmp16, sizeof(tmp16));
	
	tmp16 = htons(tsfpara->potoID);
	memcpy(tx_buf+2, &tmp16, sizeof(tmp16));
	
	tmp16 = htons(tsfpara->msglen);
	memcpy(tx_buf+4, &tmp16, sizeof(tmp16));
	
	tmp8 = htole16(tsfpara->unitID);
	memcpy(tx_buf+6, &tmp8, sizeof(tmp8));
	
	excpfc = tsfpara->fc | EXCPTIONCODE;
	tmp8 = htole16(excpfc);
	memcpy(tx_buf+7, &tmp8, sizeof(tmp8));
	
	tmp8 = htole16(excp_code);
	memcpy(tx_buf+8, &tmp8, sizeof(tmp8));

	txlen = TCPRESPEXCPLEN; 

	printf("<Modbus TCP Slave> respond Excption Code");
		
	return txlen;
}
/*
 * FC 0x01 Read Coil Status respond / FC 0x02 Read Input Status
 */
int tcp_build_resp_read_status(unsigned char *tx_buf, struct tcp_frm_para *tsfpara, unsigned char fc)
{
    int i;
    int byte;
    int txlen;
	unsigned char tmp8;
	unsigned short tmp16;
	unsigned short msglen;
    unsigned short len;   
    
	len = tsfpara->len;
	byte = carry((int)len, 8);
	txlen = byte + 9;
	msglen = byte + 2;	
	tsfpara->msglen = msglen;

	tmp16 = htons(tsfpara->transID);
	memcpy(tx_buf, &tmp16, sizeof(tmp16));
	
	tmp16 = htons(tsfpara->potoID);
	memcpy(tx_buf+2, &tmp16, sizeof(tmp16));
	
	tmp16 = htons(tsfpara->msglen);
	memcpy(tx_buf+4, &tmp16, sizeof(tmp16));

	memcpy(tx_buf+6, &tsfpara->unitID, sizeof(tsfpara->unitID));
	memcpy(tx_buf+7, &fc, sizeof(fc));
	
	tmp8 = htole16((unsigned char)byte);
	memcpy(tx_buf+8, &tmp8, sizeof(tmp8));

	tmp8 = 0;
	for(i = 9; i < txlen; i++){
		memcpy(tx_buf+i, &tmp8, sizeof(tmp8));
	}
	
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
int tcp_build_resp_read_regs(unsigned char *tx_buf, struct tcp_frm_para *tsfpara, unsigned char fc)
{
    int i;                                                                                                                                                                                                
    int byte;                                                                                                                                                                                             
    int txlen;
	unsigned int num_regs;
    unsigned char tmp8;
    unsigned short tmp16;
    unsigned short msglen;
    
    num_regs = tsfpara->len;
    byte = num_regs * 2;

    txlen = byte + 9;
    msglen = byte + 2;  
    tsfpara->msglen = msglen;

    tmp16 = htons(tsfpara->transID);
    memcpy(tx_buf, &tmp16, sizeof(tmp16));
    
    tmp16 = htons(tsfpara->potoID);
    memcpy(tx_buf+2, &tmp16, sizeof(tmp16));
    
    tmp16 = htons(tsfpara->msglen);
    memcpy(tx_buf+4, &tmp16, sizeof(tmp16));

	tmp8 = htole16(tsfpara->unitID);
    memcpy(tx_buf+6, &tmp8, sizeof(tmp8));
	
	tmp8 = htole16(fc);
    memcpy(tx_buf+7, &tmp8, sizeof(tmp8));
    
	tmp8 = htole16((unsigned char)byte);
    memcpy(tx_buf+8, &tmp8, sizeof(tmp8));

    tmp16 = 0;
    for(i = 9; i < txlen; i+=2){					// a register content need 2 byte
        memcpy(tx_buf+i, &tmp16, sizeof(tmp16));
    }
    
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
int tcp_build_resp_set_single(unsigned char *tx_buf, struct tcp_frm_para *tsfpara, unsigned char fc)
{
    int txlen;
    unsigned char tmp8;
    unsigned short tmp16;

    tsfpara->msglen = (unsigned short)TCPRESPSETSIGNALLEN;
	
	txlen = TCPRESPSETSIGNALLEN + 6;

    tmp16 = htons(tsfpara->transID);
    memcpy(tx_buf, &tmp16, sizeof(tmp16));
    
    tmp16 = htons(tsfpara->potoID);
    memcpy(tx_buf+2, &tmp16, sizeof(tmp16));
    
    tmp16 = htons(tsfpara->msglen);
    memcpy(tx_buf+4, &tmp16, sizeof(tmp16));

	tmp8 = htole16(tsfpara->unitID);
    memcpy(tx_buf+6, &tmp8, sizeof(tmp8));
   
	tmp8 = htole16(fc);
    memcpy(tx_buf+7, &tmp8, sizeof(tmp8));

	tmp16 = htons(tsfpara->straddr);
	memcpy(tx_buf+8 ,&tmp16, sizeof(tmp16));
 
    tmp16 = htons(tsfpara->act);
    memcpy(tx_buf+10, &tmp16, sizeof(tmp16));
    
    if(fc == FORCESIGLEREGS){
        printf("<Modbus TCP Slave> respond Force Single Coli\n");
	}else{
        printf("<Modbus TCP Slave> respond Preset Single Register\n");
    }  
 
    return txlen;
}








