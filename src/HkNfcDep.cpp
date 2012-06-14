#include "HkNfcDep.h"
#include "nfcpcd.h"
#include "misc.h"

#define LOG_TAG "HkNfcDep"
#include "nfclog.h"

#include <cstring>

using namespace HkNfcRwMisc;

HkNfcDep::HkNfcDep(HkNfcRw* pRw)
	: m_pHkNfcRw(pRw), m_pNfcPcd(pRw->m_pNfcPcd)
{
}

HkNfcDep::~HkNfcDep()
{
}


bool HkNfcDep::startAsInitiator()
{
#if 0

	m_pNfcPcd->setNfcId3iAsId2(id2);

	if(!m_pNfcPcd->inJumpForDep(NfcPcd::AP_PASSIVE, NfcPcd::BR_424K, true, 0, 0)) {
		LOGE("inJumpForDep");
		return false;
	}

	LOGD("OK 1");
    msleep(5000);

	uint8_t cmd[] = { 0x12, 0x34, 0x56 };
	uint8_t cmd_len = uint8_t(sizeof(cmd));
	uint8_t res[256];
	uint8_t res_len = 0;
	if(m_pNfcPcd->inDataExchange(cmd, cmd_len, res, &res_len)) {
		LOGD("read : ");
		for(int i=0; i<res_len; i++) {
			LOGD("%02x ", res[i]);
		}
		LOGD("----------1-----------------------\n");
	}
	//m_pNfcPcd->inRelease();

	return true;
#endif

}


bool HkNfcDep::startAsTarget()
{
#if 0

	m_pNfcPcd->reset();

	//IDmÝ’è
	m_pNfcPcd->setNfcId3tAsId2(id2);

	//Initiator‘Ò‚¿
    if(!m_pNfcPcd->tgInitAsTarget(syscode, 0, 0)) {
		LOGE("tgInitAsTarget");
		return false;
	}

	LOGD("OK 1");
	

	bool ret = m_pNfcPcd->getGeneralStatus(s_ResponseBuf);
	if(ret) {
		switch(s_ResponseBuf[NfcPcd::GGS_TXMODE]) {
		case NfcPcd::GGS_TXMODE_DEP:
			LOGD("TxMode:DEP\n");
			break;
		case NfcPcd::GGS_TXMODE_FALP:
			LOGD("TxMode:FALP\n");
			break;
		default:
			break;
		}
	}

#if 0
    //DEP
	uint8_t cmd[] = { 0xfe, 0xdc, 0xba, 0x98 };
	uint8_t cmd_len = uint8_t(sizeof(cmd));
	uint8_t res[256];
	uint8_t res_len = 0;
	if(m_pNfcPcd->tgGetData(res, &res_len)) {
		LOGD("read : ");
		for(int i=0; i<res_len; i++) {
			LOGD("%02x ", res[i]);
		}
		LOGD("----------1-----------------------\n");

		if(!m_pNfcPcd->tgSetData(cmd, cmd_len)) {
			//break;
		}
		LOGD("----------2-----------------------\n");
	}
	//m_pNfcPcd->inRelease();
#endif

#if 1
    // card emulation
    if(s_ResponseBuf[1] == 0x0c) {
		LOGD("send Request SysCode res.\n");
		uint8_t cmd[13];
		cmd[0] = 13;
		cmd[1] = 0x0d;		//Request System Code response
		std::memcpy(&cmd[2], id2, NFCID2_LEN);
		cmd[10] = 0x01;
		cmd[11] = h16(syscode);
		cmd[12] = l16(syscode);
		uint8_t cmd_len = uint8_t(sizeof(cmd));
		uint8_t res[256];
		uint8_t res_len = 0;
		if(m_pNfcPcd->tgResponseToInitiator(cmd, cmd_len, res, &res_len)) {
			LOGD("read : ");
			for(int i=0; i<res_len; i++) {
				LOGD("%02x ", res[i]);
			}
		}
	}
#endif

	return true;

#endif
}
