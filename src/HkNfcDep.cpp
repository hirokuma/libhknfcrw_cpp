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
	const uint8_t LlcpGb[] = {
		0x46, 0x66, 0x6d,		// LLCP Magic Number
		0x01, 0x01, 0x10,		// TLV0:VERSION[MUST] ... 1.0
		0x03, 0x02, 0x00, 0x13,	// TLV1:WKS[SHOULD]
												// bit0 : must
												// bit1 : SDP
												// bit4 : SNEP
								// http://www.nfc-forum.org/specs/nfc_forum_assigned_numbers_register
		0x04, 0x01, 200			// TLV2:LTO[MAY] ... 10ms x 200 = 2000ms
								// LTO > RWT
	};
	
	// PDU解析の戻り値で使用する。
	// 「このPDUのデータ部はService Data Unitなので、末尾までデータです」という意味。
	const uint8_t SDU = 0xff;
}


/**
 * コンストラクタ
 * 
 * [in]	pRw		有効なHkNfcRwポインタ
 */
HkNfcDep::HkNfcDep(HkNfcRw* pRw)
	: m_pHkNfcRw(pRw),
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
 * [in]		pGb		GeneralBytes(未使用の場合、0)
 * [in]		GbLen	pGb長(最大48byteだが、チェックしない)
 * @retval	true	成功
 * @retval	false	失敗
 */
bool HkNfcDep::startAsInitiator(DepMode mode, bool bLlcp/* =true */)
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
	prm.pResponse = m_pHkNfcRw->s_ResponseBuf;
	prm.ResponseLen = 0;
	if(bLlcp) {
		prm.pGb = LlcpGb;
		prm.GbLen = (uint8_t)sizeof(LlcpGb);
	} else {
		prm.pGb = 0;
		prm.GbLen = 0;
	}

	if(!NfcPcd::inJumpForDep(&prm)) {
		LOGE("inJumpForDep\n");
		return false;
	}

	m_DepMode = mode;
	m_bInitiator = true;

	if(bLlcp) {
		const uint8_t* pRecv = prm.pResponse;
#if 1
		for(int i=0; i<prm.ResponseLen; i++) {
			LOGD("[I]%02x \n", pRecv[i]);
		}
#endif

		if(prm.ResponseLen < 19) {
			// TgInitTarget Res(16) + Magic Number(3)
			LOGE("small len : %d\n", prm.ResponseLen);
			return false;
		}

		int pos = 0;

		// Tg
		if(pRecv[pos++] != 0x01) {
			LOGE("bad Tg\n");
			return false;
		}

		// NFCID3(skip)
		pos += HkNfcRw::NFCID3_LEN;

		//DIDt, BSt, BRt
		if((pRecv[pos] == 0x00) && (pRecv[pos+1] == 0x00) && (pRecv[pos+2] == 0x00)) {
			//OK
		} else {
			LOGE("bad DID/BS/BR\n");
			return false;
		}
		pos += 3;

		//TO(skip)
		pos++;

		//PPt
		if(pRecv[pos++] != 0x32) {
			LOGE("bad PP\n");
			return false;
		}

		// Gt

		// Magic Number
		if((pRecv[pos] == 0x46) && (pRecv[pos+1] == 0x66) && (pRecv[pos+2] == 0x6d)) {
			//OK
		} else {
			LOGE("bad Magic Number\n");
			return false;
		}
		pos += 3;

		//Link activation
		while(pos < prm.ResponseLen) {
			//ここでPDU解析
			PduType pdu;
			pos += analyzeParamList(&pRecv[pos]);
		}
	}

	return true;
}


/**
 * NFC-DEP開始(Target)
 * 
 * [in]		pGb			GeneralBytes(未使用の場合、0)
 * [in]		GbLen		pGb長(最大48byteだが、チェックしない)
 * @retval	true	成功
 * @retval	false	失敗
 */
bool HkNfcDep::startAsTarget(bool bLlcp/* =true */)
{
	if(m_DepMode != DEP_NONE) {
		LOGE("Already DEP mode\n");
		return false;
	}

	NfcPcd::TargetParam prm;
	prm.pCommand = m_pHkNfcRw->s_ResponseBuf;
	prm.CommandLen = 0;

	if(bLlcp) {
		prm.pGb = LlcpGb;
		prm.GbLen = (uint8_t)sizeof(LlcpGb);
		//fAutomaticATR_RES=0
		NfcPcd::setParameters(0x18);
	} else {
		prm.pGb = 0;
		prm.GbLen = 0;
		//fAutomaticATR_RES=1
		NfcPcd::setParameters(0x1c);
	}

	// Target時の通信性能向上
	const uint8_t target[] = { 0x63, 0x0d, 0x08 };
	NfcPcd::writeRegister(target, sizeof(target));

	//Initiator待ち
    if(!NfcPcd::tgInitAsTarget(&prm)) {
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

	if(m_pHkNfcRw->s_ResponseBuf[1] != prm.CommandLen - 1) {
		LOGE("bad size, %d, %d\n", m_pHkNfcRw->s_ResponseBuf[1], prm.CommandLen);
		return false;
	}

	const uint8_t* pIniCmd = &(m_pHkNfcRw->s_ResponseBuf[2]);
	uint8_t IniCmdLen = prm.CommandLen - 2;

	if((pIniCmd[0] >= 3) && (pIniCmd[1] == 0xd4) && (pIniCmd[2] == 0x0a)) {
		// RLS_REQなら、終わらせる
		const uint16_t timeout = 2000;
		m_pHkNfcRw->s_CommandBuf[0] = 3;
		m_pHkNfcRw->s_CommandBuf[1] = 0xd5;
		m_pHkNfcRw->s_CommandBuf[2] = 0x0b;			// RLS_REQ
		m_pHkNfcRw->s_CommandBuf[3] = pIniCmd[3];	// DID
		uint8_t res_len;
		NfcPcd::communicateThruEx(timeout,
						m_pHkNfcRw->s_CommandBuf, 4,
						m_pHkNfcRw->s_ResponseBuf, &res_len);
		m_DepMode = DEP_NONE;
	}

	if(m_DepMode == ACT_106K) {
		// 106k active

		// 特性を元に戻す
		const uint8_t normal[] = { 0x63, 0x0d, 0x00 };
		NfcPcd::writeRegister(normal, sizeof(normal));
	} else if(m_DepMode == PSV_106K) {
		// 106k passive

		// 特性を元に戻す
		const uint8_t normal[] = { 0x63, 0x0d, 0x00, 0x63, 0x01, 0x3b };
		NfcPcd::writeRegister(normal, sizeof(normal));
	} else if(m_DepMode == DEP_NONE) {
		LOGE("invalid response\n");
		return false;
	}
	
	if(bLlcp) {
#if 1
		for(int i=0; i<IniCmdLen; i++) {
			LOGD("[I]%02x \n", pIniCmd[i]);
		}
#endif

		if(IniCmdLen < 19) {
			// ATR_REQ(16) + Magic Number(3)
			LOGE("small len : %d\n", IniCmdLen);
			return false;
		}

		// ATR_REQチェック
		int pos = 0;

		// ATR_REQ
		if((pIniCmd[pos] == 0xd4) && (pIniCmd[pos+1] == 0x00)) {
			//OK
		} else {
			LOGE("not ATR_REQ\n");
			return false;
		}
		pos += 2;
		
		// NFCID3(skip)
		pos += HkNfcRw::NFCID3_LEN;

		//DIDi, BSi, BRi
		if((pIniCmd[pos] == 0x00) && (pIniCmd[pos+1] == 0x00) && (pIniCmd[pos+2] == 0x00)) {
			//OK
		} else {
			LOGE("bad DID/BS/BR\n");
			return false;
		}
		pos += 3;

		//PPi
		if(pIniCmd[pos++] != 0x32) {
			LOGE("bad PP\n");
			return false;
		}

		// Gi

		// Magic Number
		if((pIniCmd[pos] == 0x46) && (pIniCmd[pos+1] == 0x66) && (pIniCmd[pos+2] == 0x6d)) {
			//OK
		} else {
			LOGE("bad Magic Number\n");
			return false;
		}
		pos += 3;

		//Link activation
		while(pos < IniCmdLen) {
			//ここでPDU解析
			PduType pdu;
			pos += analyzeParamList(&pIniCmd[pos]);
		}


		bool ret = NfcPcd::tgSetGeneralBytes(&prm);
		if(ret) {
			//モード確認
			ret = NfcPcd::getGeneralStatus(m_pHkNfcRw->s_ResponseBuf);
		}
		if(ret) {
			if(m_pHkNfcRw->s_ResponseBuf[NfcPcd::GGS_TXMODE] != NfcPcd::GGS_TXMODE_DEP) {
				//DEPではない
				LOGE("not DEP mode\n");
				ret = false;
			}
		}
	}
	
	return true;
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
	bool b = NfcPcd::inDataExchange(
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
	bool b = NfcPcd::communicateThruEx(timeout,
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
	bool b = NfcPcd::tgGetData(p, pCommandLen);
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
	bool b = NfcPcd::tgSetData(
					reinterpret_cast<const uint8_t*>(pResponse), ResponseLen);
	return b;
}


/*******************************************************************
 * LLCP
 *******************************************************************/

uint8_t (HkNfcDep::*HkNfcDep::sAnalyzePdu[])(const uint8_t* pBuf) = {
	&HkNfcDep::analyzeSymm,
	&HkNfcDep::analyzePax,
	&HkNfcDep::analyzeAgf,
	&HkNfcDep::analyzeUi,
	&HkNfcDep::analyzeConn,
	&HkNfcDep::analyzeDisc,
	&HkNfcDep::analyzeCc,
	&HkNfcDep::analyzeDm,
	&HkNfcDep::analyzeFrmr,
	&HkNfcDep::analyzeDummy,		//0x09
	&HkNfcDep::analyzeDummy,		//0x0a
	&HkNfcDep::analyzeDummy,		//0x0b
	&HkNfcDep::analyzeI,
	&HkNfcDep::analyzeRr,
	&HkNfcDep::analyzeRnr,
	&HkNfcDep::analyzeDummy		//0x0f
};

uint8_t HkNfcDep::analyzePdu(const uint8_t* pBuf, PduType* pResPdu)
{
	*pResPdu = (PduType)(((*pBuf & 0x03) << 2) | (((*pBuf+1) & 0x0c) >> 6));
	uint8_t next;

	next = (this->*sAnalyzePdu[*pResPdu])(pBuf);
	return next;
}

uint8_t HkNfcDep::analyzeSymm(const uint8_t* pBuf)
{
	LOGD("PDU_SYMM\n");
	return 2;
}

uint8_t HkNfcDep::analyzePax(const uint8_t* pBuf)
{
	LOGD("PDU_PAX\n");
	return analyzeParamList(pBuf + 2);
}

uint8_t HkNfcDep::analyzeAgf(const uint8_t* pBuf)
{
	LOGD("PDU_AGF\n");
	return (uint8_t)(*(pBuf + 2) + 1);
}

uint8_t HkNfcDep::analyzeUi(const uint8_t* pBuf)
{
	LOGD("PDU_UI\n");
	return SDU;		//終わりまでデータが続く
}

uint8_t HkNfcDep::analyzeConn(const uint8_t* pBuf)
{
	LOGD("PDU_CONN\n");
	return analyzeParamList(pBuf + 2);
}

uint8_t HkNfcDep::analyzeDisc(const uint8_t* pBuf)
{
	LOGD("PDU_DISC\n");
	return 2;
}

uint8_t HkNfcDep::analyzeCc(const uint8_t* pBuf)
{
	LOGD("PDU_CC\n");
	return analyzeParamList(pBuf + 2);
}

uint8_t HkNfcDep::analyzeDm(const uint8_t* pBuf)
{
	LOGD("PDU_DM\n");
	return 3;
}

uint8_t HkNfcDep::analyzeFrmr(const uint8_t* pBuf)
{
	LOGD("PDU_FRMR\n");
	return 0;
}

uint8_t HkNfcDep::analyzeI(const uint8_t* pBuf)
{
	LOGD("PDU_I\n");
	return 0;
}

uint8_t HkNfcDep::analyzeRr(const uint8_t* pBuf)
{
	LOGD("PDU_RR\n");
	return 0;
}

uint8_t HkNfcDep::analyzeRnr(const uint8_t* pBuf)
{
	LOGD("PDU_RNR\n");
	return 0;
}

uint8_t HkNfcDep::analyzeDummy(const uint8_t* pBuf)
{
	LOGD("dummy dummy dummy\n");
	return 0;
}

uint8_t HkNfcDep::analyzeParamList(const uint8_t *pBuf)
{
	LOGD("Parameter List\n");
	uint8_t next = (uint8_t)(2 + *(pBuf + 1));
	switch(*pBuf) {
	case 0x01:	//VERSION
		LOGD("VERSION\n");
		break;
	case 0x02:	//MIUX
		LOGD("MIUX\n");
		break;
	case 0x03:	//WKS
		LOGD("WKS\n");
		break;
	case 0x04:	//LTO
		LOGD("LTO\n");
		break;
	case 0x05:
		LOGD("RW\n");
		break;
	case 0x06:
		LOGD("SN\n");
		break;
	default:
		LOGE("unknown\n");
		next = 0xff;
		break;
	}
	return next;
}

