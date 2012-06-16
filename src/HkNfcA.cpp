#include <string.h>

#include "HkNfcA.h"
#include "NfcPcd.h"

#define LOG_TAG "HkNfcA"
#include "nfclog.h"


namespace {
	//Polling
	const uint8_t INLISTPASSIVETARGET[] = { 0x00 };
	const uint8_t INLISTPASSIVETARGET_RES = 0x01;

	//Key A Authentication
	const uint8_t KEY_A_AUTH = 0x60;
	const uint8_t KEY_B_AUTH = 0x61;
	const uint8_t READ = 0x30;
	const uint8_t UPDATE = 0xa0;
}


HkNfcA::HkNfcA(HkNfcRw* pRw)
	: m_pHkNfcRw(pRw), m_pNfcPcd(pRw->m_pNfcPcd)
{
}

HkNfcA::~HkNfcA()
{
}


/**
 * [NFC-A]Polling
 *
 * @retval		true			成功
 * @retval		false			失敗
 */
bool HkNfcA::polling()
{
	int ret;
	uint8_t responseLen;
	uint8_t* pData;

	ret = m_pNfcPcd->inListPassiveTarget(
					INLISTPASSIVETARGET, sizeof(INLISTPASSIVETARGET),
					&pData, &responseLen);
	if (!ret
	  || (responseLen != 12)
	  || (pData[1] != INLISTPASSIVETARGET_RES)) {
		//LOGE("pollingA fail: ret=%d/len=%d", ret, responseLen);
		return false;
	}

	m_TargetNo = *(pData + 3);
	LOGD("TargetNo : %02x", m_TargetNo);

	m_SensRes = (uint16_t)((*(pData + 4) << 8) | *(pData + 5));
	LOGD("SENS_RES:%04x", m_SensRes);

	m_SelRes = (SelRes)*(pData + 6);
	const char* sel_res;
	switch(m_SelRes) {
	case MIFARE_UL:			sel_res = "MIFARE Ultralight";		break;
	case MIFARE_1K:			sel_res = "MIFARE 1K";				break;
	case MIFARE_MINI:		sel_res = "MIFARE MINI";			break;
	case MIFARE_4K:			sel_res = "MIFARE 4K";				break;
	case MIFARE_DESFIRE:	sel_res = "MIFARE DESFIRE";			break;
	case JCOP30:			sel_res = "JCOP30";					break;
	case GEMPLUS_MPCOS:		sel_res = "Gemplus MPCOS";			break;
	default:
		m_SelRes = SELRES_UNKNOWN;
		sel_res = "???";
	}
	LOGD("SEL_RES:%02x(%s)", m_SelRes, sel_res);

	m_pHkNfcRw->m_NfcIdLen = *(pData + 7);
	//LOGD("SEL_RES:%02x", m_pHkNfcRw->m_NfcIdLen);

	memcpy(m_pHkNfcRw->m_NfcId, pData + 8, m_pHkNfcRw->m_NfcIdLen);

	return true;
}


bool HkNfcA::read(uint8_t* buf, uint8_t blockNo)
{
	m_pHkNfcRw->s_CommandBuf[0] = m_TargetNo;
	m_pHkNfcRw->s_CommandBuf[2] = blockNo;
	m_pHkNfcRw->s_CommandBuf[3] = 0xff;
	m_pHkNfcRw->s_CommandBuf[4] = 0xff;
	m_pHkNfcRw->s_CommandBuf[5] = 0xff;
	m_pHkNfcRw->s_CommandBuf[6] = 0xff;
	m_pHkNfcRw->s_CommandBuf[7] = 0xff;
	m_pHkNfcRw->s_CommandBuf[8] = 0xff;
	memcpy(m_pHkNfcRw->s_CommandBuf + 9, m_pHkNfcRw->m_NfcId, m_pHkNfcRw->m_NfcIdLen);

	uint8_t len;
	bool ret;

#if 1
	// Key A Authentication
	m_pHkNfcRw->s_CommandBuf[1] = KEY_A_AUTH;
	ret = m_pNfcPcd->inDataExchange(
					m_pHkNfcRw->s_CommandBuf, 9 + m_pHkNfcRw->m_NfcIdLen,
					m_pHkNfcRw->s_ResponseBuf, &len);
	if(!ret) {
		LOGE("read fail1");
		return false;
	}
#endif

#if 0
	// Key B Authentication
	m_pHkNfcRw->s_CommandBuf[1] = KEY_B_AUTH;
	ret = m_pNfcPcd->inDataExchange(
					m_pHkNfcRw->s_CommandBuf, 9 + m_pHkNfcRw->m_NfcIdLen,
					m_pHkNfcRw->s_ResponseBuf, &len);
	if(!ret) {
		LOGE("read fail2");
		return false;
	}
#endif

	// Read
	m_pHkNfcRw->s_CommandBuf[1] = READ;
	ret = m_pNfcPcd->inDataExchange(
					m_pHkNfcRw->s_CommandBuf, 3,
					m_pHkNfcRw->s_ResponseBuf, &len);
	if(ret) {
		memcpy(buf, m_pHkNfcRw->s_ResponseBuf, len);
	} else {
		LOGE("read fail3");
	}

	return ret;
}

bool HkNfcA::write(const uint8_t* buf, uint8_t blockNo)
{
	return false;
}
