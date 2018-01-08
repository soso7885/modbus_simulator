#ifndef MBUS_H
#define MBUS_H
#include <stdint.h>
#include <assert.h>
#ifdef __APPLE__
	#include <machine/endian.h>
#else
	#include <endian.h>
#endif

#define INITTCPTRANSID			1<<8
#define EXCPMSGTOTAL			6

#define TCPMBUSPROTOCOL			0
#define TCPQUERYMSGLEN			6
#define TCPRESPSETSIGNALLEN		6
#define TCPRESPEXCPFRMLEN		9
#define TCPRESPEXCPMSGLEN		3
#define TCPSENDQUERYLEN			12
#define SERRECVQRYLEN 			8

#define READCOILSTATUS 			1
#define READINPUTSTATUS 		2
#define READHOLDINGREGS 		3
#define READINPUTREGS 			4
#define FORCESINGLECOIL 		5
#define PRESETSINGLEREG 		6
#define FORCEMULTICOILS			15
#define PRESETMULTIREGS			16

#define EXCPTIONCODE			0x80
#define EXCPILLGFUNC			1
#define EXCPILLGDATAADDR		2
#define EXCPILLGDATAVAL			3
#define READCOILSTATUS_EXCP		129
#define READINPUTSTATUS_EXCP	130
#define READHOLDINGREGS_EXCP	131
#define READINPUTREGS_EXCP		132
#define FORCESIGLEREGS_EXCP		133
#define PRESETEXCPSTATUS_EXCP	134

#define MBUS_ERROR_DATALEN	(-1)
#define MBUS_ERROR_FORMATE	(-2)

//#define DEBUGMSG
//#define POLL_SLVID
#define BAUDRATE 9600

#define FRMLEN 260  /* | 1byte | 1byte | 0~255byte | 2byte | */

#define handle_error_en(en, msg) \
				do{ errno = en; perror(msg); exit(EXIT_FAILURE); }while (0)

#ifdef POLL_SLVID
#define poll_slvID(slvID) \
				do{ ((slvID) == 32) ? (slvID)=1:(slvID)++; \
					printf("Slave ID = %d\n", slvID); }while(0)
#else
#define poll_slvID(slvID) \
				do{}while(0)
#endif
#ifdef DEBUGMSG
#define debug_print_data(buf, len, status) \
				do{ print_data(buf, len, status);}while(0)
#else
#define debug_print_data(buf, len, status) \
				do{}while(0)
#endif

static inline int carry(int bdiv, int div)
{
	int tmp;
	int ans;
	
	ans = bdiv / div;
	tmp = bdiv % div;
	if(tmp > 0)
		ans += 1;

	return ans;
}

static inline void print_data(uint8_t *buf, int len, int status)
{
	char *s[8] = {"Recv Respond :", 
				 "Recv Query :",
				 "Recv Excption :",
				 "Recv Incomplete :",
				 "Send Respond :",
				 "Send Query :",
				 "Send Excption :",
				 "Send Incomplete :"
				};
	
	printf("%s\n", s[status]);
	for(int i = 0; i < len; i++)
		printf(" %x |", buf[i]);
	printf("## len = %d ##\n", len);
}

struct _mbus_gen_hdr{
	uint8_t unitID;
	uint8_t fc;
}__attribute__((packed));

struct _mbus_multi{
	struct _mbus_gen_hdr info;
	uint16_t addr;
	uint16_t len;
	uint8_t bcnt;
	uint8_t data[1];
}__attribute__((packed));

struct _mbus_r_multi{
	struct _mbus_gen_hdr info;
	uint8_t bcnt;
	uint8_t data[1];
}__attribute__((packed));

struct _mbus_single{
	struct _mbus_gen_hdr info;
	uint16_t addr;
	uint16_t data;
}__attribute__((packed));

struct _mbus_query{
	struct _mbus_gen_hdr info;
	uint16_t addr;
	uint16_t len;
}__attribute__((packed));

struct _mbus_excp{
	struct _mbus_gen_hdr info;
	uint8_t ec;
}__attribute__((packed));

union _mbus_hdr{
	struct _mbus_gen_hdr info;

	/*common data types*/
	struct _mbus_multi mwrite;
	struct _mbus_query query;
	
	/*used for build_cmd/req and get_cmd/reqinfo*/
	struct _mbus_single force_scoil;
	struct _mbus_single preset_sreg;
	struct _mbus_single swrite;

	/*used for build_resp and get_respinfo*/
	struct _mbus_r_multi r_readregs;
	struct _mbus_r_multi r_readcoils;
	struct _mbus_r_multi r_query;
	struct _mbus_query r_mwrite;

	struct _mbus_excp excp;
}__attribute__((packed));

struct _tcp_hdr{
	uint16_t transID;
	uint16_t protoID;
	uint16_t msglen;
}__attribute__((packed));

struct mbus_tcp_frm{
	struct _tcp_hdr tcp;
	union _mbus_hdr mbus;
}__attribute__((packed));

#define __mbus_len(PTR, TYPENAME, BYTECNT)				(sizeof((PTR)->TYPENAME)+BYTECNT-(BYTECNT>0 ? sizeof(uint8_t) : 0))
#define __mbus_tcp_framelen(PTR, TYPENAME, BYTECNT)		(sizeof((PTR)->tcp) + __mbus_len(&((PTR)->mbus), TYPENAME, BYTECNT))
#define __mbus_tcp_hdrlen								(sizeof(struct _tcp_hdr))
#define __mbus_rtu_crc16(PTR, LEN)						((uint16_t *)(((uint8_t *)PTR) + LEN))

struct _mbus_multi_cmdinfo{
	struct _mbus_gen_hdr info;
	uint16_t addr;
	uint16_t len;
	uint8_t bcnt;
	uint8_t *data;
}__attribute__((packed));

typedef union {
	struct _mbus_gen_hdr info;
	struct _mbus_single swrite;
	struct _mbus_query query;
	struct _mbus_multi_cmdinfo mwrite;
} mbus_cmd_info;

struct _mbus_multi_respinfo{
	struct _mbus_gen_hdr info;
	uint8_t bcnt;
	uint8_t *data;
};

typedef union{
	struct _mbus_gen_hdr info;
	struct _mbus_single swrite;
	struct _mbus_query mwrite;
	struct _mbus_multi_respinfo query;
	struct _mbus_excp excp;
} mbus_resp_info;

typedef struct _tcp_hdr mbus_tcp_info;

int tcp_get_cmdinfos(const void *buf, int buflen, mbus_cmd_info *mbus_result, mbus_tcp_info *tcp_result);
int tcp_get_respinfos(const void *buf, int buflen, mbus_resp_info *mbus_result, mbus_tcp_info *tcp_result);
int tcp_build_cmd(void *buf, int buflen, const mbus_cmd_info *mbusinfo, const mbus_tcp_info *tcpinfo);
int tcp_build_excp(void *buf, int buflen, const mbus_cmd_info *mbusinfo, const mbus_tcp_info *tcpinfo, uint8_t excp_code);
int tcp_build_resp(void *buf, int buflen, const mbus_resp_info *mbusinfo, const mbus_tcp_info *tcpinfo);

int mbus_cmd_resp(const mbus_cmd_info *cmd, mbus_resp_info *resp, const void *data);
int mbus_build_cmd(union _mbus_hdr *mbus, int buf_len, const mbus_cmd_info *cmd_info);
int mbus_build_resp(union _mbus_hdr *mbus, int buflen, const mbus_resp_info *mbusinfo);
int mbus_build_excp(union _mbus_hdr *mbus, int buflen, const mbus_cmd_info *mbusinfo, uint8_t excp_code);
int mbus_get_cmdinfo(const union _mbus_hdr *mbus_frame, int len, mbus_cmd_info *mbus_result);
int mbus_get_respinfo(const union _mbus_hdr *mbus_frame, int len, mbus_resp_info *mbus_result);

uint16_t crc_checksum(const void *buff, int len);

int rtu_build_excp(void *buf, int buflen, const mbus_cmd_info *mbusinfo, uint8_t excp_code);
int rtu_build_resp(void *buf, int buflen, const mbus_resp_info *mbusinfo);
int rtu_build_cmd(void *buf, int buflen, const mbus_cmd_info *mbusinfo);
int rtu_get_respinfo(void *buf, int buflen, mbus_resp_info *mbusinfo);
int rtu_get_cmdinfo(void * buf, int buflen, mbus_cmd_info * mbusinfo);
#endif


