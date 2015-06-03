#include <stdio.h>
#include <stdlib.h>

#include "mbus.h"

int analz_query(unsigned char *rx_buf, struct frm_para *sfpara)
{
	unsigned int qslvID;
	unsigned int rslvID;
	unsigned char qfc;
	unsigned char rfc;
	unsigned int qstraddr;
	unsigned int rstraddr;
	unsigned int qact;
	unsigned int rlen;
	
	qslvID = *(rx_buf);
	qfc = *(rx_buf+1);
	qstraddr = *(rx_buf+2)<<8 | *(rx_buf+3);
	qact = *(rx_buf+4)<<8 | *(rx_buf+5);
	rslvID = sfpara->slvID;
	rfc = sfpara->fc;
	rstraddr = sfpara->straddr;
	rlen = sfpara->len;

	if(rslvID != qslvID){								// check master & slave mode slvId
		printf("<Slave mode> Slave ID improper, slaveID from query : %d | slaveID from setting : %d\n", qslvID, rslvID);
		return -1;
	}

	if(rfc != qfc){
		printf("<Slave mode> Function code improper\n");
		return -2;
	}
	
	if(!(rfc ^ FORCESIGLEREGS)){				// FC = 0x05, get the status to write(on/off)
		if(!qact || qact == 255){
			sfpara->act = qact;
		}else{
			printf("<Slave mode> Query set the status to write fuckin worng\n");
			return -2;		// the other fuckin respond excp code?
		}
	}else if(!(rfc ^ PRESETEXCPSTATUS)){		// FC = 0x06, get the value to write
		sfpara->act = qact;
	}else{
		if(qstraddr + qact <= rstraddr + rlen){	// Query addr+shift len must smaller than the contain we set in addr+shift len
			sfpara->straddr = qstraddr;
			sfpara->len = qact;
		}else{
			printf("<Slave mode> The address have no contain\n");
			return -3;
		}
	}
	
	return 0;
}

int analz_respond(unsigned char *rx_buf, struct frm_para *mfpara, int rlen)
{
	int i;
	int act_tmp;
	int tmp;
	unsigned int qslvID;
	unsigned char qfc;
	unsigned int qact;
	unsigned int raddr;
	unsigned int rslvID;
	unsigned char rfc;
	unsigned int rrlen;
	unsigned int ract;
	
	qslvID = mfpara->slvID;
	qfc = mfpara->fc;
	qact = mfpara->act;
	rslvID = *(rx_buf);	
	rfc = *(rx_buf+1);
	rrlen = *(rx_buf+2);

	if(qslvID ^ rslvID){		// check slave ID
		printf("<Master mode> Slave ID improper !!\n");
		return -1;
	}

	if(qfc ^ rfc){			// check excption
		if(rfc == READCOILSTATUS_EXCP){
			printf("<Master mode> Read Coil Status (FC=01) exception !!\n");
			return -1;
		}
		if(rfc == READINPUTSTATUS_EXCP){
            printf("<Master mode> Read Input Status (FC=02) exception !!\n");
            return -1;
        }
		if(rfc == READHOLDINGREGS_EXCP){
            printf("<Master mode> Read Holding Registers (FC=03) exception !!\n");
            return -1;
        }
		if(rfc == READINPUTREGS_EXCP){
            printf("<Master mode> Read Input Registers (FC=04) exception !!\n");
            return -1;
        }
		if(rfc == FORCESIGLEREGS_EXCP){
            printf("<Master mode> Force Single Coil (FC=05) exception !!\n");
            return -1;
        }
		if(rfc == PRESETEXCPSTATUS_EXCP){
            printf("<Master mode> Preset Single Register (FC=06) exception !!\n");
            return -1;
        }
		printf("<Master mode> Uknow respond function code !!\n");
		return -1;
	}

	if(!(rfc ^ READCOILSTATUS) || !(rfc ^ READINPUTSTATUS)){	// fc = 0x01/0x02, get data len
		act_tmp = qact / 8;
		tmp = qact % 8;
		if(tmp > 0){
			act_tmp++;
		}
		if(rrlen != act_tmp){
			printf("<Master mode> length fault !!\n");
			return -1;
		}
		printf("<Master mode> Data :");
		for(i = 3; i < rlen-2; i++){
			printf(" %x |", *(rx_buf+i));
		}
		printf("\n");
	}else if(!(rfc ^ READHOLDINGREGS) || !(rfc ^ READINPUTREGS)){	//fc = 0x03/0x04, get data byte
		if(rrlen != qact<<1){
			printf("<Master mode> byte fault !!\n");
			return -1;
		}
		printf("<Master mode> Data : ");
		for(i = 3; i < rlen-2; i+=2){
			printf(" %x%x", *(rx_buf+i), *(rx_buf+i+1));
			printf("|");
		}
		printf("\n");
	}else if(!(rfc ^ FORCESIGLEREGS)){		//fc = 0x05, get action
		ract = *(rx_buf+4);
		raddr = *(rx_buf+2)<<8 | *(rx_buf+3);
		if(ract){
			printf("<Master mode> addr : %x The status to wirte on (FC:0x04)\n", raddr);
		}else if(!ract){
			printf("<Master mode> addr : %x The status to wirte off (FC:0x04)\n", raddr);
		}else{
			printf("<Master mode> Unknow action !!\n");
			return -1;
		}
	}else if(!(rfc ^ PRESETEXCPSTATUS)){	// fc = 0x06, get action
		ract = *(rx_buf+4)<<8 | *(rx_buf+5);
		printf("<Master mode> Action code = %x\n", ract);
	}else{
		printf("<Master mode> Unknow respond function code = %x\n", rfc);
		return -1;
	}
	
	return 0;
}
/*
 * build modbus master mode Query
 */
int build_query(unsigned char *tx_buf, struct frm_para *mfpara)
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
			printf("<Master mode> build Read Coil Status query\n");
			break;
		case READINPUTSTATUS:	// 0x02
			src[1] = READINPUTSTATUS;
			printf("<Master mode> build Read Input Status query\n");
			break;
		case READHOLDINGREGS:	// 0x03
			src[1] = READHOLDINGREGS;
			printf("<Master mode> build Read Holding Register query\n");
			break;
		case READINPUTREGS:		// 0x04
			src[1] = READINPUTREGS;
			printf("<Master mode> build Read Input Register query\n");
			break;
		case FORCESIGLEREGS:	// 0x05
			src[1] = FORCESIGLEREGS;
			printf("<Master mode> build Force Sigle Coil query\n");
			break;
		case PRESETEXCPSTATUS:	// 0x06	
			src[1] = PRESETEXCPSTATUS;
			printf("<Master mode> build Preset Single Register query\n");
			break;
		default:
			printf("<Master mode> Unknow Function Code\n");
			break;
	}

	build_rtu_frm(tx_buf, src, srclen);
	srclen += 2;
	
	return srclen;
}

int build_resp_excp(struct frm_para *sfpara, unsigned int excp_code, unsigned char *tx_buf)
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

    printf("<Slave mode> respond Excption Code\n");
    build_rtu_frm(tx_buf, src, src_num);
    src_num += 2;

    return src_num;
}

/* 
 * FC 0x01 Read Coil Status respond / FC 0x02 Read Input Status
 */
int build_resp_read_status(struct frm_para *sfpara, unsigned char *tx_buf, unsigned char fc)
{
    int i;
	int byte;
	int res;
    int src_len;
	unsigned int slvID;
	unsigned int straddr;
	unsigned int len;	
    unsigned char src[FRMLEN];
    
	slvID = sfpara->slvID;
	straddr = sfpara->straddr;
	len = sfpara->len; 
	res = len % 8;
	byte = len / 8;	
	if(res > 0){
		byte += 1;
	}
    src_len = byte + 3;

    src[0] = slvID;           		// Slave ID
	src[1] = fc;					// Function code
	src[2] = byte & 0xff;       	// The number of data byte to follow
	for(i = 3; i < src_len; i++){	
//		src[i] = (straddr%229 + i) & 0xff;	// The Coil Status addr status, we set random value (0 ~ 240) 
		src[i] = 0;
    }
	if(fc == READCOILSTATUS){
    	printf("<Slave mode> respond Read Coil Status\n");
    }else{
		printf("<Slave mode> respond Read Input Status\n");
	}

    /* build RTU frame */
    build_rtu_frm(tx_buf, src, src_len);
    src_len += 2;       // add CRC 2 byte
    
    return src_len;
} 
/* 
 * FC 0x03 Read Holding Registers respond / FC 0x04 Read Input Registers respond
 */
int build_resp_read_regs(struct frm_para *sfpara, unsigned char *tx_buf, unsigned char fc)
{
    int i;
    int byte;
    int src_len;
	unsigned int slvID;
	unsigned int straddr;
	unsigned int num_regs;
    unsigned char src[FRMLEN];
	
	slvID = sfpara->slvID;
    straddr = sfpara->straddr;
	num_regs = sfpara->len;
	byte = num_regs * 2;			// num_regs * 2byte
    src_len = byte + 3;

    src[0] = slvID;                 // Slave ID
    src[1] = fc;       				// Function code
    src[2] = byte & 0xff;           // The number of data byte to follow
    for(i = 3; i < src_len; i++){   
//		src[i] = (straddr%229 + i) & 0xff;	// The Holding Regs addr, we set random value (0 ~ 240)
		src[i] = 0;
    }
	if(fc == READHOLDINGREGS){
    	printf("<Slave mode> respond Read Holding Registers \n");
	}else{
		printf("<Slave mode> respond Read Input Registers \n");
	}
    
    /* build RTU frame */
    build_rtu_frm(tx_buf, src, src_len);
    src_len += 2;       // add CRC 2 byte
    
    return src_len;
}
/*
 * FC 0x05 Force Single Coli respond / FC 0x06 Preset Single Register respond
 */
int build_resp_set_single(struct frm_para *sfpara, unsigned char *tx_buf, unsigned char fc)
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
    src[0] = slvID;                 // Slave ID
    src[1] = fc;					// Function code
    src[2] = straddr >> 8;			// data addr Hi
	src[3] = straddr;				// data addr Lo
	src[4] = act >> 8;				// active Hi
	src[5] = act;					// active Lo
	src_len = 6;
	
	if(fc == FORCESIGLEREGS){
    	printf("<Slave mode> respond Force Single Coli \n");
	}else{
		printf("<Slave mode> respond Preset Single Register \n");
	}
    
    /* build RTU frame */
    build_rtu_frm(tx_buf, src, src_len);
    src_len += 2;       // add CRC 2 byte
    
    return src_len;
}
