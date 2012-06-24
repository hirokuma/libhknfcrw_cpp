#include "HkNfcRw.h"
#include "HkNfcB.h"
#include "NfcPcd.h"

#define LOG_TAG "HkNfcB"
#include "nfclog.h"


namespace {
	//Polling
	const uint8_t INLISTPASSIVETARGET[] = { 0x03, 0x00 };
	const uint8_t INLISTPASSIVETARGET_RES = 0x01;
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

	ret = NfcPcd::inListPassiveTarget(
					INLISTPASSIVETARGET, sizeof(INLISTPASSIVETARGET),
					&pData, &responseLen);
	if (!ret
	  || (pData[1] != INLISTPASSIVETARGET_RES)) {
		//LOGE("pollingA fail: ret=%d/len=%d", ret, responseLen);
		return false;
	}

	HkNfcRw::nfcIdLen() = 12;
	//LOGD("SEL_RES:%02x", HkNfcRw::nfcIdLen());

	memcpy(HkNfcRw::nfcId(), HkNfcRw::responseBuf() + 4, HkNfcRw::nfcIdLen());

	return true;
}

