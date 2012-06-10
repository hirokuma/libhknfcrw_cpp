#include "HkNfcB.h"
#include "nfcpcd.h"

#define LOG_TAG "HkNfcB"
#include "nfclog.h"


namespace {
	//Polling
	const uint8_t INLISTPASSIVETARGET[] = { 0x03, 0x00 };
	const uint8_t INLISTPASSIVETARGET_RES = 0x01;
}

HkNfcB::HkNfcB(HkNfcRw* pRw)
	: m_pHkNfcRw(pRw), m_pNfcPcd(pRw->m_pNfcPcd)
{
}

HkNfcB::~HkNfcB()
{
}

/**
 * [NFC-B]Polling
 *
 * @retval		true			成功
 * @retval		false			失敗
 */
bool HkNfcB::polling()
{
	int ret;
	uint8_t responseLen;
	uint8_t* pData;

	ret = m_pNfcPcd->inListPassiveTarget(
					INLISTPASSIVETARGET, sizeof(INLISTPASSIVETARGET),
					&pData, &responseLen);
	if (!ret
	  || (pData[1] != INLISTPASSIVETARGET_RES)) {
		//LOGE("pollingA fail: ret=%d/len=%d", ret, responseLen);
		return false;
	}

	m_pHkNfcRw->m_NfcIdLen = 12;
	//LOGD("SEL_RES:%02x", m_pHkNfcRw->m_NfcIdLen);

	memcpy(m_pHkNfcRw->m_NfcId, m_pHkNfcRw->s_ResponseBuf + 4, m_pHkNfcRw->m_NfcIdLen);

	return true;
}

