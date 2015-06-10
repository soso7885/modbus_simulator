#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <endian.h> 

#include "mbus.h"

int tcp_query_parser(unsigned char *rx_buf, struct tcp_frm_para *tsfpara)                                                                      {
	unsigned short qtransID, rtransID;
	unsigned short qpotoID;
	unsigned short qmsglen;
	unsigned short qunitID, runitID;
	unsigned char qfc, rfc;
	unsigned short qstraddr, rstraddr;
	unsigned short qact, rlen;

    qtransID = *rx_buf << 8 | *(rx_buf+1);
    qpotoID = *(rx_buf+2) << 8 | *(rx_buf+3);
    qmsglen = *(rx_buf+4) << 8 | *(rx_buf+5);
    qunitID = *(rx_buf+6);
    qfc = *(rx_buf+7);
    qstraddr = *(rx_buf+8) << 8 | *(rx_buf+9);
    qact = *(rx_buf+10) << 8 | *(rx_buf+11);
    
    rtransID = tsfpara->transID;
    runitID = tsfpara->unitID;
    rfc = tsfpara->fc;
	rstraddr = tsfpara->straddr;
	rlen = tsfpara->len;
    
    if(qtransID != rtransID){
        printf("<Modbus TCP Slave> Modbus TCP transaction ID improper !!\n");
		return -1;
    }
    
    if(qpotoID != TCPMBUSPROTOCOL){
        printf("<Modbus TCP Slave> Modbus TCP protocol ID should be 0 !!\n");
		return -2;
    }
    
    if(qmsglen != TCPQUERYMSGLEN){
        printf("<Modbus TCP Slave> Modbus TCP message length should be 6 byte !!\n");
		return -3;
    }

    if(qunitID != runitID){
        printf("<Modbus TCP Slave> Modbus TCP unit ID improper !!\n");
		return -4;
    }

	if(qfc != rfc){
        printf("<Modbus TCP Slave> Modbus TCP function code improper !!\n");
		return -5;
    }
    
    if(!(rfc ^ FORCESIGLEREGS)){                // FC = 0x05, get the status to write(on/off)                                                                                                             
        if(!qact || qact == 255){
			tsfpara->act = qact;
        }else{
            printf("<Slave mode> Query set the status to write fuckin worng\n");
			return -6;                          // the other fuckin respond excp code?
        }
    }else if(!(rfc ^ PRESETEXCPSTATUS)){        // FC = 0x06, get the value to write
        tsfpara->act = qact;
    }else{
        if(qstraddr + qact <= rstraddr + rlen){ // Query addr+shift len must smaller than the contain we set in addr+shift len
            tsfpara->straddr = qstraddr;
            tsfpara->len = qact;
		}else{
			printf("<Slave mode> The address have no contain\n");
			return -5;
		}
	}

	return 0;
}

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
	
	tmp8 = htons(tmfpara->unitID);	
	memcpy(tx_buf+6, &tmp8, sizeof(tmp8));
	
	tmp8 = htons(tmfpara->fc);
	memcpy(tx_buf+7, &tmp8, sizeof(tmp8));

	tmp16 = htons(tmfpara->straddr);
	memcpy(tx_buf+8, &tmp16, sizeof(tmp16));

	tmp16 = htons(tmfpara->act);
	memcpy(tx_buf+10, &tmp16, sizeof(tmp16));

	return 0;
}
/*
 * build modbus TCP respond exception
 */
int tcp_build_resp_excp(struct tcp_frm_para *tsfpara, unsigned char excp_code, unsigned char *tx_buf)
{
	int txlen;
	unsigned char excp_fc;
	unsigned char tmp8;
	unsigned short tmp16;

	tsfpara->msglen = TCPQUERYMSGLEN;
	
	tmp16 = htons(tsfpara->transID);
	memcpy(tx_buf, &tmp16, sizeof(tmp16));
	
	tmp16 = htons(tsfpara->potoID);
	memcpy(tx_buf+2, &tmp16, sizeof(tmp16));
	
	tmp16 = htons(tsfpara->msglen);
	memcpy(tx_buf+4, &tmp16, sizeof(tmp16));
	
	tmp8 = htons(tsfpara->unitID);
	memcpy(tx_buf+6, &tmp8, sizeof(tmp8));
	
	excp_fc = tsfpara->fc | EXCPTIONCODE;
	tmp8 = hotns(excp_fc);
	memcpy(tx_buf+7, &tmp8, sizeof(tmp8));
	
	tmp8 = htons(excp_code);
	memcpy(tx_buf+8, &tmp8, sizeof(tmp8));

	txlen = TCPRESPEXCPLEN; 

	printf("<Modbus TCP Slave> respond Excption Code");
		
	return txlen;
}

/*
 * FC 0x01 Read Coil Status respond / FC 0x02 Read Input Status
 */
int tcp_build_resp_read_status(struct tcp_frm_para *tsfpara, unsigned char *tx_buf, unsigned char fc)
{
    int i;
    int byte;
	int res;
    int txlen;
	unsigned char tmp8;
	unsigned short tmp16;
	unsigned short msglen;
//  unsigned int straddr;
    unsigned int len;   
    
	len = tsfpara->len;
	res = len % 8;
	byte = len / 8;
	if(res > 0){
		byte += 1;
	}

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
	
	tmp8 = htons((unsigned char)byte);
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
int tcp_build_resp_read_regs(struct tcp_frm_para *tsfpara, unsigned char *tx_buf, unsigned char fc)
{
    int i;                                                                                                                                                                                                
    int byte;                                                                                                                                                                                             
    int txlen;
	unsigned int num_regs;
    unsigned char tmp8;
    unsigned short tmp16;
    unsigned short msglen;
//  unsigned int straddr;
   
    
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

	tmp8 = htons(tsfpara->unitID);
    memcpy(tx_buf+6, &tmp8, sizeof(tmp8));
	
	tmp8 = htons(fc);
    memcpy(tx_buf+7, &tmp8, sizeof(tmp8));
    
    tmp8 = htons((unsigned char)byte);
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
int tcp_build_resp_set_single(struct tcp_frm_para *tsfpara, unsigned char *tx_buf, unsigned char fc)
{
    int txlen;
    unsigned char tmp8;
    unsigned short tmp16;
//  unsigned int straddr;

    tsfpara->msglen = (unsigned short)TCPRESPSETSIGNALLEN;
	
	txlen = TCPRESPSETSIGNALLEN + 6;

    tmp16 = htons(tsfpara->transID);
    memcpy(tx_buf, &tmp16, sizeof(tmp16));
    
    tmp16 = htons(tsfpara->potoID);
    memcpy(tx_buf+2, &tmp16, sizeof(tmp16));
    
    tmp16 = htons(tsfpara->msglen);
    memcpy(tx_buf+4, &tmp16, sizeof(tmp16));

    tmp8 = htons(tsfpara->unitID);
    memcpy(tx_buf+6, &tmp8, sizeof(tmp8));
    
    tmp8 = htons(fc);
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








