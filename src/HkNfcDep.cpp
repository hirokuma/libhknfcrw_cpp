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

	//fAutomaticATR_RES=0
	m_pNfcPcd->setParameters(0x18);

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
		LOGD("Active\n");
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

	if((pIniCmd[0] >= 3) && (pIniCmd[1] == 0xd4) && (pIniCmd[2] == 0x0a)) {
		// RLS_REQなら、終わらせる
		stopAsTarget(pIniCmd[3]);
	}

	if((br == 0x00) && (comm == 0x01)) {
		// 106k active

		// 特性を元に戻す
		const uint8_t normal[] = { 0x63, 0x0d, 0x00 };
		m_pNfcPcd->writeRegister(normal, sizeof(normal));
	} else if((br == 0x00) && (comm == 0x00)) {
		// 106k passive

		// 特性を元に戻す
		const uint8_t normal[] = { 0x63, 0x0d, 0x00, 0x63, 0x01, 0x3b };
		m_pNfcPcd->writeRegister(normal, sizeof(normal));
	}

	m_pNfcPcd->tgSetGeneralBytes(&prm);

	//モード確認
	bool ret = m_pNfcPcd->getGeneralStatus(m_pHkNfcRw->s_ResponseBuf);
	if(ret) {
		if(m_pHkNfcRw->s_ResponseBuf[NfcPcd::GGS_TXMODE] != NfcPcd::GGS_TXMODE_DEP) {
			//DEPではない
			ret = false;
		}
	}

	return ret;
}


/**
 * [DEP-Initiator]データ送信
 * 
 * @param	[in]	pCommand		Targetへの送信データ
 * @param	[in]	CommandLen		Targetへの送信データサイズ
 * @param	[out]	pResponse		Targetからの返信データ
 * @param	[out]	pResponseLen	Targetからの返信データサイズ
 * @retval	true	成功
 * @retval	false	失敗(pResponse/pResponseLenは無効)
 */
bool HkNfcDep::sendAsInitiator(
			const void* pCommand, uint8_t CommandLen,
			void* pResponse, uint8_t* pResponseLen)
{
	bool b = m_pNfcPcd->inDataExchange(
					reinterpret_cast<const uint8_t*>(pCommand), CommandLen,
					reinterpret_cast<uint8_t*>(pResponse), pResponseLen);
	return b;
}


bool HkNfcDep::stopAsInitiator()
{
	msleep(100);

	const uint16_t timeout = 2000;
	m_pHkNfcRw->s_CommandBuf[0] = 3;
	m_pHkNfcRw->s_CommandBuf[1] = 0xd4;
	m_pHkNfcRw->s_CommandBuf[2] = 0x0a;		// RLS_REQ
	m_pHkNfcRw->s_CommandBuf[3] = 0x00;		// DID
	uint8_t res_len;
	bool b = m_pNfcPcd->communicateThruEx(timeout,
					m_pHkNfcRw->s_CommandBuf, 4,
					m_pHkNfcRw->s_ResponseBuf, &res_len);
	return b;
}


/**
 * [DEP-Target]データ受信
 * 
 * @param	[out]	pCommand	Initiatorからの送信データ
 * @param	[out]	CommandLen	Initiatorからの送信データサイズ
 * @retval	true	成功
 * @retval	false	失敗(pCommand/pCommandLenは無効)
 */
bool HkNfcDep::recvAsTarget(void* pCommand, uint8_t* pCommandLen)
{
	uint8_t* p = reinterpret_cast<uint8_t*>(pCommand);
	bool b = m_pNfcPcd->tgGetData(p, pCommandLen);
	return b;
}

/**
 * [DEP-Target]データ送信
 * 
 * @param	[in]	pResponse		Initiatorへの返信データ
 * @param	[in]	ResponseLen		Initiatorへの返信データサイズ
 * @retval	true	成功
 * @retval	false	失敗
 */
bool HkNfcDep::respAsTarget(const void* pResponse, uint8_t ResponseLen)
{
	bool b = m_pNfcPcd->tgSetData(
					reinterpret_cast<const uint8_t*>(pResponse), ResponseLen);
	return b;
}


bool HkNfcDep::stopAsTarget(uint8_t did)
{
	const uint16_t timeout = 2000;
	m_pHkNfcRw->s_CommandBuf[0] = 3;
	m_pHkNfcRw->s_CommandBuf[1] = 0xd5;
	m_pHkNfcRw->s_CommandBuf[2] = 0x0b;		// RLS_REQ
	m_pHkNfcRw->s_CommandBuf[3] = did;		// DID
	uint8_t res_len;
	bool b = m_pNfcPcd->communicateThruEx(timeout,
					m_pHkNfcRw->s_CommandBuf, 4,
					m_pHkNfcRw->s_ResponseBuf, &res_len);
	return b;
}
