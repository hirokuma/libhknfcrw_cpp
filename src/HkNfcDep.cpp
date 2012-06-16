#include <cstring>
#include <cstdlib>

#include "HkNfcDep.h"
#include "NfcPcd.h"
#include "misc.h"

#define LOG_TAG "HkNfcDep"
#include "nfclog.h"


using namespace HkNfcRwMisc;


/**
 * コンストラクタ
 * 
 * [in]	pRw		有効なHkNfcRwポインタ
 */
HkNfcDep::HkNfcDep(HkNfcRw* pRw)
	: m_pHkNfcRw(pRw), m_pNfcPcd(pRw->m_pNfcPcd)
{
}


/**
 * デストラクタ
 */
HkNfcDep::~HkNfcDep()
{
}


/**
 * NFC-DEP開始(Initiator)
 * 
 * [in]		mode		開始するDepMode
 * [in]		pGt			GeneralBytes(未使用の場合、0)
 * [in]		GtLen		pGt長(最大48byteだが、チェックしない)
 */
bool HkNfcDep::startAsInitiator(DepMode mode, const uint8_t* pGt/* =0 */, uint8_t GtLen/* =0 */)
{
	NfcPcd::DepInitiatorParam prm;
	
	prm.Ap = (mode & _AP_MASK) ? NfcPcd::AP_ACTIVE : NfcPcd::AP_PASSIVE;

	uint32_t br = mode & _BR_MASK;
	switch(br) {
	case _BR106K:
		prm.Br = NfcPcd::BR_106K;
		break;
	case _BR212K:
		prm.Br = NfcPcd::BR_212K;
		break;
	case _BR424K:
	default:
		prm.Br = NfcPcd::BR_424K;
		break;
	}
	
	prm.pNfcId3 = 0;		// R/Wで自動生成
	prm.pGt = pGt;
	prm.GtLen = GtLen;

	if(!m_pNfcPcd->inJumpForDep(&prm)) {
		LOGE("inJumpForDep\n");
		return false;
	}

	return true;
}


/**
 * NFC-DEP開始(Target)
 * 
 * [in]		pGt			GeneralBytes(未使用の場合、0)
 * [in]		GtLen		pGt長(最大48byteだが、チェックしない)
 */
bool HkNfcDep::startAsTarget(const uint8_t* pGt/* =0 */, uint8_t GtLen/* =0 */)
{
	NfcPcd::TargetParam prm;

	prm.pGt = pGt;
	prm.GtLen = GtLen;

	// Target時の通信性能向上
	const uint8_t target[] = { 0x63, 0x0d, 0x08 };
	m_pNfcPcd->writeRegister(target, sizeof(target));

	//Initiator待ち
	uint8_t res_len;
    if(!m_pNfcPcd->tgInitAsTarget(&prm, m_pHkNfcRw->s_ResponseBuf, &res_len)) {
		LOGE("tgInitAsTarget\n");
		return false;
	}
	uint8_t br = m_pHkNfcRw->s_ResponseBuf[0] & 0x70;
#if 1
	switch(br) {
	case 0x00:
		LOGD("106kbps\n");
		break;
	case 0x10:
		LOGD("212kbps\n");
		break;
	case 0x20:
		LOGD("424kbps\n");
		break;
	default:
		LOGE("unknown bps : %02x\n", br);
		break;
	}
#endif

	uint8_t comm = m_pHkNfcRw->s_ResponseBuf[0] & 0x03;
#if 1
	switch(comm) {
	case 0x00:
		LOGD("106kbps passive\n");
		break;
	case 0x01:
		LOGD("106kbps active\n");
		break;
	case 0x02:
		LOGD("212k/424kbps passive\n");
		break;
	default:
		LOGE("unknown comm : %02x\n", comm);
		break;
	}
#endif

	const uint8_t* pIniCmd = &(m_pHkNfcRw->s_ResponseBuf[1]);
	uint8_t IniCmdLen = res_len - 1;
#if 1
	for(int i=0; i<IniCmdLen; i++) {
		LOGD("%02x \n", pIniCmd[i]);
	}
#endif

	//モード確認
	bool ret = m_pNfcPcd->getGeneralStatus(m_pHkNfcRw->s_ResponseBuf);
	if(ret) {
		if(m_pHkNfcRw->s_ResponseBuf[NfcPcd::GGS_TXMODE] != NfcPcd::GGS_TXMODE_DEP) {
			//DEPではない
			ret = false;
		}
	}

	return ret;

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

#if 0
    // card emulation
    if(m_pHkNfcRw->s_ResponseBuf[1] == 0x0c) {
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
}
