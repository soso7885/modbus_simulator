#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <endian.h>
#include <pthread.h>

#include "mbus.h"
/*
 * Analyze modebus TCP query
 */
int tcp_get_respinfos(const void *buf, int buflen, mbus_resp_info *mbus_result, mbus_tcp_info *tcp_result)
{
	int ret;
	uint16_t msglen;
	uint16_t tid;
	uint16_t pid;
	const struct mbus_tcp_frm *hdr = buf;
	const struct _tcp_hdr *tcp = &hdr->tcp;
	const union _mbus_hdr *mbus = &hdr->mbus;
	
	if(buflen < __mbus_tcp_framelen(hdr, info, 0)){
//		printf("%s data length is too short for getting info\n", __func__);
		return MBUS_ERROR_DATALEN;
	}

	msglen = ntohs(tcp->msglen);
	tid = ntohs(tcp->transID);
	pid = ntohs(tcp->protoID);

	ret = mbus_get_respinfo(mbus, buflen-sizeof(struct _tcp_hdr), mbus_result);
	if(ret <= 0){
//		printf("%s failed to get mbus info\n", __func__);
		return ret;
	}

	tcp_result->msglen = msglen;
	tcp_result->transID = tid;
	tcp_result->protoID = pid;

	return (sizeof(struct _tcp_hdr) + ret);
}

int tcp_get_cmdinfos(const void *buf, int buflen, mbus_cmd_info *mbus_result, mbus_tcp_info *tcp_result)
{
	int ret;
	uint16_t msglen;
	uint16_t tid;
	uint16_t pid;
	const struct mbus_tcp_frm *hdr = buf;
	const struct _tcp_hdr *tcp = &hdr->tcp;
	const union _mbus_hdr *mbus = &hdr->mbus;
	
	if(buflen < __mbus_tcp_framelen(hdr, info, 0)){
//		printf("%s data length is too short for getting info\n", __func__);
		return MBUS_ERROR_DATALEN;
	}

	msglen = ntohs(tcp->msglen);
	tid = ntohs(tcp->transID);
	pid = ntohs(tcp->protoID);

	ret = mbus_get_cmdinfo(mbus, buflen-sizeof(struct _tcp_hdr), mbus_result);
	if(ret <= 0){
//		printf("%s failed to get mbus info\n", __func__);
		return ret;
	}

	tcp_result->msglen = msglen;
	tcp_result->transID = tid;
	tcp_result->protoID = pid;

	return (sizeof(struct _tcp_hdr) + ret);
}

/*
 * build Modbus TCP query
 */			
int tcp_build_cmd(void *buf, int buf_len, const mbus_cmd_info *cmd_info, const mbus_tcp_info *tcp_info) 
{
	int ret;
	struct mbus_tcp_frm * hdr = (struct mbus_tcp_frm *)buf;
	struct _tcp_hdr * tcp = &hdr->tcp;
	union _mbus_hdr * mbus = &hdr->mbus;

	if(buf_len < __mbus_tcp_framelen(hdr, info, 0)){
		printf("framelen is too short buflen = %d reqires %ld\n", 
				buf_len, __mbus_tcp_framelen(hdr, info, 0));
		return 0;
	}

	ret = mbus_build_cmd(mbus, buf_len - sizeof(struct _tcp_hdr), cmd_info);
	if(ret < 0)
		printf("Failed to build mbus command\n");

	tcp->transID = htons(tcp_info->transID);
	tcp->protoID = htons(tcp_info->protoID);
	tcp->msglen = htons(ret);

	return (ret + sizeof(struct _tcp_hdr));
}

/*
 * build modbus TCP respond exception
 */
int tcp_build_excp(void *buf, int buflen, const mbus_cmd_info *mbusinfo, 
				const mbus_tcp_info *tcpinfo, uint8_t excp_code)
{
	int txlen;
	uint16_t msglen;
	struct mbus_tcp_frm *hdr = (struct mbus_tcp_frm *)buf;
	struct _tcp_hdr *tcp = &hdr->tcp;
	union _mbus_hdr *mbus = &hdr->mbus;

	txlen = __mbus_tcp_framelen(hdr, excp, 0);
	msglen = __mbus_len(mbus, excp, 0);
		
	if(buflen < txlen){
//		printf("%s buflen = %d need %d\n", __func__, buflen, txlen);
		return MBUS_ERROR_DATALEN;
	}

	tcp->transID = htons(tcpinfo->transID);
	tcp->protoID = htons(tcpinfo->protoID);
	tcp->msglen = htons(msglen);

	return txlen;
}

int tcp_build_resp(void *buf, int buflen, const mbus_resp_info *mbusinfo, 
					const mbus_tcp_info *tcpinfo/*, unsigned char * data*/)
{
	int txlen;
	uint16_t msglen = 0;
	struct mbus_tcp_frm *hdr = (struct mbus_tcp_frm *)buf;
	struct _tcp_hdr *tcp = &hdr->tcp;
	union _mbus_hdr *mbus = &hdr->mbus;

	if(buflen < __mbus_tcp_framelen(hdr, info, 0)){
//		printf("%s bufsize %d is too small\n", __func__, buflen);
		return MBUS_ERROR_DATALEN;
	}

	msglen = mbus_build_resp(mbus, buflen-__mbus_tcp_hdrlen, mbusinfo);
	txlen = msglen + __mbus_tcp_hdrlen;

	if(txlen > buflen){
//		printf("%s buflen = %d need msglen %d", __func__, buflen, txlen);
		return MBUS_ERROR_DATALEN;
	}

	tcp->transID = htons(tcpinfo->transID);
	tcp->protoID = htons(tcpinfo->protoID);
	tcp->msglen = htons(msglen);

	return txlen;
}

