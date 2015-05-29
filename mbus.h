#ifndef MBUS_H
#define MBUS_H

#define READCOILSTATUS 		0x01
#define READINPUTSTATUS 	0x02
#define READHOLDINGREGS 	0x03
#define READINPUTREGS 		0x04
#define FORCESIGLEREGS 		0x05
#define PRESETEXCPSTATUS 	0x06
#define FORCEMUILTCOILS		0x15
#define PRESETMUILTREGS		0x16

#define FRMLEN 260  /* | 1byte | 1byte | 0~255byte | 2byte | */

struct frm_para {
	unsigned int slvID;
	int len;
	unsigned char fc;
	unsigned int straddr;
	unsigned int act;			// The status to write (in FC 0x05)
	unsigned int val;			// The value of write (in FC 0x06) 
};

void build_rtu_frm(unsigned char *dst_buf, unsigned char *src_buf, unsigned char lenth);
int paser_query(unsigned char *rx_buf, int len);

int build_resp_Rcoil_status(unsigned char slvID, unsigned char *tx_buf, unsigned int straddr, int len);
int build_resp_Rinput_status(unsigned char slvID, unsigned char *tx_buf, unsigned int straddr, int len);
int build_resp_Rholding_regs(unsigned char slvID, unsigned char *tx_buf, unsigned int straddr, int num_regs);
int build_resp_Rinput_regs(unsigned char slvID, unsigned char *tx_buf, unsigned int straddr, int num_regs);
int build_resp_force_single_coli(unsigned char slvID, unsigned char *tx_buf, unsigned int straddr, unsigned int act);
int build_resp_preset_single_regs(unsigned char slvID, unsigned char *tx_buf, unsigned int straddr, unsigned int val);

#endif


