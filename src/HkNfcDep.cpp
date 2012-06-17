#include <cstring>
#include <cstdlib>

#include "HkNfcDep.h"
#include "NfcPcd.h"
#include "misc.h"

#define LOG_TAG "HkNfcDep"
#include "nfclog.h"


using namespace HkNfcRwMisc;


namespace {
	/// LLCPのGeneralBytes
	const uint8_t LlcpGt[] = {
		0x46, 0x66, 0x6d,		// LLCP Magic Number
		0x01, 0x01, 0x10,		// TLV0:VERSION[MUST] ... 1.0
		0x03, 0x02, 0x00, 0x13,	// TLV1:WKS[SHOULD] ... 0:must, 1:SDP, 4:SNEP
								// http://www.nfc-forum.org/specs/nfc_forum_assigned_numbers_register
		0x04, 0x01, 200			// TLV2:LTO[MAY] ... 10ms x 200 = 2000ms
								// LTO > RWT
	};
	
	bool checkLlcpGeneralBytes(const uint8_t* pRecv, uint8_t RecvBuf)
	{
		return false;
	}
}


/**
 * コンストラクタ
 * 
 * [in]	pRw		有効なHkNfcRwポインタ
 */
HkNfcDep::HkNfcDep(HkNfcRw* pRw)
	: m_pHkNfcRw(pRw), m_pNfcPcd(pRw->m_pNfcPcd),
	  m_DepMode(DEP_NONE), m_bInitiator(false)
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
 * [in]		mode	開始するDepMode
 * [in]		pGt		GeneralBytes(未使用の場合、0)
 * [in]		GtLen	pGt長(最大48byteだが、チェックしない)
 * @retval	true	成功
 * @retval	false	失敗
 */
bool HkNfcDep::startAsInitiator(DepMode mode, const uint8_t* pGt/* =0 */, uint8_t GtLen/* =0 */,
								bool (*pFunc)(const uint8_t* pRecv, uint8_t RecvLen)/* =0 */)
{
	if(m_DepMode != DEP_NONE) {
		LOGE("Already DEP mode\n");
		return false;
	}

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
	prm.pResponse = m_pHkNfcRw->s_ResponseBuf;
	prm.ResponseLen = 0;

	if(!m_pNfcPcd->inJumpForDep(&prm)) {
		LOGE("inJumpForDep\n");
		return false;
	}

	m_DepMode = mode;
	m_bInitiator = true;

	bool ret = true;
	if(pFunc) {
		ret = (*pFunc)(prm.pResponse, prm.ResponseLen);
	}

	return ret;
}


/**
 * NFC-DEP開始(Target)
 * 
 * [in]		pGt			GeneralBytes(未使用の場合、0)
 * [in]		GtLen		pGt長(最大48byteだが、チェックしない)
 * @retval	true	成功
 * @retval	false	失敗
 */
bool HkNfcDep::startAsTarget(const uint8_t* pGt/* =0 */, uint8_t GtLen/* =0 */)
{
	if(m_DepMode != DEP_NONE) {
		LOGE("Already DEP mode\n");
		return false;
	}

	NfcPcd::TargetParam prm;

	prm.pGt = pGt;
	prm.GtLen = GtLen;
	prm.pCommand = m_pHkNfcRw->s_ResponseBuf;
	prm.CommandLen = 0;

	//fAutomaticATR_RES=0
	m_pNfcPcd->setParameters(0x18);

	// Target時の通信性能向上
	const uint8_t target[] = { 0x63, 0x0d, 0x08 };
	m_pNfcPcd->writeRegister(target, sizeof(target));

	//Initiator待ち
    if(!m_pNfcPcd->tgInitAsTarget(&prm)) {
		LOGE("tgInitAsTarget\n");
		return false;
	}
	uint8_t br = m_pHkNfcRw->s_ResponseBuf[0] & 0x70;
	switch(br) {
	case 0x00:
		LOGD("106kbps\n");
		m_DepMode = (DepMode)_BR106K;	//あとでorして完成する
		break;
	case 0x10:
		LOGD("212kbps\n");
		m_DepMode = (DepMode)_BR212K;	//あとでorして完成する
		break;
	case 0x20:
		LOGD("424kbps\n");
		m_DepMode = (DepMode)_BR424K;	//あとでorして完成する
		break;
	default:
		LOGE("unknown bps : %02x\n", br);
		m_DepMode = DEP_NONE;
		break;
	}

	uint8_t comm = m_pHkNfcRw->s_ResponseBuf[0] & 0x03;
	switch(comm) {
	case 0x00:
		LOGD("106kbps passive\n");
		m_DepMode = (DepMode)(m_DepMode | _PSV);
		break;
	case 0x01:
		LOGD("Active\n");
		m_DepMode = (DepMode)(m_DepMode | _ACT);
		break;
	case 0x02:
		LOGD("212k/424kbps passive\n");
		m_DepMode = (DepMode)(m_DepMode | _PSV);
		break;
	default:
		LOGE("unknown comm : %02x\n", comm);
		m_DepMode = DEP_NONE;
		break;
	}


	const uint8_t* pIniCmd = &(m_pHkNfcRw->s_ResponseBuf[1]);
	uint8_t IniCmdLen = prm.CommandLen - 1;
#if 1
	for(int i=0; i<IniCmdLen; i++) {
		LOGD("%02x \n", pIniCmd[i]);
	}
#endif

	if((pIniCmd[0] >= 3) && (pIniCmd[1] == 0xd4) && (pIniCmd[2] == 0x0a)) {
		// RLS_REQなら、終わらせる
		const uint16_t timeout = 2000;
		m_pHkNfcRw->s_CommandBuf[0] = 3;
		m_pHkNfcRw->s_CommandBuf[1] = 0xd5;
		m_pHkNfcRw->s_CommandBuf[2] = 0x0b;			// RLS_REQ
		m_pHkNfcRw->s_CommandBuf[3] = pIniCmd[3];	// DID
		uint8_t res_len;
		m_pNfcPcd->communicateThruEx(timeout,
						m_pHkNfcRw->s_CommandBuf, 4,
						m_pHkNfcRw->s_ResponseBuf, &res_len);
		m_DepMode = DEP_NONE;
	}

	//ATR_REQ以外だったら終わらせた方がよいが、また考えよう

	if(m_DepMode == ACT_106K) {
		// 106k active

		// 特性を元に戻す
		const uint8_t normal[] = { 0x63, 0x0d, 0x00 };
		m_pNfcPcd->writeRegister(normal, sizeof(normal));
	} else if(m_DepMode == PSV_106K) {
		// 106k passive

		// 特性を元に戻す
		const uint8_t normal[] = { 0x63, 0x0d, 0x00, 0x63, 0x01, 0x3b };
		m_pNfcPcd->writeRegister(normal, sizeof(normal));
	} else if(m_DepMode == DEP_NONE) {
		LOGE("invalid response\n");
		return false;
	}
	

	m_pNfcPcd->tgSetGeneralBytes(&prm);

	//モード確認
	bool ret = m_pNfcPcd->getGeneralStatus(m_pHkNfcRw->s_ResponseBuf);
	if(ret) {
		if(m_pHkNfcRw->s_ResponseBuf[NfcPcd::GGS_TXMODE] != NfcPcd::GGS_TXMODE_DEP) {
			//DEPではない
			LOGE("not DEP mode\n");
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


/**
 * [DEP-Initiator]データ交換終了(RLS_REQ送信)
 * @retval	true	成功
 * @retval	false	失敗
 */
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


/*******************************************************************
 * LLCP
 *******************************************************************/

/**
 * LLCP開始(Initiator)
 * 
 * [in]		mode	開始するDepMode
 * @retval	true	成功
 * @retval	false	失敗
 */
bool HkNfcDep::startLlcpInitiator(DepMode mode)
{
	bool b = startAsInitiator(mode, LlcpGt, (uint8_t)sizeof(LlcpGt), checkLlcpGeneralBytes);
	
	return b;
}


/**
 * LLCP開始(Target)
 * 
 * @retval	true	成功
 * @retval	false	失敗
 */
bool HkNfcDep::startLlcpTarget()
{
	bool b = startAsTarget(LlcpGt, (uint8_t)sizeof(LlcpGt));
	return b;
}
//
//void HkNfcDep::createLlcpPdu()
//{
//}
//
