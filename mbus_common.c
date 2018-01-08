#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <endian.h>

#include "mbus.h"

int mbus_build_excp(union _mbus_hdr * mbus, int buflen, const mbus_cmd_info *mbusinfo, unsigned char excp_code)
{
	mbus->info.unitID = mbusinfo->info.unitID;
	mbus->info.fc = mbusinfo->info.fc | EXCPTIONCODE;
	mbus->excp.ec = excp_code;

	return __mbus_len(mbus, excp, 0);
}

int mbus_build_cmd(union _mbus_hdr *mbus, int buf_len,  const mbus_cmd_info * cmd_info)
{
	int txlen = 0;

	if(buf_len < __mbus_len(mbus, info, 0)){
//		printf("%s buf len is too short for info\n", __func__);
		return MBUS_ERROR_DATALEN;
	}

	mbus->info.unitID = cmd_info->info.unitID;
	mbus->info.fc = cmd_info->info.fc;
	
	switch(cmd_info->info.fc){
		case PRESETSINGLEREG:
		case FORCESINGLECOIL:
			txlen = __mbus_len(mbus, swrite, 0);
			if(buf_len < txlen){
/*				printf("%s buflen is too short buflen = %d reqires %d\n",
						__func__,
						buf_len, 
						__mbus_len(mbus, swrite, 0));*/
				return MBUS_ERROR_DATALEN;
			}
			mbus->swrite.addr = htons(cmd_info->swrite.addr);
			mbus->swrite.data = htons(cmd_info->swrite.data);
			break;
	
		case READHOLDINGREGS:
		case READCOILSTATUS:
		case READINPUTSTATUS:
			txlen = __mbus_len(mbus, query, 0);
			if(buf_len < txlen){
/*				printf("%s framelen is too short buflen = %d reqires %d\n", 
						__func__,
						buf_len, 
						__mbus_len(mbus, query, 0));*/
				return MBUS_ERROR_DATALEN;
			}
			mbus->query.addr = htons(cmd_info->query.addr);
			mbus->query.len = htons(cmd_info->query.len);
			break;
		case FORCEMULTICOILS:
		case PRESETMULTIREGS:
			txlen = __mbus_len(mbus, mwrite, cmd_info->mwrite.bcnt);
			if(buf_len < txlen){
/*				printf("%s framelen is too short buflen = %d reqires %d\n", 
						__func__,
						buf_len, 
						__mbus_len(mbus, query, 0));*/
				return MBUS_ERROR_DATALEN;
			}
			mbus->mwrite.addr = htons(cmd_info->mwrite.addr);
			mbus->mwrite.len = htons(cmd_info->mwrite.len);
			mbus->mwrite.bcnt = cmd_info->mwrite.bcnt;
			if(cmd_info->mwrite.data > 0){
				memcpy(mbus->mwrite.data, cmd_info->mwrite.data, cmd_info->mwrite.bcnt);
			}else{
				memset(mbus->mwrite.data, 0, cmd_info->mwrite.bcnt);
			}
			break;
		default:
//			printf("don't support this fc %x\n", cmd_info->info.fc);
			return MBUS_ERROR_FORMATE;
			break;
	}

	return txlen;
}


int mbus_get_cmdinfo(const union _mbus_hdr *mbus_frame, int len, mbus_cmd_info *mbus_result)
{
	int msglen;
	
	mbus_result->info.fc = mbus_frame->info.fc;
	mbus_result->info.unitID = mbus_frame->info.unitID;

	switch(mbus_frame->info.fc){
		case READCOILSTATUS:
		case READINPUTSTATUS:
			msglen = __mbus_len(mbus_frame, query, 0);
			if(len < msglen){
//				printf("%s query coil len is too short%d:%d\n", __func__, len, msglen);
				return MBUS_ERROR_DATALEN;
			}
			mbus_result->query.addr = ntohs(mbus_frame->query.addr);
			mbus_result->query.len = ntohs(mbus_frame->query.len);
			break;
		case READHOLDINGREGS:
		case READINPUTREGS:
			msglen = __mbus_len(mbus_frame, query, 0);
			if(len < msglen){
//				printf("%s query reg len is too short%d:%d\n", __func__, len, msglen);
				return MBUS_ERROR_DATALEN;
			}
			mbus_result->query.addr = ntohs(mbus_frame->query.addr);
			mbus_result->query.len = ntohs(mbus_frame->query.len);
			break;
		case PRESETSINGLEREG:
		case FORCESINGLECOIL:
			msglen = __mbus_len(mbus_frame, swrite, 0);
			if(len < msglen){
//				printf("%s single write len is too short %d:%d\n", __func__, len, msglen);
				return MBUS_ERROR_DATALEN;
			}
			mbus_result->swrite.addr = ntohs(mbus_frame->swrite.addr);
			mbus_result->swrite.data = ntohs(mbus_frame->swrite.data);
			break;
		case PRESETMULTIREGS:
		case FORCEMULTICOILS:
			msglen = __mbus_len(mbus_frame, mwrite, 0);
			if(len < msglen){
//				printf("%s single write len is too short %d:%d\n", __func__, len, msglen);
				return MBUS_ERROR_DATALEN;
			}
			mbus_result->mwrite.addr = ntohs(mbus_frame->mwrite.addr);
			mbus_result->mwrite.len = ntohs(mbus_frame->mwrite.len);
			mbus_result->mwrite.bcnt = mbus_frame->mwrite.bcnt;
			mbus_result->mwrite.data = (uint8_t *)mbus_frame->mwrite.data;
			break;
		default:
//			printf("%s unknown function code\n", __func__);
			return MBUS_ERROR_FORMATE;
	}

	return msglen;
}

int mbus_build_resp(union _mbus_hdr * mbus, int buflen, const mbus_resp_info * mbusinfo)
{
	unsigned char bytelen = 0;
	unsigned short msglen = 0;

	mbus->info.unitID = mbusinfo->info.unitID;
	mbus->info.fc = mbusinfo->info.fc;

	switch(mbusinfo->info.fc){
		case READCOILSTATUS:
		case READINPUTSTATUS:
		case READHOLDINGREGS:
		case READINPUTREGS:
			bytelen = mbusinfo->query.bcnt;
			msglen = __mbus_len(mbus, r_query, bytelen);
			if(msglen > buflen){
//				printf("%s bufsize %d is too small\n", __func__, buflen);	
				return MBUS_ERROR_DATALEN;
			}
			mbus->r_query.bcnt = (unsigned char)bytelen;
			if(mbusinfo->query.data > 0){
				memcpy(mbus->r_query.data, mbusinfo->query.data, bytelen);
			}else{
				memset(mbus->r_query.data, 0, bytelen);
			}
			break;
		case FORCESINGLECOIL:
		case PRESETSINGLEREG:
			msglen = __mbus_len(mbus, swrite, 0);
			if(msglen > buflen){
//				printf("%s bufsize %d is too small\n", __func__, buflen);	
				return MBUS_ERROR_DATALEN;
			}
			mbus->swrite.addr = htons(mbusinfo->swrite.addr);
			mbus->swrite.data = htons(mbusinfo->swrite.data);
			break;
		case FORCEMULTICOILS:
		case PRESETMULTIREGS:
			msglen = __mbus_len(mbus, r_mwrite, 0);
			if(msglen > buflen){
//				printf("%s bufsize %d is too small\n", __func__, buflen);	
				return MBUS_ERROR_DATALEN;
			}
			mbus->r_mwrite.addr = htons(mbusinfo->mwrite.addr);
			mbus->r_mwrite.len = htons(mbusinfo->mwrite.len);
			break;
		default:
//			printf("%s unknown function code\n", __func__);
			return MBUS_ERROR_FORMATE;
	}

	return msglen;
}


int mbus_get_respinfo(const union _mbus_hdr *mbus_frame, int len, mbus_resp_info *mbus_result)
{
	int msglen;
	uint8_t bytecnt;

	mbus_result->info.fc = mbus_frame->info.fc;
	mbus_result->info.unitID = mbus_frame->info.unitID;

	if(mbus_frame->info.fc & EXCPTIONCODE){
		msglen = __mbus_len(mbus_frame, excp, 0);

		if(msglen > len){
//			printf("%s excp len is not enough\n", __func__);
			return MBUS_ERROR_DATALEN;
		}

		mbus_result->excp.ec = mbus_frame->excp.ec;
		return msglen;
	}

	switch(mbus_frame->info.fc){
		case READCOILSTATUS:
		case READINPUTSTATUS:
		case READHOLDINGREGS:
		case READINPUTREGS:
			msglen = __mbus_len(mbus_frame, r_query, 0);
			if(msglen > len){
//				printf("%s buffer len is too short 1\n", __func__);
				return MBUS_ERROR_DATALEN;
			}
			bytecnt = mbus_frame->r_query.bcnt;
			msglen = __mbus_len(mbus_frame, r_query, bytecnt);
			if(len < msglen){
//				printf("%s query len is too short%d:%d\n", __func__, len, msglen);
				return MBUS_ERROR_DATALEN;
			}

			mbus_result->query.bcnt = bytecnt;
			mbus_result->query.data = (uint8_t *)mbus_frame->r_query.data;
			break;
		case PRESETSINGLEREG:
		case FORCESINGLECOIL:
			msglen = __mbus_len(mbus_frame, swrite, 0);
			if(len < msglen){
//				printf("%s single write len is too short %d:%d\n", __func__, len, msglen);
				return MBUS_ERROR_DATALEN;
			}
			mbus_result->swrite.addr = ntohs(mbus_frame->swrite.addr);
			mbus_result->swrite.data = ntohs(mbus_frame->swrite.data);
			break;
		case PRESETMULTIREGS:
		case FORCEMULTICOILS:
			msglen = __mbus_len(mbus_frame, r_mwrite, 0);
			if(len < msglen){
//				printf("%s single write len is too short %d:%d\n", __func__, len, msglen);
				return MBUS_ERROR_DATALEN;
			}
			mbus_result->mwrite.addr = ntohs(mbus_frame->r_mwrite.addr);
			mbus_result->mwrite.len = ntohs(mbus_frame->r_mwrite.len);
			break;

		default:
//			printf("%s unknown function code\n", __func__);
			return MBUS_ERROR_FORMATE;
	}

	return msglen;
}

int mbus_cmd_resp(const mbus_cmd_info *cmd, mbus_resp_info *resp, const void *data)
{
	uint8_t bytelen = 0;

	resp->info.fc = cmd->info.fc;
	resp->info.unitID = cmd->info.unitID;

	switch(cmd->info.fc){
		case READCOILSTATUS:
		case READINPUTSTATUS:
			bytelen = carry((int)cmd->query.len, 8/*bits per char*/);
			break;
		case READHOLDINGREGS:
		case READINPUTREGS:
			bytelen = cmd->query.len * sizeof(uint16_t);
			break;
		case FORCESINGLECOIL:
		case PRESETSINGLEREG:
			resp->swrite.addr = htons(cmd->swrite.addr);
			resp->swrite.data = htons(cmd->swrite.data);
			break;
		case FORCEMULTICOILS:
		case PRESETMULTIREGS:
			resp->mwrite.addr = htons(cmd->mwrite.addr);
			resp->mwrite.len = htons(cmd->mwrite.len);
			break;
		default:
//			printf("%s unknown function code\n", __func__);
			return MBUS_ERROR_FORMATE;
	}

	if(bytelen > 0){
		resp->query.bcnt = (uint8_t)bytelen;
		if(data > 0){
			resp->query.data = (uint8_t *)data;
		}else{
			resp->query.data = 0;
		}
	}

	return (int)bytelen;
}
