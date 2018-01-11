#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include "mbus.h"

int rtu_build_resp(void *buf, int buflen, const mbus_resp_info *mbusinfo)
{
	uint32_t txlen;
	uint32_t msglen;
	uint16_t *rtu_crc16;
	union _mbus_hdr *mbus = (union _mbus_hdr *)buf;

	if(buflen < __mbus_len(mbus, info, 0) + sizeof(uint16_t))
		return MBUS_ERROR_DATALEN;

	msglen = mbus_build_resp(mbus, buflen - sizeof(uint16_t), mbusinfo);
	txlen = msglen + sizeof(uint16_t);

	rtu_crc16 = __mbus_rtu_crc16(buf, msglen);

	if(txlen > buflen)
		return MBUS_ERROR_DATALEN;

	*rtu_crc16 = htobe16(crc_checksum(buf, msglen));
	
	return txlen;
}

int rtu_build_cmd(void *buf, int buflen, const mbus_cmd_info *mbusinfo)
{
	uint32_t txlen;
	uint32_t msglen;
	uint16_t *rtu_crc16;
	union _mbus_hdr *mbus = (union _mbus_hdr *)buf;

	if(buflen < __mbus_len(mbus, info, 0) + sizeof(uint16_t))
		return MBUS_ERROR_DATALEN;

	msglen = mbus_build_cmd(mbus, buflen - sizeof(uint16_t), mbusinfo);

	rtu_crc16 = __mbus_rtu_crc16(buf, msglen);
	txlen = msglen + sizeof(uint16_t);

	if(txlen > buflen)
		return MBUS_ERROR_DATALEN;

	*rtu_crc16 = htobe16(crc_checksum(buf, msglen));
	
	return txlen;
}

int rtu_build_excp(void *buf, int buflen, const mbus_cmd_info *mbusinfo, uint8_t excp_code)
{
	uint32_t txlen;
	uint32_t msglen;
	uint16_t *rtu_crc16;
	union _mbus_hdr *mbus = (union _mbus_hdr *)buf;

	if(buflen < __mbus_len(mbus, excp, 0) + sizeof(uint16_t))
		return MBUS_ERROR_DATALEN;

	msglen = mbus_build_excp(mbus, buflen - sizeof(uint16_t), mbusinfo, excp_code);

	txlen = msglen + sizeof(uint16_t);
	rtu_crc16 = __mbus_rtu_crc16(buf, msglen);

	if(txlen > buflen)
		return MBUS_ERROR_DATALEN;

	*rtu_crc16 = htobe16(crc_checksum(buf, msglen));
	
	return txlen;
}

int rtu_get_cmdinfo(void *buf, int buflen, mbus_cmd_info *mbusinfo)
{
	int ret;
	uint16_t *rtu_crc16;
	union _mbus_hdr *mbus = (union _mbus_hdr *)buf;

	if(buflen < __mbus_len(mbus, info, 0) + sizeof(uint16_t))
		return MBUS_ERROR_DATALEN;
	
	ret = mbus_get_cmdinfo(mbus, buflen - sizeof(uint16_t), mbusinfo);
	if(ret <= 0)
		return ret;

	rtu_crc16 = __mbus_rtu_crc16(buf, ret);

	if(*rtu_crc16 != htobe16(crc_checksum(buf, ret)))
		return MBUS_ERROR_FORMATE;

	return (ret + sizeof(uint16_t));
}

int rtu_get_respinfo(void *buf, int buflen, mbus_resp_info *mbusinfo)
{
	int ret;
	uint16_t *rtu_crc16;
	union _mbus_hdr *mbus = (union _mbus_hdr *)buf;

	if(buflen < __mbus_len(mbus, info, 0) + sizeof(uint16_t))
		return MBUS_ERROR_DATALEN;

	ret = mbus_get_respinfo(mbus, buflen - sizeof(uint16_t), mbusinfo);
	if(ret <= 0) return ret;

	rtu_crc16 = __mbus_rtu_crc16(buf, ret);

	if(*rtu_crc16 != htobe16(crc_checksum(buf, ret)))
		return MBUS_ERROR_FORMATE;

	return (ret + sizeof(uint16_t));
}

