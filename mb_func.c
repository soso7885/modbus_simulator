#include <stdio.h>
#include <stdlib.h>

#include "mbus.h"

struct frm_para fpara;

int paser_query(unsigned char *rx_buf, int len)
{
	fpara.slvID = *(rx_buf);      						// paser slave ID
	fpara.fc = *(rx_buf+1);         					// paser function code		
	fpara.straddr = *(rx_buf+2)<<8 | *(rx_buf+3);		// paser start addr
	if(!(fpara.fc ^ FORCESIGLEREGS)){
		fpara.act = *(rx_buf+4)<<8 | *(rx_buf+5);	
	}else if(!(fpara.fc ^ PRESETEXCPSTATUS)){
		fpara.val = *(rx_buf+4)<<8 | *(rx_buf+5);
	}else{
		fpara.len = *(rx_buf+4)<<8 | *(rx_buf+5);		// paser data len	
	}
	
	return 0;
}
/* 
 * FC 0x01 Read Coil Status
 */
int build_resp_Rcoil_status(unsigned char slvID, unsigned char *tx_buf, unsigned int straddr, int len)
{
    int i;
	int byte;
	int res;
    int src_num;
    int txlen;
    unsigned char src[FRMLEN];
    
	res = len % 8;
	byte = len / 8;	
	if(res > 0){
		byte += 1;
	}

    src_num = byte + 3;
    /* init data, fill in slave addr & function code ##Start Addr */
    src[0] = slvID;           		// Slave ID
	src[1] = READCOILSTATUS;		// Function code
	src[2] = byte & 0xff;       	// The number of data byte to follow
    for(i = 3; i < src_num; i++){	
        src[i] = (straddr%229 + i) & 0xff;	// The Coil Status addr status, we set random value (0 ~ 240) 
    }

    printf("respond Read Coil Status \n");
    
    /* build RTU frame */
    build_rtu_frm(tx_buf, src, src_num);
    txlen = src_num + 2;       // add CRC 2 byte
    
    return txlen;
}
/*
 * FC 0x02 Read Input Status
 */
int build_resp_Rinput_status(unsigned char slvID, unsigned char *tx_buf, unsigned int straddr, int len)
{
    int i;
    int byte;
    int res;
    int src_num;
    int txlen;
    unsigned char src[FRMLEN];
    
    res = len%8;
    byte = len/8;   
    if(res > 0){
        byte += 1;
    }

    src_num = byte + 3;
    /* init data, fill in slave addr & function code ##Start Addr */
    src[0] = slvID;                 // Slave ID
    src[1] = READINPUTSTATUS;		// Function code
    src[2] = byte & 0xff;           // The number of data byte to follow
    for(i = 3; i < src_num; i++){   
        src[i] = (straddr%229 + i) & 0xff;	// The Input Status addr, we set random value (0 ~ 240)
    }

    printf("respond Read Input Status \n");
    
    /* build RTU frame */
    build_rtu_frm(tx_buf, src, src_num);
    txlen = src_num + 2;       // add CRC 2 byte
    
    return txlen;
}
/* 
 * FC 0x03 Read Holding Registers
 */
int build_resp_Rholding_regs(unsigned char slvID, unsigned char *tx_buf, unsigned int straddr, int num_regs)
{
    int i;
    int byte;
    int src_num;
    int txlen;
    unsigned char src[FRMLEN];
    
	byte = num_regs * 2;			// num_regs * 2byte
    src_num = byte + 3;
    /* init data, fill in slave addr & function code ##Start Addr */
    src[0] = slvID;                 // Slave ID
    src[1] = READHOLDINGREGS;       // Function code
    src[2] = byte & 0xff;           // The number of data byte to follow
    for(i = 3; i < src_num; i++){   
        src[i] = (straddr%229 + i) & 0xff;	// The Holding Regs addr, we set random value (0 ~ 240)
    }

    printf("respond Read Holding Registers \n");
    
    /* build RTU frame */
    build_rtu_frm(tx_buf, src, src_num);
    txlen = src_num + 2;       // add CRC 2 byte
    
    return txlen;
}
/* 
 * FC 0x04 Read Input Registers
 */
int build_resp_Rinput_regs(unsigned char slvID, unsigned char *tx_buf, unsigned int straddr, int num_regs)
{
    int i;
    int byte;
    int src_num;
    int txlen;
	unsigned int tmp;
    unsigned char src[FRMLEN];
    
    byte = num_regs * 2;            // num_regs * 2byte
    src_num = byte + 3;
    /* init data, fill in slave addr & function code ##Start Addr */
    src[0] = slvID;                 // Slave ID
    src[1] = READINPUTREGS;			// Function code
    src[2] = byte & 0xff;           // The number of data byte to follow
    for(i = 3; i < src_num; i+=2){  
		tmp = straddr * 77 + i;			// set random regs value
        src[i] = tmp >> 8;
		src[i+1] = tmp & 0xff;
    }

    printf("respond Read Input Registers \n");
    
    /* build RTU frame */
    build_rtu_frm(tx_buf, src, src_num);
    txlen = src_num + 2;       // add CRC 2 byte
    
    return txlen;
}
/*
 * FC 0x05 Force Single Coli
 */
int build_resp_force_single_coli(unsigned char slvID, unsigned char *tx_buf, unsigned int straddr, unsigned int act)
{
    int src_num;
    int txlen;
    unsigned char src[FRMLEN];
    
    /* init data, fill in slave addr & function code ##Start Addr */
    src[0] = slvID;                 // Slave ID
    src[1] = FORCESIGLEREGS;		// Function code
    src[2] = straddr >> 8;			// data addr Hi
	src[3] = straddr;				// data addr Lo
	src[4] = act >> 8;				// active Hi
	src[5] = act;					// active Lo
	src_num = 6;

    printf("respond Force Single Coli \n");
    
    /* build RTU frame */
    build_rtu_frm(tx_buf, src, src_num);
    txlen = src_num + 2;       // add CRC 2 byte
    
    return txlen;
}
/*
 * FC 0x06 Preset Single Register
 */
int build_resp_preset_single_regs(unsigned char slvID, unsigned char *tx_buf, unsigned int straddr, unsigned int val)
{
    int src_num;
    int txlen;
    unsigned char src[FRMLEN];
    
    /* init data, fill in slave addr & function code ##Start Addr */
    src[0] = slvID;                 // Slave ID
    src[1] = PRESETEXCPSTATUS;      // Function code
    src[2] = straddr >> 8;          // data addr Hi
    src[3] = straddr;               // data addr Lo
    src[4] = val >> 8;              // the value of write Hi
    src[5] = val;                   // the value of write Lo
    src_num = 6;

    printf("respond Preset Single Register \n");
    
    /* build RTU frame */
    build_rtu_frm(tx_buf, src, src_num);
    txlen = src_num + 2;       // add CRC 2 byte
    
    return txlen;
}






