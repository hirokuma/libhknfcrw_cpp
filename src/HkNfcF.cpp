#include "HkNfcF.h"
#include "NfcPcd.h"
#include "misc.h"

#define LOG_TAG "HkNfcF"
#include "nfclog.h"

#include <cstring>

using namespace HkNfcRwMisc;

namespace {
	const uint16_t kSC_BROADCAST = 0xffff;		///<

	const uint16_t kDEFAULT_TIMEOUT = 1000 * 2;
	const uint16_t kPUSH_TIMEOUT = 2100 * 2;

#ifdef HKNFCRW_USE_FALP
	//FALP command
	const uint8_t kFCMD_HELLO = 0x00;
	const uint8_t kFCMD_YEAH = 0x01;
	const uint8_t kFCMD_CSND = 0x02;
	const uint8_t kFCMD_SEND = 0x03;
	const uint8_t kFCMD_ADIOS = 0x05;
	const uint8_t kFCMD_BYE = 0x07;

	//OBEX command
	const uint8_t kOBEX_SUCCESS = 0xa0;
#endif	//HKNFCRW_USE_FALP
}

namespace {

	enum AccessModeType {
		AM_NORMAL = 0x00,
	};
	enum SCOrderType {
		SCO_NORMAL = 0x00,
	};
	inline uint16_t create_blocklist2(uint16_t blockNo, AccessModeType am=AM_NORMAL, SCOrderType sco=SCO_NORMAL)
	{
		return uint16_t(0x8000U | (am << 12) | (sco << 8) | blockNo);
	}
}


HkNfcF::HkNfcF(HkNfcRw* pRw)
	: m_pHkNfcRw(pRw), m_pNfcPcd(pRw->m_pNfcPcd),
		m_SystemCode(kSC_BROADCAST),
		m_SvcCode(SVCCODE_RW)
#ifdef HKNFCRW_USE_FALP
		, m_FalpSId(0x0000), m_FalpRId(0x0000)
#endif	//HKNFCRW_USE_FALP
{
}


HkNfcF::~HkNfcF()
{
}


/**
 * pollingしたカードの解放
 */
void HkNfcF::release()
{
	m_SystemCode = kSC_BROADCAST;
}


/**
 * Polling
 *
 * @param[in]		systemCode		システムコード
 * @retval			true			取得成功
 * @retval			false			取得失敗
 *
 * @retval		true			成功
 * @retval		false			失敗
 *
 * @attention	- 取得失敗は、主にカードが認識できない場合である。
 */
bool HkNfcF::polling(uint16_t systemCode /* = 0xffff */)
{
	//InListPassiveTarget
	const uint8_t INLISTPASSIVETARGET[] = {
		0x02,				// 0x01:212Kbps  0x02:424Kbps
		0x00,
		0xff, 0xff,			// SystemCode
		0x01,				// opt:0x01...get SystemCode
		0x0f				// Time Slot
	};
	const uint8_t INLISTPASSIVETARGET_RES[] = {
		0x01,				// System Number
		0x14,				// length(opt:+2)
		0x01				// response code
	};

	bool ret;
	uint8_t responseLen;
	uint8_t* pData;

	// 424Kbps
	memcpy(m_pHkNfcRw->s_CommandBuf, INLISTPASSIVETARGET, sizeof(INLISTPASSIVETARGET));
	m_pHkNfcRw->s_CommandBuf[2] = h16(systemCode);
	m_pHkNfcRw->s_CommandBuf[3] = l16(systemCode);

	ret = m_pNfcPcd->inListPassiveTarget(
				m_pHkNfcRw->s_CommandBuf, sizeof(INLISTPASSIVETARGET),
				&pData, &responseLen);
	if (!ret
	  || (memcmp(&pData[3], INLISTPASSIVETARGET_RES, sizeof(INLISTPASSIVETARGET_RES)) != 0)) {
		LOGE("pollingF fail(424Kbps): ret=%d/len=%d", ret, responseLen);

		//212Kbps
		m_pHkNfcRw->s_CommandBuf[0] = 0x01;
		ret = m_pNfcPcd->inListPassiveTarget(
				m_pHkNfcRw->s_CommandBuf, sizeof(INLISTPASSIVETARGET),
				&pData, &responseLen);
		if (!ret
		  || (memcmp(&pData[3], INLISTPASSIVETARGET_RES, sizeof(INLISTPASSIVETARGET_RES)) != 0)) {
			LOGE("pollingF fail(212Kbps): ret=%d/len=%d", ret, responseLen);
			m_SystemCode = kSC_BROADCAST;
			return false;
		}
	}

	memcpy(m_pHkNfcRw->m_NfcId, pData + 6, NFCID_LEN);
	m_pHkNfcRw->m_NfcIdLen = NFCID_LEN;
	m_SystemCode = (uint16_t)(*(pData + 22) << 8 | *(pData + 23));
	//LOGD("SystemCode : %04x", m_SystemCode);

	return true;
}


/**
 *
 */
bool HkNfcF::read(uint8_t* buf, uint8_t blockNo/*=0x00*/)
{
	uint8_t len;
	m_pHkNfcRw->s_CommandBuf[0] = 16;
	m_pHkNfcRw->s_CommandBuf[1] = 0x06;
	memcpy(m_pHkNfcRw->s_CommandBuf + 2, m_pHkNfcRw->m_NfcId, NFCID_LEN);
	m_pHkNfcRw->s_CommandBuf[10] = 0x01;				//サービス数
	m_pHkNfcRw->s_CommandBuf[11] = l16(m_SvcCode);
	m_pHkNfcRw->s_CommandBuf[12] = h16(m_SvcCode);
	m_pHkNfcRw->s_CommandBuf[13] = 0x01;			//ブロック数
	uint16_t blist = create_blocklist2(blockNo);
	m_pHkNfcRw->s_CommandBuf[14] = h16(blist);
	m_pHkNfcRw->s_CommandBuf[15] = l16(blist);
	bool ret = m_pNfcPcd->communicateThruEx(
					kDEFAULT_TIMEOUT,
					m_pHkNfcRw->s_CommandBuf, 16,
					m_pHkNfcRw->s_ResponseBuf, &len);
	if (!ret || (m_pHkNfcRw->s_ResponseBuf[0] != m_pHkNfcRw->s_CommandBuf[0]+1)
	  || (memcmp(m_pHkNfcRw->s_ResponseBuf + 1, m_pHkNfcRw->m_NfcId, NFCID_LEN) != 0)
	  || (m_pHkNfcRw->s_ResponseBuf[9] != 0x00)
	  || (m_pHkNfcRw->s_ResponseBuf[10] != 0x00)) {
		LOGE("read : ret=%d / %02x / %02x / %02x", ret, m_pHkNfcRw->s_ResponseBuf[0], m_pHkNfcRw->s_ResponseBuf[9], m_pHkNfcRw->s_ResponseBuf[10]);
		return false;
	}
	//m_pHkNfcRw->s_ResponseBuf[11] == 0x01
	memcpy(buf, &m_pHkNfcRw->s_ResponseBuf[12], 16);

	return true;
}


/**
 *
 */
bool HkNfcF::write(const uint8_t* buf, uint8_t blockNo/*=0x00*/)
{
	return false;
}


void HkNfcF::setServiceCode(uint16_t svccode)
{
	m_SvcCode = svccode;
}


#ifdef QHKNFCRW_USE_FELICA
/**
 *
 */
bool HkNfcF::reqSystemCode(uint8_t* pNums)
{
	// Request System Codeのテスト
	uint8_t len;

	m_pHkNfcRw->s_CommandBuf[0] = 10;
	m_pHkNfcRw->s_CommandBuf[1] = 0x0c;
	memcpy(m_pHkNfcRw->s_CommandBuf + 2, m_pHkNfcRw->m_NfcId, NFCID_LEN);
	bool ret = m_pNfcPcd->communicateThruEx(
						kDEFAULT_TIMEOUT,
						m_pHkNfcRw->s_CommandBuf, 10,
						m_pHkNfcRw->s_ResponseBuf, &len);
	if (!ret || (m_pHkNfcRw->s_ResponseBuf[0] != m_pHkNfcRw->s_CommandBuf[0]+1)
	  || (memcmp(m_pHkNfcRw->s_ResponseBuf + 1, m_pHkNfcRw->m_NfcId, NFCID_LEN) != 0)) {
		LOGE("req_sys_code : ret=%d", ret);
		return false;
	}

	*pNums = *(m_pHkNfcRw->s_ResponseBuf + 9);

	m_syscode_num = *pNums;
	for(int i=0; i<m_syscode_num; i++) {
		m_syscode[i] = (uint16_t)(*(m_pHkNfcRw->s_ResponseBuf + 10 + i * 2) << 8 | *(m_pHkNfcRw->s_ResponseBuf + 10 + i * 2 + 1));
		LOGD("sys[%d] : %04x", i, m_syscode[i]);
	}

	return true;
}


/**
 *
 */
bool HkNfcF::searchServiceCode()
{
	// Search Service Code
	uint8_t len;
	uint16_t loop = 0x0000;
	m_pHkNfcRw->s_CommandBuf[0] = 12;
	m_pHkNfcRw->s_CommandBuf[1] = 0x0a;
	memcpy(m_pHkNfcRw->s_CommandBuf + 2, m_pHkNfcRw->m_NfcId, NFCID_LEN);

	do {
		m_pHkNfcRw->s_CommandBuf[10] = l16(loop);
		m_pHkNfcRw->s_CommandBuf[11] = h16(loop);
		bool ret = m_pNfcPcd->communicateThruEx(
							kDEFAULT_TIMEOUT,
							m_pHkNfcRw->s_CommandBuf, 12,
							m_pHkNfcRw->s_ResponseBuf, &len);
		if (!ret || (m_pHkNfcRw->s_ResponseBuf[0] != m_pHkNfcRw->s_CommandBuf[0]+1)
		  || (memcmp(m_pHkNfcRw->s_ResponseBuf + 1, m_pHkNfcRw->m_NfcId, NFCID_LEN) != 0)) {
			LOGE("searchServiceCode : ret=%d", ret);
			return false;
		}

		len -= 9;
		const uint8_t* p = &m_pHkNfcRw->s_ResponseBuf[9];
		if(len) {
			uint16_t svc = uint16_t((*(p+1) << 8) | *p);
#ifdef HKNFCRW_ENABLE_DEBUG
			uint16_t code = uint16_t((svc & 0xffc0) >> 6);
			uint8_t  attr = svc & 0x003f;
			if(len == 2) {
				LOGD("%04x(code:%04x : attr:%02x)", svc, code, attr);
			} else {
				LOGD("%04x(%s)", svc, (attr) ? "can" : "cannot");
			}
#endif

			//おしまいチェック
			if(svc == 0xffff) {
				break;
			}
			loop++;
		} else {
			break;
		}
	} while(true);

	return true;
}


/**
 * [FeliCa]PUSHコマンド
 *
 * @param[in]	data		PUSHデータ
 * @param[in]	dataLen		dataの長さ
 *
 * @retval		true			成功
 * @retval		false			失敗
 *
 * @attention	- dataはそのまま送信するため、上位層で加工しておくこと。
 */
bool HkNfcF::push(const uint8_t* data, uint8_t dataLen)
{
	int ret;
	uint8_t responseLen;

	LOGD("%s", __FUNCTION__);

	if (dataLen > 224) {
		LOGE("bad len");
		return false;
	}

	m_pHkNfcRw->s_CommandBuf[0] = (uint8_t)(10 + dataLen);
	m_pHkNfcRw->s_CommandBuf[1] = 0xb0;			//PUSH
	memcpy(m_pHkNfcRw->s_CommandBuf + 2, m_pHkNfcRw->m_NfcId, NFCID_LEN);
	m_pHkNfcRw->s_CommandBuf[10] = dataLen;
	memcpy(m_pHkNfcRw->s_CommandBuf + 11, data, dataLen);

	// xx:IDm
	// [cmd]b0 xx xx xx xx xx xx xx xx len (push data...)
	ret = m_pNfcPcd->communicateThruEx(
						kPUSH_TIMEOUT,
						m_pHkNfcRw->s_CommandBuf, 10 + dataLen,
						m_pHkNfcRw->s_ResponseBuf, &responseLen);
	if (!ret || (responseLen != 10) || (m_pHkNfcRw->s_ResponseBuf[0] != m_pHkNfcRw->s_CommandBuf[0]+1) ||
	  (memcmp(m_pHkNfcRw->s_ResponseBuf + 1, m_pHkNfcRw->m_NfcId, NFCID_LEN) != 0) ||
	  (m_pHkNfcRw->s_ResponseBuf[9] != dataLen)) {

		LOGE("push1 : ret=%d", ret);
		return false;
	}

	// xx:IDm
	// [cmd]a4 xx xx xx xx xx xx xx xx 00
	m_pHkNfcRw->s_CommandBuf[0] = 11;
	m_pHkNfcRw->s_CommandBuf[1] = 0xa4;			//inactivate? activate2?
	memcpy(m_pHkNfcRw->s_CommandBuf + 2, m_pHkNfcRw->m_NfcId, NFCID_LEN);
	m_pHkNfcRw->s_CommandBuf[10] = 0x00;

	ret = m_pNfcPcd->communicateThruEx(
						kDEFAULT_TIMEOUT,
						m_pHkNfcRw->s_CommandBuf, 11,
						m_pHkNfcRw->s_ResponseBuf, &responseLen);
	if (!ret || (responseLen != 10) || (m_pHkNfcRw->s_ResponseBuf[0] != m_pHkNfcRw->s_CommandBuf[0]+1) ||
	  (memcmp(m_pHkNfcRw->s_ResponseBuf + 1, m_pHkNfcRw->m_NfcId, NFCID_LEN) != 0) ||
	  (m_pHkNfcRw->s_ResponseBuf[9] != 0x00)) {

		LOGE("push2 : ret=%d", ret);
		return false;
	}

	msleep(1000);

	return true;
}
#endif	//QHKNFCRW_USE_FELICA


#ifdef HKNFCRW_USE_FALP

uint8_t* HkNfcF::createVnote(const uint8_t* data, uint16_t dataLen, uint16_t* pVnoteLen)
{
	const uint8_t OBEX_PUT[] = {
		0x82,			//PUT
		0x00, 0x00,		//LEN:OBEX_PUT + VNOTE1 + data + VNOTE2

		//Name
		0x01,
		0x00, 0x13,		//Name LEN("pim.vnt") + 3
		0x00, 'p', 0x00, 'i', 0x00, 'm', 0x00, '.', 0x00, 'v', 0x00, 'n', 0x00, 't', 0x00, 0x00,

		//Length
		0xc3,
		0x00, 0x00, 0x00, 0x00,		//LEN:VNOTE1 + data + VNOTE2

		//Body
		0x49,
		0x00, 0x00,		//LEN:Length + 3
	};

	//vNoteのあたま
	const char VNOTE1[] =
		"BEGIN:VNOTE\r\n"
		"VERSION:1.1\r\n"
		"DCREATED:"
			"20110313T030000Z\r\n"
		"LAST-MODIFIED:"
			"20110313T030000Z\r\n"
		"BODY;CHARSET=SHIFT_JIS;ENCODING=QUOTED-PRINTABLE:";

	//vNoteのおしり
	const char VNOTE2[] =
		"\r\n"
		"CATEGORIES:\r\n"
		"END:VNOTE\r\n";

	const uint8_t LEN_OBEX_PUT = uint8_t(sizeof(OBEX_PUT));
	const uint8_t LEN_VNOTE1 = uint8_t(sizeof(VNOTE1) - 1);		// \0
	const uint8_t LEN_VNOTE2 = uint8_t(sizeof(VNOTE2) - 1);		// \0
	const uint16_t LEN_VNOTE = LEN_VNOTE1 + dataLen + LEN_VNOTE2;
	LOGD("LEN_OBEX_PUT : %d", LEN_OBEX_PUT);
	LOGD("LEN_VNOTE1 : %d", LEN_VNOTE1);
	LOGD("LEN_VNOTE2 : %d", LEN_VNOTE2);
	LOGD("LEN_VNOTE : %d", LEN_VNOTE);

	*pVnoteLen = uint16_t(LEN_OBEX_PUT + LEN_VNOTE);
	LOGD("*pVnoteLen : %d", *pVnoteLen);
	uint8_t* pVnote = new uint8_t[*pVnoteLen];
	memcpy(pVnote, OBEX_PUT, LEN_OBEX_PUT);
	memcpy(pVnote + LEN_OBEX_PUT, VNOTE1, LEN_VNOTE1);
	memcpy(pVnote + LEN_OBEX_PUT + LEN_VNOTE1, data, dataLen);
	memcpy(pVnote + LEN_OBEX_PUT + LEN_VNOTE1 + dataLen, VNOTE2, LEN_VNOTE2);

	//PUT全体
	pVnote[1] = h16(LEN_OBEX_PUT + LEN_VNOTE);
	pVnote[2] = l16(LEN_OBEX_PUT + LEN_VNOTE);
	//Object長
	pVnote[0x17] = 0x00;
	pVnote[0x18] = 0x00;
	pVnote[0x19] = h16(LEN_VNOTE);
	pVnote[0x1a] = l16(LEN_VNOTE);
	//Body長
	pVnote[0x1c] = h16(LEN_VNOTE + 3);
	pVnote[0x1d] = l16(LEN_VNOTE + 3);

	LOGD("vNote Len : %d", *pVnoteLen);
	return pVnote;
}


/**
 *
 */
uint8_t HkNfcF::falpPacket(uint16_t UId, uint16_t SId, uint8_t Cmd=kFCMD_SEND, uint8_t Len=0)
{
	uint8_t offset = 9;
	if(Cmd == kFCMD_BYE) {
		m_pHkNfcRw->s_CommandBuf[1] = 0xae;
		Len = 2;
		m_pHkNfcRw->s_CommandBuf[9] = 0x00;
		m_pHkNfcRw->s_CommandBuf[10] = 0x00;
		offset += 2;
	} else {
		m_pHkNfcRw->s_CommandBuf[1] = 0xbc;
		LOGD("SendId: 0x%04x / Cmd:0x%02x", SId, Cmd);
	}
	m_pHkNfcRw->s_CommandBuf[2] = h16(UId);
	m_pHkNfcRw->s_CommandBuf[3] = l16(UId);
	m_pHkNfcRw->s_CommandBuf[4] = 0x01;
	m_pHkNfcRw->s_CommandBuf[5] = (uint8_t)(Len + 3);
	m_pHkNfcRw->s_CommandBuf[6] = Cmd;
	m_pHkNfcRw->s_CommandBuf[7] = h16(SId);
	m_pHkNfcRw->s_CommandBuf[8] = l16(SId);

	return offset;
}


/**
 *
 */
bool HkNfcF::falpCheck(uint8_t* pFalpLen, uint8_t* pRes, uint8_t ChkCode/*=0x01*/)
{
	if(hl16(m_pHkNfcRw->s_ResponseBuf[1], m_pHkNfcRw->s_ResponseBuf[2]) != m_FalpUId) {
		LOGE("not same UID");
		return false;
	}
	if(m_pHkNfcRw->s_ResponseBuf[3] != ChkCode) {
		LOGE("not ChkCode");
		return false;
	}
	*pFalpLen = m_pHkNfcRw->s_ResponseBuf[4];
	if(*pFalpLen < 3) {
		LOGE("less length");
		return false;
	}
	*pRes = m_pHkNfcRw->s_ResponseBuf[5];
	if(*pRes == kFCMD_ADIOS) {
		LOGE("adios...");
		return false;
	}
	m_FalpRId = hl16(m_pHkNfcRw->s_ResponseBuf[6], m_pHkNfcRw->s_ResponseBuf[7]);

	return true;
}


/**
 *
 */
bool HkNfcF::falpObexWait(uint8_t responseLen)
{
	bool ret;
	bool bRet = false;
	uint16_t Nextm_FalpRId = 0xffff;
	uint8_t offset;

	while(true) {
		bool bNext = false;
		if((responseLen == 1) && (m_pHkNfcRw->s_ResponseBuf[0]) == 0x01) {
			LOGD("OBEX wait1 ----");
			ret = m_pNfcPcd->communicateThruEx(
									0x04a6,		//TObはたぶんランダム値
									0, 0,
									m_pHkNfcRw->s_ResponseBuf, &responseLen);
			if(!ret) {
				LOGE("OBEX wait1 fail");
				break;
			}
			for(int i=0; i<responseLen; i++) {
				LOGD("OBEX wait1: %02x", m_pHkNfcRw->s_ResponseBuf[i]);
			}
		} else {
			if(responseLen >= 5) {
				uint8_t len = m_pHkNfcRw->s_ResponseBuf[4];
				if(len >= 3) {
					uint8_t rescode;
					if(!falpCheck(&len, &rescode)) {
						bNext = true;
					}
					LOGD("OBEX wait1 : Res:0x%02x m_FalpRId:0x%04x", rescode, m_FalpRId);
					if(m_FalpRId == Nextm_FalpRId) {
						bNext = true;
						bRet = true;
					}
					if(len > 3) {
						const uint8_t* pObex = m_pHkNfcRw->s_ResponseBuf + 8;
						if(pObex[0] == kOBEX_SUCCESS) {
							//OK
							LOGD("OBEX wait1 : LEN : 0x%02x%02x", pObex[1], pObex[2]);
							Nextm_FalpRId = uint16_t(m_FalpRId + 1);
						}
					}
				}
			}
			if(!bNext) {
				LOGD("OBEX wait2----");
				offset = falpPacket(m_FalpUId, m_FalpSId);
				ret = m_pNfcPcd->communicateThruEx(
										0x0000,
										m_pHkNfcRw->s_CommandBuf, offset,
										m_pHkNfcRw->s_ResponseBuf, &responseLen);
				if(!ret) {
					LOGE("OBEX wait2 fail");
					break;
				}
				for(int i=0; i<responseLen; i++) {
					LOGD("OBEX wait2: %02x", m_pHkNfcRw->s_ResponseBuf[i]);
				}
				m_FalpSId++;
			}
		}

		if(bNext) {
			break;
		}
	}

	return bRet;
}


/**
 *
 */
bool HkNfcF::falpInit()
{
	//RFConfiguration
	const uint8_t RFCFG1[] = {
		0xed, 0x77, 0x1c, 0x11, 0x41, 0x85, 0x61, 0xf1,
	};
	const uint8_t RFCFG2[] = {
		0x02,
	};
	//InListPassiveTarget
	const uint8_t INLISTPASSIVETARGET[] = {
		0x02,				// 0x02:424Kbps
		0x00,
		0xff, 0xff,			// SystemCode
		0x02,				// opt:0x02...Communication Rate
		0x00				// Time Slot
	};
	const uint8_t INLISTPASSIVETARGET_RES[] = {
		0x01,				// System Number
		0x14,				// length(opt:+2)
		0x01				// response code
	};

	bool ret;
	uint8_t responseLen;
	uint8_t* pData;

	// RFConfiguration(Analog Settings)
	ret = m_pNfcPcd->rfConfiguration(0x0b, RFCFG1, sizeof(RFCFG1));
	if(!ret) {
		LOGE("RFConfig fail.");
		return false;
	}

	// Polling(424Kbps)
	memcpy(m_pHkNfcRw->s_CommandBuf, INLISTPASSIVETARGET, sizeof(INLISTPASSIVETARGET));
	ret = m_pNfcPcd->inListPassiveTarget(
				m_pHkNfcRw->s_CommandBuf, sizeof(INLISTPASSIVETARGET),
				&pData, &responseLen);
	if (!ret
	  || (memcmp(&pData[3], INLISTPASSIVETARGET_RES, sizeof(INLISTPASSIVETARGET_RES)) != 0)) {
		LOGE("cannot polling");
		m_SystemCode = kSC_BROADCAST;
		return false;
	}
	if((pData[0x16] != 0x00) || (pData[0x17] != 0x83)) {
		LOGE("not 424kbps");
		return false;
	}
	memcpy(m_pHkNfcRw->m_NfcId, pData + 6, NFCID_LEN);
	m_pHkNfcRw->m_NfcIdLen = NFCID_LEN;

	// RFConfiguration(?)
	ret = m_pNfcPcd->rfConfiguration(0x80, RFCFG2, sizeof(RFCFG2));
	if(!ret) {
		LOGE("RFConfig fail 2.");
		return false;
	}

	return true;
}


/**
 *
 */
bool HkNfcF::falpPrepare(uint16_t uid)
{
	bool ret;
	uint8_t responseLen;
	uint8_t offset;
	uint8_t falplen;
	uint8_t rescode;

	m_FalpUId = uid;

	LOGD("first----");
	const uint8_t HS1[] = {
		0xaa,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	//IDm
		0x00, 0x00,		//UID?
		0x34, 0x08, 0x00, 0x00,
	};
	memcpy(m_pHkNfcRw->s_CommandBuf + 1, HS1, sizeof(HS1));
	memcpy(m_pHkNfcRw->s_CommandBuf + 2, m_pHkNfcRw->m_NfcId, NFCID_LEN);
	m_pHkNfcRw->s_CommandBuf[9] = h16(m_FalpUId);
	m_pHkNfcRw->s_CommandBuf[10] = l16(m_FalpUId);
	ret = m_pNfcPcd->communicateThruEx(
							0x1068,
							m_pHkNfcRw->s_CommandBuf, sizeof(HS1),
							m_pHkNfcRw->s_ResponseBuf, &responseLen);
	if(ret) {
		for(int i=0; i<responseLen; i++) {
			LOGD("%02x", m_pHkNfcRw->s_ResponseBuf[i]);
		}
	} else {
		LOGE("Thru1");
		return false;
	}

	LOGD("second----");
	const uint8_t kHELLO[] = {
		0x08, 0x01, 0x00,
		'D', 'C', 'M', 'I', '2', '0',
		0x02, 0x00, 0x00, 0xf9, 0x00, 0x00, 0x05, 0xdc, 0x0b, 0x00,
		0x06, 0x77, 0x46, 0x0e, 0x08, 0x00, 0x03, 0x84, 0x00, 0x00,
		0x00, 0x0b, 0xb8, 0x00, 0x01, 0x86, 0xa0, 0x00, 0x00, 0x13,
		0x88, 0x00, 0x01, 0x86, 0xa0, 0x00, 0x03, 0x0d, 0x40, 0x00,
		0x00, 0x00, 0x00, 0x00,
	};
	offset = falpPacket(m_FalpUId, 0x0000, kFCMD_HELLO, sizeof(kHELLO));
	memcpy(m_pHkNfcRw->s_CommandBuf + offset, kHELLO, sizeof(kHELLO));
	ret = m_pNfcPcd->communicateThruEx(
							0x1770,
							m_pHkNfcRw->s_CommandBuf, uint8_t(offset + sizeof(kHELLO)),
							m_pHkNfcRw->s_ResponseBuf, &responseLen);
	if(!ret) {
		LOGE("Thru2");
		return false;
	}
	for(int i=0; i<responseLen; i++) {
		LOGD("%02x", m_pHkNfcRw->s_ResponseBuf[i]);
	}
	if(responseLen < 0x2f) {
		LOGE("[HELLO]less response : %02x", responseLen);
		return false;
	}
	if(!falpCheck(&falplen, &rescode)) {
		return false;
	}
	if(rescode != kFCMD_YEAH) {
		LOGE("not HELLO response");
		return false;
	}

	return true;
}


/**
 *
 */
bool HkNfcF::falpFinish()
{
	bool ret;
	uint8_t responseLen;
	uint8_t offset;

	LOGD("end----");
	offset = falpPacket(m_FalpUId, 0x0000, kFCMD_BYE);
	ret = m_pNfcPcd->communicateThruEx(
						0x07d0,
						m_pHkNfcRw->s_CommandBuf, offset,
						m_pHkNfcRw->s_ResponseBuf, &responseLen);
	if(ret) {
		for(int i=0; i<responseLen; i++) {
			LOGD("%02x", m_pHkNfcRw->s_ResponseBuf[i]);
		}
	} else {
		LOGE("Thru4");
		return false;
	}

	//停止
	m_pNfcPcd->rfOff();

	return true;
}


/**
 *
 */
bool HkNfcF::falpObexConnect()
{
	bool ret;
	uint8_t responseLen;
	uint8_t offset;

	LOGD("\n----OBEX connect----");
	const uint8_t OBEX_CONN[] = {
		0x80,			//CONNECT
		0x00, 0x07,		//LEN
		0x10,			//VERSION
		0x00,			//Flags
		0x80, 0x00,		//MAX LEN
	};
	offset = falpPacket(m_FalpUId, m_FalpSId, kFCMD_SEND, sizeof(OBEX_CONN));
	memcpy(m_pHkNfcRw->s_CommandBuf + offset, OBEX_CONN, sizeof(OBEX_CONN));
	ret = m_pNfcPcd->communicateThruEx(
							0x0000,
							m_pHkNfcRw->s_CommandBuf, offset + sizeof(OBEX_CONN),
							m_pHkNfcRw->s_ResponseBuf, &responseLen);
	if(ret) {
//		for(int i=0; i<responseLen; i++) {
//			LOGD("%02x", m_pHkNfcRw->s_ResponseBuf[i]);
//		}
	} else {
		LOGE("CONN");
		return false;
	}
	m_FalpSId++;

	ret = falpObexWait(responseLen);
	if(ret) {
		const uint8_t* pObex = m_pHkNfcRw->s_ResponseBuf + 8;
		LOGD("Version : %02x", pObex[3]);
		LOGD("Flags : %02x", pObex[4]);
		LOGD("max LEN : 0x%02x%02x", pObex[5], pObex[6]);
	} else {
		LOGE("OBEX connect fail");
		return false;
	}

	return true;
}


/**
 *
 */
bool HkNfcF::falpObexPut(const uint8_t* data, uint16_t dataLen)
{
	bool ret;
	uint8_t responseLen = 0;
	uint16_t sendLen = 0;
	const uint8_t LEN_MAX = 242;

	LOGD("\n----OBEX put----");

	while(dataLen) {
		uint8_t len;
		uint8_t cmd;

		if(dataLen > LEN_MAX) {
			//分割送信
			len = LEN_MAX;
			dataLen -= LEN_MAX;
			cmd = kFCMD_CSND;
		} else {
			len = dataLen;
			dataLen = 0;
			cmd = kFCMD_SEND;
		}

		uint8_t offset = falpPacket(m_FalpUId, m_FalpSId, cmd, len);
		LOGD("offset : %d / len : %d", offset, len);
		memcpy(m_pHkNfcRw->s_CommandBuf + offset, data + sendLen, len);
		ret = m_pNfcPcd->communicateThruEx(
								0x0000,
								m_pHkNfcRw->s_CommandBuf, uint8_t(offset + len),
								m_pHkNfcRw->s_ResponseBuf, &responseLen);
		if(!ret) {
			LOGE("PUT");
			return false;
		}
		sendLen += len;

		//相手の応答は見ない・・・でいいはずはないが、見方がわからん

		m_FalpSId++;
	}

	ret = falpObexWait(responseLen);
	if(!ret) {
		LOGE("OBEX put fail");
	}

	return true;
}


/**
 *
 */
bool HkNfcF::falpObexDisconnect()
{
	bool ret;
	uint8_t responseLen;
	uint8_t offset;

	LOGD("\n----OBEX disconnect----");
	const uint8_t OBEX_DISC[] = {
		0x81,			//DISCONNECT
		0x00, 0x03,		//LEN
	};
	offset = falpPacket(m_FalpUId, m_FalpSId, kFCMD_SEND, sizeof(OBEX_DISC));
	memcpy(m_pHkNfcRw->s_CommandBuf + offset, OBEX_DISC, sizeof(OBEX_DISC));
	ret = m_pNfcPcd->communicateThruEx(
							0x0000,
							m_pHkNfcRw->s_CommandBuf, offset + sizeof(OBEX_DISC),
							m_pHkNfcRw->s_ResponseBuf, &responseLen);
	if(!ret) {
		LOGE("DISC");
		return false;
	}
	m_FalpSId++;

	ret = falpObexWait(responseLen);
	if(!ret) {
		LOGE("OBEX disconnect fail");
	}

	return true;
}


/**
 *
 */
bool HkNfcF::falpSendString(const uint8_t* data, uint16_t dataLen)
{
	LOGD("%s : %d", __PRETTY_FUNCTION__, dataLen);

	if(!falpInit()) {
		return false;
	}

	if(!falpPrepare(0x1234)) {
		return false;
	}

	//iC通信開始

	//OBEX connect
	if(!falpObexConnect()) {
		return false;
	}

	//OBEX put
	uint16_t vnoteLen;
	uint8_t* pVnote = createVnote(data, dataLen, &vnoteLen);
	if(!falpObexPut(pVnote, vnoteLen)) {
		return false;
	}
	delete[] pVnote;

	//OBEX disconnect
	if(!falpObexDisconnect()) {
		return false;
	}

	if(!falpFinish()) {
		return false;
	}

	return true;
}
#endif	//HKNFCRW_USE_FALP
