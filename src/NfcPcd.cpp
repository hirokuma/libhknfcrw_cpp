/**
 * @file	NfcPcd.cpp
 * @brief	NFCのPCD(Proximity Coupling Device)アクセス実装.
 * 			RC-S620/S用(今のところ).
 */

#include <cstdlib>
#include "NfcPcd.h"
#include "devaccess.h"
#include "misc.h"

// ログ用
#define LOG_TAG "NfcPcd"
#include "nfclog.h"
#define ENABLE_FRAME_LOG

using namespace HkNfcRwMisc;

/**
 * const
 */
namespace {
	const uint16_t RWBUF_MAX = NfcPcd::DATA_MAX + 10;
	const uint16_t NORMAL_DATA_MAX = 255;	//Normalフレームのデータ部最大
	const uint16_t RESHEAD_LEN = 2;
	
	const int POS_RESDATA = 2;
	const uint8_t ACK[] = { 0x00, 0x00, 0xff, 0x00, 0xff, 0x00 };
	
	const int POS_NORMALFRM_DATA = 5;
	const int POS_EXTENDFRM_DATA = 8;
}

/**
 * バッファ
 */
namespace {
	/// 送信バッファ
	uint8_t s_SendBuf[RWBUF_MAX];

	/// Normalフレームのデータ部
	uint8_t* s_NormalFrmBuf = &(s_SendBuf[POS_NORMALFRM_DATA]);
	/// Extendedフレームのデータ部
	uint8_t* s_ExtendFrmBuf = &(s_SendBuf[POS_EXTENDFRM_DATA]);

	/// 受信バッファ
	uint8_t s_ResponseBuf[RWBUF_MAX];
}


/**
 * 内部用関数
 */
namespace {

	/**
	 * DCS計算
	 *
	 * @param[in]		data		計算元データ
	 * @param[in]		len			dataの長さ
	 *
	 * @return			DCS値
	 */
	uint8_t _calc_dcs(const uint8_t* data, uint16_t len)
	{
		uint8_t sum = 0;
		for(uint16_t i = 0; i < len; i++) {
			sum += data[i];
		}
		return (uint8_t)(0 - (sum & 0xff));
	}

}


///////////////////////////////////////////////////////////////////////////
// 実装
///////////////////////////////////////////////////////////////////////////

/**
 * インスタンス取得(singleton)
 */
NfcPcd* NfcPcd::getInstance()
{
	static NfcPcd sNfcPcd;
	
	return &sNfcPcd;
}


/**
 * コンストラクタ
 */
NfcPcd::NfcPcd()
	: m_bOpened(false)
{
	s_SendBuf[0] = 0x00;
	s_SendBuf[1] = 0x00;
	s_SendBuf[2] = 0xff;
}


/**
 * デストラクタ
 *
 * @attention		- ポートが開いている場合は、#rfOff()を呼び出す。
 */
NfcPcd::~NfcPcd()
{
	if(m_bOpened) {
		rfOff();
		reset();
	}
	portClose();

	m_bOpened = false;
}


/**
 * シリアルポートオープン
 *
 * @retval	true		成功
 * @retval	false		失敗
 */
bool NfcPcd::portOpen()
{
	m_bOpened = DevAccess::open();
	return m_bOpened;
}


/**
 * シリアルポートクローズ
 */
void NfcPcd::portClose()
{
	DevAccess::close();
	m_bOpened = false;
}


/**
 * デバイス初期化
 *
 * @retval	true		初期化成功(=使用可能)
 * @retval	false		初期化失敗
 * @attention			初期化失敗時には、#rfOff()を呼び出すこと
 */
bool NfcPcd::init()
{
	//LOGD("%s", __PRETTY_FUNCTION__);

	bool ret;
	uint16_t res_len;

	sendAck();

	ret = reset();
	if(!ret) {
		return false;
	}

	// RF通信のT/O
	s_NormalFrmBuf[0] = 0xd4;
	s_NormalFrmBuf[1] = 0x32;
	s_NormalFrmBuf[2] = 0x02;		// T/O
	s_NormalFrmBuf[3] = 0x00;		// RFU
	s_NormalFrmBuf[4] = 0x00;		// ATR_RES : no timeout
	s_NormalFrmBuf[5] = 0x00;		// 非DEP通信時 : no timeout
	ret = sendCmd(s_NormalFrmBuf, 6, s_ResponseBuf, &res_len);
	if(!ret || (res_len != RESHEAD_LEN)) {
		LOGE("d4 32 02\n");
		return false;
	}

	// Target捕捉時のRF通信リトライ回数
	s_NormalFrmBuf[2] = 0x05;		// Retry
	s_NormalFrmBuf[3] = 0x00;		// ATR_REQ/RES : only once
	s_NormalFrmBuf[4] = 0x00;		// PSL_REQ/RES : only once
	s_NormalFrmBuf[5] = 0x00;		// InListPassiveTarget : only once
	ret = sendCmd(s_NormalFrmBuf, 6, s_ResponseBuf, &res_len);
	if(!ret || (res_len != RESHEAD_LEN)) {
		LOGE("d4 32 05(%d)%d\n", ret, res_len);
		return false;
	}

	// RF出力ONからTargetID取得コマンド送信までの追加ウェイト時間
	s_NormalFrmBuf[2] = 0x81;		// wait
	s_NormalFrmBuf[3] = 0xb7;		// 0.1 x prm + 5 + 0.7 [ms]
									// 0xb7は、サンプルソースの値
	ret = sendCmd(s_NormalFrmBuf, 4, s_ResponseBuf, &res_len);
	if(!ret || (res_len != RESHEAD_LEN)) {
		LOGE("d4 32 81(%d)%d\n", ret, res_len);
		return false;
	}

#if 0
	// DEPターゲットでのタイムアウト
	s_NormalFrmBuf[2] = 0x82;		// DEP Target Timeout
	s_NormalFrmBuf[3] = 0x08;		// gbyAtrResTo
	s_NormalFrmBuf[4] = 0x01;		// gbyRtox
	s_NormalFrmBuf[5] = 0x08;		// gbyTargetStoTo
	ret = sendCmd(s_NormalFrmBuf, 6, s_ResponseBuf, &res_len);
	if(!ret || (res_len != RESHEAD_LEN)) {
		LOGE("d4 32 82(%d)%d\n", ret, res_len);
		//return false;
	}
#endif


#if 0
	// nfcpyのまね4
	s_NormalFrmBuf[0] = 0xd4;
	s_NormalFrmBuf[1] = 0x32;		//RFConfiguration
	s_NormalFrmBuf[2] = 0x0b;
	memcpy(s_NormalFrmBuf + 3, s_ResponseBuf + POS_RESDATA, READREG_LEN);
	ret = sendCmd(s_NormalFrmBuf, 3+READREG_LEN, s_ResponseBuf, &res_len);
	if(!ret || res_len != RESHEAD_LEN) {
		LOGE("32 0b(%d) / %d\n", ret, res_len);
		//return false;
	}
#endif

	return ret;
}


/**
 * 搬送波停止
 *
 * @retval		true			成功
 * @retval		false			失敗
 */
bool NfcPcd::rfOff()
{
	//LOGD("%s", __PRETTY_FUNCTION__);

	uint16_t res_len;
	s_NormalFrmBuf[0] = 0xd4;
	s_NormalFrmBuf[1] = 0x32;
	s_NormalFrmBuf[2] = 0x01;		// RF field
	s_NormalFrmBuf[3] = 0x00;		// bit1 : Auto RFCA : OFF
									// bit0 : RF ON/OFF : OFF
	bool ret = sendCmd(s_NormalFrmBuf, 4, s_ResponseBuf, &res_len);
	if(!ret || (res_len != RESHEAD_LEN)) {
		LOGE("rfOff ret=%d\n", ret);
		return false;
	}

	return true;
}


/**
 * RFConfiguration
 *
 * @param[in]	cmd				コマンド
 * @param[in]	pCommand		送信するコマンド引数
 * @param[in]	CommandLen		pCommandの長さ
 * @retval	true		成功
 * @retval	false		失敗
 */
bool NfcPcd::rfConfiguration(uint8_t cmd, const uint8_t* pCommand, uint8_t CommandLen)
{
	//LOGD("%s", __PRETTY_FUNCTION__);

	s_NormalFrmBuf[0] = 0xd4;
	s_NormalFrmBuf[1] = 0x32;		//RFConfiguration
	s_NormalFrmBuf[2] = cmd;
	memcpy(s_NormalFrmBuf + 3, pCommand, CommandLen);

	uint16_t res_len;
	bool ret = sendCmd(s_NormalFrmBuf, 3 + CommandLen, s_ResponseBuf, &res_len);
	if(!ret || (res_len != RESHEAD_LEN)) {
		LOGE("rfConfiguration ret=%d\n", ret);
		return false;
	}

	return true;
}


/**
 * Reset
 *
 * @retval		true			成功
 * @retval		false			失敗
 */
bool NfcPcd::reset()
{
	//LOGD("%s", __PRETTY_FUNCTION__);

	s_NormalFrmBuf[0] = 0xd4;
	s_NormalFrmBuf[1] = 0x18;		//Reset
	s_NormalFrmBuf[2] = 0x01;

	uint16_t res_len;
	bool ret = sendCmd(s_NormalFrmBuf, 3, s_ResponseBuf, &res_len);
	if(!ret || (res_len != RESHEAD_LEN)) {
		LOGE("reset ret=%d\n", ret);
		return false;
	}
	
	// execute
	sendAck();
	msleep(10);

	return true;
}


/**
 * Diagnose
 *
 * @param[in]	pCommand		送信するコマンド
 * @param[in]	CommandLen		pCommandの長さ
 * @param[out]	pResponse		レスポンス
 * @param[out]	pResponseLen	pResponseの長さ
 * @retval		true			成功
 * @retval		false			失敗
 */
bool NfcPcd::diagnose(uint8_t cmd, const uint8_t* pCommand, uint8_t CommandLen,
											uint8_t* pResponse, uint8_t* pResponseLen)
{
	//LOGD("%s", __PRETTY_FUNCTION__);

	s_NormalFrmBuf[0] = 0xd4;
	s_NormalFrmBuf[1] = 0x00;			//Diagnose
	s_NormalFrmBuf[2] = cmd;
	memcpy(s_NormalFrmBuf + 3, pCommand, CommandLen);
	
	uint16_t res_len;
	bool ret = sendCmd(s_NormalFrmBuf, 3 + CommandLen, s_ResponseBuf, &res_len);
	if(!ret || (res_len < RESHEAD_LEN)) {
		LOGE("diagnose ret=%d/%d\n", ret, res_len);
		return false;
	}
	
	*pResponseLen = res_len - RESHEAD_LEN;
	memcpy(pResponse, s_ResponseBuf + RESHEAD_LEN, *pResponseLen);

	return true;
}


/**
 * SetParameters
 *
 * @retval		true			成功
 * @retval		false			失敗
 */
bool NfcPcd::setParameters(uint8_t val)
{
	//LOGD("%s", __PRETTY_FUNCTION__);

	s_NormalFrmBuf[0] = 0xd4;
	s_NormalFrmBuf[1] = 0x12;
	s_NormalFrmBuf[2] = val;

	uint16_t res_len;
	bool ret = sendCmd(s_NormalFrmBuf, 3, s_ResponseBuf, &res_len);
	if(!ret || (res_len != RESHEAD_LEN)) {
		LOGE("setParam ret=%d\n", ret);
	}

	return true;
}


/**
 * WriteRegister
 *
 * @retval		true			成功
 * @retval		false			失敗
 */
bool NfcPcd::writeRegister(const uint8_t* pCommand, uint8_t CommandLen)
{
	//LOGD("%s", __PRETTY_FUNCTION__);

	s_NormalFrmBuf[0] = 0xd4;
	s_NormalFrmBuf[1] = 0x08;
	memcpy(s_NormalFrmBuf + 2, pCommand, CommandLen);

	uint16_t res_len;
	bool ret = sendCmd(s_NormalFrmBuf, 2 + CommandLen, s_ResponseBuf, &res_len);
	if(!ret || (res_len < RESHEAD_LEN + CommandLen/2)) {
		LOGE("writeReg ret=%d res_len=%d\n", ret, res_len);
	} else {
		for(int i=0; i<CommandLen/2; i++) {
			if(s_ResponseBuf[2+i] != 0x00) {
				LOGE("writeReg [%02x]\n", s_ResponseBuf[2+i]);
				return false;
			}
		}
	}

	return true;
}

/**
 * GetFirmwareVersion
 *
 * @param[out]	pResponse		レスポンス(GF_XXでアクセス)
 * @retval		true			成功
 * @retval		false			失敗
 */
bool NfcPcd::getFirmwareVersion(uint8_t* pResponse)
{
	//LOGD("%s\n", __PRETTY_FUNCTION__);

	const uint16_t RES_LEN = 4;
	s_NormalFrmBuf[0] = 0xd4;
	s_NormalFrmBuf[1] = 0x02;

	uint16_t res_len;
	bool ret = sendCmd(s_NormalFrmBuf, 2, s_ResponseBuf, &res_len);
	if(!ret || (res_len < RESHEAD_LEN + RES_LEN)) {
		LOGE("getFirmware ret=%d\n", ret);
		return false;
	}
	
	memcpy(pResponse, s_ResponseBuf + RESHEAD_LEN, RES_LEN);

	return true;
}


/**
 * GetGeneralStatus
 *
 * @param[out]	pResponse		レスポンス(GGS_XXでアクセス)
 * @retval		true			成功
 * @retval		false			失敗
 */
bool NfcPcd::getGeneralStatus(uint8_t* pResponse)
{
	//LOGD("%s", __PRETTY_FUNCTION__);

	s_NormalFrmBuf[0] = 0xd4;
	s_NormalFrmBuf[1] = 0x04;

	const uint16_t RES_LEN = 5;
	uint16_t res_len;
	bool ret = sendCmd(s_NormalFrmBuf, 2, s_ResponseBuf, &res_len);
	if(!ret || (res_len < RESHEAD_LEN + RES_LEN)) {
		LOGE("getGeneralStatus ret=%d/%d\n", ret, res_len);
		return false;
	}
	
	memcpy(pResponse, s_ResponseBuf + RESHEAD_LEN, RES_LEN);

	return true;
}


/**
 * CommunicateThruEX
 *
 * @retval		true			成功
 * @retval		false			失敗
 */
bool NfcPcd::communicateThruEx()
{
	//LOGD("%s : [%d]", __PRETTY_FUNCTION__, CommandLen);

	s_NormalFrmBuf[0] = 0xd4;
	s_NormalFrmBuf[1] = 0xa0;		//CommunicateThruEX
	s_NormalFrmBuf[2] = 0x00;
	s_NormalFrmBuf[3] = 0x00;

	uint16_t res_len;
	bool ret = sendCmd(s_NormalFrmBuf, 4, s_ResponseBuf, &res_len);
	if(!ret || (res_len < RESHEAD_LEN+1)) {
		LOGE("communicateThruEx ret=%d\n", ret);
		return false;
	}

	return true;
}


/**
 * CommunicateThruEX
 *
 * @param[in]	pCommand		送信するコマンド
 * @param[in]	CommandLen		pCommandの長さ
 * @param[out]	pResponse		レスポンス
 * @param[out]	pResponseLen	pResponseの長さ
 *
 * @retval		true			成功
 * @retval		false			失敗
 */
bool NfcPcd::communicateThruEx(
			const uint8_t* pCommand, uint8_t CommandLen,
			uint8_t* pResponse, uint8_t* pResponseLen)
{
	//LOGD("%s : [%d]", __PRETTY_FUNCTION__, CommandLen);

	s_NormalFrmBuf[0] = 0xd4;
	s_NormalFrmBuf[1] = 0xa0;		//CommunicateThruEX
	memcpy(s_NormalFrmBuf + 2, pCommand, CommandLen);

	uint16_t res_len;
	bool ret = sendCmd(s_NormalFrmBuf, 2 + CommandLen, s_ResponseBuf, &res_len);
	if(!ret || (res_len < RESHEAD_LEN+1)) {
		LOGE("communicateThruEx ret=%d\n", ret);
		return false;
	}
	if(res_len == 3) {
		//Statusを返す
		pResponse[0] = s_ResponseBuf[2];
		*pResponseLen = 1;
	} else {
		if((s_ResponseBuf[2] != 0x00) || (res_len != (3 + s_ResponseBuf[3]))) {
			return false;
		}
		//Statusは返さない
		*pResponseLen = (uint8_t)(s_ResponseBuf[3] - 1);
		memcpy(pResponse, s_ResponseBuf + 4, *pResponseLen);
	}

	return true;
}


/**
 * CommunicateThruEX
 *
 * @param[in]	Timeout			タイムアウト値[msec]
 * @param[in]	pCommand		送信するコマンド
 * @param[in]	CommandLen		pCommandの長さ
 * @param[out]	pResponse		レスポンス
 * @param[out]	pResponseLen	pResponseの長さ
 *
 * @retval		true			成功
 * @retval		false			失敗
 *
 * @note		-# #Timeoutは往復分の時間を設定すること.
 */
bool NfcPcd::communicateThruEx(
			uint16_t Timeout,
			const uint8_t* pCommand, uint8_t CommandLen,
			uint8_t* pResponse, uint8_t* pResponseLen)
{
	//LOGD("%s : (%d)", __PRETTY_FUNCTION__, CommandLen);

	s_NormalFrmBuf[0] = 0xd4;
	s_NormalFrmBuf[1] = 0xa0;		//CommunicateThruEX
	s_NormalFrmBuf[2] = l16(Timeout);
	s_NormalFrmBuf[3] = h16(Timeout);
	if(CommandLen) {
		memcpy(s_NormalFrmBuf + 4, pCommand, CommandLen);
		CommandLen += 4;
	}

	uint16_t res_len;
	bool ret = sendCmd(s_NormalFrmBuf, CommandLen, s_ResponseBuf, &res_len);
	if(!ret || (res_len < RESHEAD_LEN+1)) {
		LOGE("communicateThruEx ret=%d\n", ret);
		return false;
	}
	if(res_len == 3) {
		//Statusを返す
		pResponse[0] = s_ResponseBuf[POS_RESDATA];
		*pResponseLen = 1;
	} else {
		//Statusは返さない
		if((s_ResponseBuf[POS_RESDATA] != 0x00) || (res_len != (3 + s_ResponseBuf[POS_RESDATA+1]))) {
			return false;
		}
		*pResponseLen = (uint8_t)(s_ResponseBuf[POS_RESDATA+1] - 1);
		memcpy(pResponse, s_ResponseBuf + RESHEAD_LEN + 2, *pResponseLen);
	}

	return true;
}


/////////////////////////////////////////////////////////////////////////
// Initiator Command
/////////////////////////////////////////////////////////////////////////


/**
 * InJumpForDEP or InJumpForPSL
 *
 * @param[in]	pParam		DEP initiatorパラメータ
 */
bool NfcPcd::_inJump(uint8_t Cmd,
		const DepInitiatorParam* pParam)
{
	//LOGD("%s", __PRETTY_FUNCTION__);

	s_NormalFrmBuf[0] = 0xd4;
	s_NormalFrmBuf[1] = Cmd;
	s_NormalFrmBuf[2] = (uint8_t)pParam->Ap;
	s_NormalFrmBuf[3] = (uint8_t)pParam->Br;
	s_NormalFrmBuf[4] = 0x00;		//Next
	uint8_t len = 5;
	if(pParam->Ap == AP_PASSIVE) {
		if(pParam->Br == BR_106K) {
//			s_NormalFrmBuf[4] |= 0x01;
//			const uint8_t known_id[4] = { 0x08, 0x01, 0x02, 0x03 };
//			memcpy(&(s_NormalFrmBuf[len]), known_id, sizeof(known_id));
//			len += sizeof(known_id);
		} else {
			s_NormalFrmBuf[4] |= 0x01;
			const uint8_t pol_req[5] = { 0x00, 0xff, 0xff, 0x01, 0x00 };
			memcpy(&(s_NormalFrmBuf[len]), pol_req, sizeof(pol_req));
			len += sizeof(pol_req);
		}
	}
	if(pParam->pNfcId3) {
		s_NormalFrmBuf[4] |= 0x02;
		memcpy(&(s_NormalFrmBuf[len]), pParam->pNfcId3, NFCID3_LEN);
		len += NFCID3_LEN;
	}
	if(pParam->pGt && pParam->GtLen) {
		s_NormalFrmBuf[4] |= 0x04;
		memcpy(&(s_NormalFrmBuf[len]), pParam->pGt, pParam->GtLen);
		len += pParam->GtLen;
	}

//	for(int i=0; i<len; i++) {
//		LOGD("%02x ", s_NormalFrmBuf[i]);
//	}

	uint16_t res_len;
	bool ret = sendCmd(s_NormalFrmBuf, len, s_ResponseBuf, &res_len);

//	for(int i=0; i<res_len; i++) {
//		LOGD("%02x ", s_ResponseBuf[i]);
//	}

	if(!ret || (res_len < RESHEAD_LEN+17)) {
		LOGE("inJumpForDep ret=%d/len=%d\n", ret, res_len);
		return false;
	}

	return true;
}


/**
 * InJumpForDEP
 *
 * @param[in]	pParam		DEP initiatorパラメータ
 * @param[in]	pGt			Gt(Initiator)
 * @param[in]	GtLen		Gtサイズ
 */
bool NfcPcd::inJumpForDep(
		const DepInitiatorParam* pParam)
{
	//LOGD("%s", __PRETTY_FUNCTION__);
	return _inJump(0x56, pParam);
}


/**
 * InJumpForPSL
 *
 * @param[in]	pParam		DEP initiatorパラメータ
 * @param[in]	pGt			Gt(Initiator)
 * @param[in]	GtLen		Gtサイズ
 */
bool NfcPcd::inJumpForPsl(
		const DepInitiatorParam* pParam)
{
	//LOGD("%s", __PRETTY_FUNCTION__);
	return _inJump(0x46, pParam);
}


/**
 * InListPassiveTarget
 *
 * @param[in]	pInitData		InListPassiveTargetの引数
 * @param[in]	InitLen			pInitDataの長さ
 * @param[out]	ppTgData		InListPassiveTargeの戻り値
 * @param[out]	pTgLen			*ppTgDataの長さ
 *
 * @retval		true			成功
 * @retval		false			失敗
 */
bool NfcPcd::inListPassiveTarget(
			const uint8_t* pInitData, uint8_t InitLen,
			uint8_t** ppTgData, uint8_t* pTgLen)
{
	uint16_t responseLen;
	s_NormalFrmBuf[0] = 0xd4;
	s_NormalFrmBuf[1] = 0x4a;				//InListPassiveTarget
	s_NormalFrmBuf[2] = 0x01;
	memcpy(s_NormalFrmBuf + 3, pInitData, InitLen);

	bool ret = sendCmd(s_NormalFrmBuf, 3+InitLen, s_ResponseBuf, &responseLen);
	if(!ret || s_ResponseBuf[POS_RESDATA] != 0x01) {
		return false;
	}
	*ppTgData = s_ResponseBuf;
	*pTgLen = static_cast<uint8_t>(responseLen);

	return true;
}


/**
 * InDataExchange
 *
 * @param[in]	pCommand		送信するコマンド
 * @param[in]	CommandLen		pCommandの長さ
 * @param[out]	pResponse		レスポンス
 * @param[out]	pResponseLen	pResponseの長さ
 * @param[in]	bCoutinue		MIフラグを立てるかどうか
 *
 * @retval		true			成功
 * @retval		false			失敗
 */
bool NfcPcd::inDataExchange(
			const uint8_t* pCommand, uint8_t CommandLen,
			uint8_t* pResponse, uint8_t* pResponseLen,
			bool bCoutinue/*=false*/)
{
	if(CommandLen > 252) {
		LOGE("Too large\n");
		return false;
	}

	s_NormalFrmBuf[0] = 0xd4;
	s_NormalFrmBuf[1] = 0x40;			//InDataExchange
	s_NormalFrmBuf[2] = 0x01;			//Tg
	if(bCoutinue) {
		s_NormalFrmBuf[2] |= 0x40;	//MI
	}
	memcpy(s_NormalFrmBuf + 3, pCommand, CommandLen);

	uint16_t res_len;
	bool ret = sendCmd(s_NormalFrmBuf, 3 + CommandLen, s_ResponseBuf, &res_len);
	if(!ret || (res_len < RESHEAD_LEN+1) || (s_ResponseBuf[POS_RESDATA] != 0x00)) {
		LOGE("inDataExchange ret=%d / len=%d / code=%02x\n", ret, res_len, s_ResponseBuf[POS_RESDATA]);
		return false;
	}

	*pResponseLen = res_len - (RESHEAD_LEN+1);
	memcpy(pResponse, s_ResponseBuf + RESHEAD_LEN+1, *pResponseLen);

	return true;
}


/**
 * InCommunicateThru
 *
 * @param[in]	pCommand		送信するコマンド
 * @param[in]	CommandLen		pCommandの長さ
 * @param[out]	pResponse		レスポンス
 * @param[out]	pResponseLen	pResponseの長さ
 *
 * @retval		true			成功
 * @retval		false			失敗
 */
bool NfcPcd::inCommunicateThru(
			const uint8_t* pCommand, uint8_t CommandLen,
			uint8_t* pResponse, uint8_t* pResponseLen)
{
	s_NormalFrmBuf[0] = 0xd4;
	s_NormalFrmBuf[1] = 0x42;			//InCommunicateThru
	memcpy(s_NormalFrmBuf + 2, pCommand, CommandLen);

	uint16_t res_len;
	bool ret = sendCmd(s_NormalFrmBuf, 2 + CommandLen, s_ResponseBuf, &res_len);
//	for(int i=0; i<res_len; i++) {
//		LOGD("%02x \n", s_ResponseBuf[i]);
//	}
	if(!ret || (res_len < RESHEAD_LEN+1) || (s_ResponseBuf[POS_RESDATA] != 0x00)) {
		LOGE("InCommunicateThru ret=%d\n", ret);
		return false;
	}

	*pResponseLen = res_len - (RESHEAD_LEN+1);
	memcpy(pResponse, s_ResponseBuf + RESHEAD_LEN+1, *pResponseLen);

	return true;
}


/**
 * InRelease
 *
 * DEP用。Targetの解放
 *
 * @retval		true			成功
 * @retval		false			失敗
 */
bool NfcPcd::inRelease()
{
	s_NormalFrmBuf[0] = 0xd4;
	s_NormalFrmBuf[1] = 0x52;			//InRelease
	s_NormalFrmBuf[2] = 0x00;

	uint16_t res_len;
	bool ret = sendCmd(s_NormalFrmBuf, 3, s_ResponseBuf, &res_len);
	if(!ret || (res_len != RESHEAD_LEN+1) || (s_ResponseBuf[POS_RESDATA] != 0x00)) {
		LOGE("inRelease ret=%d / len=%d / code=%02x\n", ret, res_len, s_ResponseBuf[POS_RESDATA]);
		return false;
	}

	return true;
}


/////////////////////////////////////////////////////////////////////////
// Target Command
/////////////////////////////////////////////////////////////////////////


/**
 * TgInitAsTarget
 *
 * ターゲットとして起動する.
 * 応答はTgResponseToInitiatorで返す.
 *
 * [in]		pParam			ターゲットパラメータ
 * [out]	pResponse		レスポンスバッファ(Activated情報以下を返す)
 * [out]	pResponseLen	レスポンス長
 * 
 * @retval		true			成功
 * @retval		false			失敗
 */
bool NfcPcd::tgInitAsTarget(
			const TargetParam* pParam,
			uint8_t* pResponse/*=0*/, uint8_t* pResponseLen/*=0*/)
{
	//LOGD("%s", __PRETTY_FUNCTION__);

	uint16_t len = 0;

	s_NormalFrmBuf[len++] = 0xd4;
	s_NormalFrmBuf[len++] = 0x8c;				//TgInitAsTarget

	//mode
	s_NormalFrmBuf[len++] = 0x00;

	//MIFARE
	//SENS_RES
	s_NormalFrmBuf[len++] = 0x00;
	s_NormalFrmBuf[len++] = 0x04;		//0x00はだめ
	//NFCID1
#if 0
	if(pParam->pUId) {
		memcpy(s_NormalFrmBuf + len, pParam->pUId, 3);
		len += 3;
	} else {
		s_NormalFrmBuf[len++] = std::rand() & 0xff;
		s_NormalFrmBuf[len++] = std::rand() & 0xff;
		s_NormalFrmBuf[len++] = std::rand() & 0xff;
	}
#else
	s_NormalFrmBuf[len++] = std::rand() & 0xff;
	s_NormalFrmBuf[len++] = std::rand() & 0xff;
	s_NormalFrmBuf[len++] = std::rand() & 0xff;
#endif
	//SEL_RES
	s_NormalFrmBuf[len++] = 0x40;		//固定

	//FeliCaParams
	//IDm
	uint8_t idm_pos = len;
#if 0
	if(pParam->pIDm) {
		memcpy(s_NormalFrmBuf + len, pParam->pIDm, NFCID2_LEN);
	} else {
		//自動生成
		s_NormalFrmBuf[len++] = 0x01;
		s_NormalFrmBuf[len++] = 0xfe;
		s_NormalFrmBuf[len++] = std::rand() & 0xff;
		s_NormalFrmBuf[len++] = std::rand() & 0xff;
		s_NormalFrmBuf[len++] = std::rand() & 0xff;
		s_NormalFrmBuf[len++] = std::rand() & 0xff;
		s_NormalFrmBuf[len++] = std::rand() & 0xff;
		s_NormalFrmBuf[len++] = std::rand() & 0xff;
	}
#else
	//自動生成
	s_NormalFrmBuf[len++] = 0x01;
	s_NormalFrmBuf[len++] = 0xfe;
	s_NormalFrmBuf[len++] = std::rand() & 0xff;
	s_NormalFrmBuf[len++] = std::rand() & 0xff;
	s_NormalFrmBuf[len++] = std::rand() & 0xff;
	s_NormalFrmBuf[len++] = std::rand() & 0xff;
	s_NormalFrmBuf[len++] = std::rand() & 0xff;
	s_NormalFrmBuf[len++] = std::rand() & 0xff;
#endif
	len += NFCID2_LEN;
	//PMm
	memset(s_NormalFrmBuf + len, 0xff, 8);
	len += 8;
	//SystemCode
#if 0
	s_NormalFrmBuf[len++] = h16(pParam->SystemCode);
	s_NormalFrmBuf[len++] = l16(pParam->SystemCode);
#else
	s_NormalFrmBuf[len++] = 0xff;
	s_NormalFrmBuf[len++] = 0xff;
#endif

	//NFCID3t
	memcpy(s_NormalFrmBuf + len, s_NormalFrmBuf + idm_pos, NFCID2_LEN);
	s_NormalFrmBuf[len++] = 0x00;
	s_NormalFrmBuf[len++] = 0x00;

	//General Bytes
	if(pParam->GtLen) {
		s_NormalFrmBuf[len++] = pParam->GtLen;	//Gt
		memcpy(s_NormalFrmBuf + len, pParam->pGt, pParam->GtLen);
		len += pParam->GtLen;
	}


//	for(int i=0; i<len; i++) {
//		LOGD("%02x ", s_NormalFrmBuf[i]);
//	}

	uint16_t res_len;
	bool ret = sendCmd(s_NormalFrmBuf, len, s_ResponseBuf, &res_len);
	if(!ret || (res_len < RESHEAD_LEN+1)) {
		LOGE("TgInitAsTarget ret=%d/len=%d\n", ret, res_len);
		return ret;
	}

//	uint8_t mode = s_ResponseBuf[POS_RESDATA];
//	LOGD("Mode : %02x \n", mode);
//	if(pResponse && pResponseLen) {
//		*pResponseLen = res_len - 3;
//		memcpy(pResponse, &(s_ResponseBuf[POS_RESDATA+1]), *pResponseLen);
//	}
//	for(int i=0; i<res_len; i++) {
//		LOGD("%02x \n", s_ResponseBuf[i]);
//	}

	if(pResponse && pResponseLen) {
		//Activated情報以下
		*pResponseLen = (uint8_t)(res_len - 4);
		memcpy(pResponse, s_ResponseBuf + 2, *pResponseLen);
	}

	return true;
}


bool NfcPcd::tgSetGeneralBytes(const TargetParam* pParam)
{
	if(pParam->GtLen > 48) {
		LOGE("Too large\n");
		return false;
	}

	s_NormalFrmBuf[0] = 0xd4;
	s_NormalFrmBuf[1] = 0x92;				//TgSetGeneralBytes
	if(pParam->GtLen) {
		memcpy(s_NormalFrmBuf + 2, pParam->pGt, pParam->GtLen);
	}

	uint16_t res_len;
	bool ret = sendCmd(s_NormalFrmBuf, 2 + pParam->GtLen, s_ResponseBuf, &res_len);
	if(!ret || (res_len < RESHEAD_LEN+1) || (s_ResponseBuf[POS_RESDATA] != 0x00)) {
		LOGE("tgSetGeneralBytes ret=%d / len=%d / code=%02x\n", ret, res_len, s_ResponseBuf[POS_RESDATA]);
		return false;
	}

	return true;
}


/**
 * TgResponseToInitiator
 *
 * TgInitAsTargetおよびTgGetInitiatorCommandのレスポンスに対するカードとしての応答
 *
 * @param[in]	pData			応答データ
 * @param[in]	DataLen			pDataの長さ
 * @param[out]	pResponse		レスポンス
 * @param[out]	pResponseLen	pResponseの長さ
 *
 * @retval		true			成功
 * @retval		false			失敗
 */
bool NfcPcd::tgResponseToInitiator(
			const uint8_t* pData, uint8_t DataLen,
			uint8_t* pResponse/*=0*/, uint8_t* pResponseLen/*=0*/)
{
	//LOGD("%s", __PRETTY_FUNCTION__);

	uint16_t len = 0;

	s_NormalFrmBuf[len++] = 0xd4;
	s_NormalFrmBuf[len++] = 0x90;				//TgResponseToInitiator

	//応答
//ここは練り直した方がいい
	s_NormalFrmBuf[len++] = 0xd5;
	memcpy(&(s_NormalFrmBuf[len]), pData, DataLen);
	len += DataLen;

	uint16_t res_len;
	bool ret = sendCmd(s_NormalFrmBuf, len, s_ResponseBuf, &res_len);
	if(!ret || (res_len != 3) || s_ResponseBuf[POS_RESDATA] != 0x00) {
		LOGE("tgResponseToInitiator ret=%d\n", ret);
		return false;
	}

	if(pResponse && pResponseLen) {
		*pResponseLen = uint8_t(res_len - (RESHEAD_LEN+1));
		memcpy(pResponse, &(s_ResponseBuf[POS_RESDATA+1]), *pResponseLen);
	}

	return true;
}


/**
 * TgGetInitiatorCommand
 *
 * Initiatorからのコマンドを取得する。
 * 応答はTgResponseToInitiatorで返す。
 *
 * @param[out]	pResponse		応答データ
 * @param[out]	pResponseLen	pResponseの長さ
 *
 * @retval		true			成功
 * @retval		false			失敗
 */
bool NfcPcd::tgGetInitiatorCommand(uint8_t* pResponse, uint8_t* pResponseLen)
{
	s_NormalFrmBuf[0] = 0xd4;
	s_NormalFrmBuf[1] = 0x88;				//TgGetInitiatorCommand

	uint16_t res_len;
	bool ret = sendCmd(s_NormalFrmBuf, 2, s_ResponseBuf, &res_len);
	if(!ret || (res_len < RESHEAD_LEN+1) || (s_ResponseBuf[POS_RESDATA] != 0x00)) {
		LOGE("tgGetInitiatorCommand ret=%d / len=%d / code=%02x\n", ret, res_len, s_ResponseBuf[POS_RESDATA]);
		return false;
	}

	*pResponseLen = res_len - (RESHEAD_LEN+1);
	memcpy(pResponse, s_ResponseBuf + RESHEAD_LEN+1, *pResponseLen);

	return true;
}


/**
 * TgGetData
 *
 * DEP用。Initiatorからのデータを取得する。
 *
 * @param[out]	pResponse		応答データ
 * @param[out]	pResponseLen	pResponseの長さ
 *
 * @retval		true			成功
 * @retval		false			失敗
 */
bool NfcPcd::tgGetData(uint8_t* pResponse, uint8_t* pResponseLen)
{
	s_NormalFrmBuf[0] = 0xd4;
	s_NormalFrmBuf[1] = 0x86;				//TgGetData

	uint16_t res_len;
	bool ret = sendCmd(s_NormalFrmBuf, 2, s_ResponseBuf, &res_len);
	if(!ret || (res_len < RESHEAD_LEN+1) || (s_ResponseBuf[POS_RESDATA] != 0x00)) {
		LOGE("tgGetData ret=%d / len=%d / code=%02x\n", ret, res_len, s_ResponseBuf[POS_RESDATA]);
		return false;
	}

	*pResponseLen = res_len - (RESHEAD_LEN+1);
	memcpy(pResponse, s_ResponseBuf + RESHEAD_LEN+1, *pResponseLen);

	return true;
}


/**
 * TgSetData
 *
 * DEP用。Initiatorへデータを返す。
 *
 * @param[in]	pCommand		送信するコマンド
 * @param[in]	CommandLen		pCommandの長さ
 *
 * @retval		true			成功
 * @retval		false			失敗
 */
bool NfcPcd::tgSetData(const uint8_t* pCommand, uint8_t CommandLen)
{
	if(CommandLen > 252) {
		LOGE("Too large\n");
		return false;
	}

	s_NormalFrmBuf[0] = 0xd4;
	s_NormalFrmBuf[1] = 0x8e;			//TgSetData
	memcpy(s_NormalFrmBuf + 2, pCommand, CommandLen);

	uint16_t res_len;
	bool ret = sendCmd(s_NormalFrmBuf, 2 + CommandLen, s_ResponseBuf, &res_len);
	if(!ret || (res_len != RESHEAD_LEN+1) || (s_ResponseBuf[POS_RESDATA] != 0x00)) {
		LOGE("tgSetData ret=%d / len=%d / code=%02x\n", ret, res_len, s_ResponseBuf[POS_RESDATA]);
		return false;
	}

	return true;
}



/////////////////////////////////////////////////////////////////////////
// RC-S620/Sとのやりとり
/////////////////////////////////////////////////////////////////////////


/**
 * パケット送受信
 *
 * @param[in]	pCommand		送信するコマンド
 * @param[in]	CommandLen		pCommandの長さ
 * @param[out]	pResponse		レスポンス(0xd5含む)
 * @param[out]	pResponseLen	pResponseの長さ
 *
 * @retval		true			成功
 * @retval		false			失敗
 */
bool NfcPcd::sendCmd(
			const uint8_t* pCommand, uint16_t CommandLen,
			uint8_t* pResponse, uint16_t* pResponseLen,
			bool bRecv/*=true*/)
{
	//LOGD("CommandLen : %d", CommandLen);
	*pResponseLen = 0;

	if(CommandLen > DATA_MAX) {
		LOGE("too large\n");
		return false;
	}

	uint16_t send_len = 3;
	uint8_t dcs = _calc_dcs(pCommand, CommandLen);

	if(CommandLen <= NORMAL_DATA_MAX) {
		// Normalフレーム
		s_SendBuf[send_len++] = CommandLen;						//[3]
		s_SendBuf[send_len++] = (uint8_t)(0 - s_SendBuf[3]);	//[4]
		if(pCommand != s_SendBuf + POS_NORMALFRM_DATA) {
			memcpy(s_SendBuf + send_len, pCommand, CommandLen);
		}
	} else {
		// Extendedフレーム
		s_SendBuf[send_len++] = 0xff;							//[3]
		s_SendBuf[send_len++] = 0xff;							//[4]
		s_SendBuf[send_len++] = (uint8_t)(CommandLen >> 8);		//[5]
		s_SendBuf[send_len++] = (uint8_t)CommandLen;			//[6]
		s_SendBuf[send_len++] = (uint8_t)(0 - s_SendBuf[5] - s_SendBuf[6]);
		if(pCommand != s_SendBuf + POS_EXTENDFRM_DATA) {
			memcpy(s_SendBuf + send_len, pCommand, CommandLen);
		}
	}
	send_len += CommandLen;
	s_SendBuf[send_len++] = dcs;
	s_SendBuf[send_len++] = 0x00;

#ifdef ENABLE_FRAME_LOG
	LOGD("------------\n");
	for(int i=0; i<send_len; i++) {
		LOGD("[W]%02x \n", s_SendBuf[i]);
	}
	LOGD("------------\n");
#endif

	if(DevAccess::write(s_SendBuf, send_len) != send_len) {
		LOGE("write error.\n");
		return false;
	}

	//ACK受信
	uint8_t res_buf[6];
	uint16_t ret_len = DevAccess::read(res_buf, 6);
	if((ret_len != 6) || (memcmp(res_buf, ACK, sizeof(ACK)) != 0)) {
		LOGE("sendCmd 0: ret=%d\n", ret_len);
#ifdef ENABLE_FRAME_LOG
		LOGD("------------\n");
		for(int i=0; i<ret_len; i++) {
			LOGD("[ack]%02x \n", res_buf[i]);
		}
		LOGD("------------\n");
#endif
		sendAck();
		return false;
	}

	// レスポンス
	return (bRecv) ? recvResp(pResponse, pResponseLen, pCommand[1]) : true;
}


/**
 * レスポンス受信
 *
 * @param[out]	pResponse		レスポンス(0xd5, cmd+1含む)
 * @param[out]	pResponseLen	pResponseの長さ
 * @param[in]	CmdCode			送信コマンド(省略可)
 *
 * @retval		true			成功
 * @retval		false			失敗
 */
bool NfcPcd::recvResp(uint8_t* pResponse, uint16_t* pResponseLen, uint8_t CmdCode/*=0xff*/)
{
	uint8_t res_buf[6];
	uint16_t ret_len = DevAccess::read(res_buf, 5);
	if(ret_len != 5) {
		LOGE("recvResp 1: ret=%d\n", ret_len);
		sendAck();
		return false;
	}

	if ((res_buf[0] != 0x00) || (res_buf[1] != 0x00) || (res_buf[2] != 0xff)) {
		LOGE("recvResp 2\n");
		return false;
	}
	if((res_buf[3] == 0xff) && (res_buf[4] == 0xff)) {
		// extend frame
		ret_len = DevAccess::read(res_buf, 3);
		if((ret_len != 3) || (((res_buf[0] + res_buf[1] + res_buf[2]) & 0xff) != 0)) {
			LOGE("recvResp 3: ret=%d\n", ret_len);
			return false;
		}
		*pResponseLen = (((uint16_t)res_buf[0] << 8) | res_buf[1]);
	} else {
		// normal frame
		if(((res_buf[3] + res_buf[4]) & 0xff) != 0) {
			LOGE("recvResp 4\n");
			return false;
		}
		*pResponseLen = res_buf[3];
	}
	if(*pResponseLen > RWBUF_MAX) {
		LOGE("recvResp 5  len:%d\n", *pResponseLen);
		return false;
	}

	ret_len = DevAccess::read(pResponse, *pResponseLen);

#ifdef ENABLE_FRAME_LOG
	LOGD("------------\n");
	for(int i=0; i<ret_len; i++) {
		LOGD("[R]%02x \n", pResponse[i]);
	}
	LOGD("------------\n");
#endif

	if((ret_len != *pResponseLen) || (pResponse[0] != 0xd5)) {
		if((ret_len == 1) && (pResponse[0] == 0x7f)) {
			LOGE("recvResp 6 : Error Frame\n");
		} else {
			LOGE("recvResp 6 :[%02x] ret_len=%d\n", pResponse[0], ret_len);
		}
		sendAck();
		return false;
	}
	if((CmdCode != 0xff) && (pResponse[1] != CmdCode+1)) {
		LOGE("recvResp 7 : ret=%d\n", pResponse[1]);
		sendAck();
		return false;
	}

	uint8_t dcs = _calc_dcs(pResponse, *pResponseLen);
	ret_len = DevAccess::read(res_buf, 2);
	if((ret_len != 2) || (res_buf[0] != dcs) || (res_buf[1] != 0x00)) {
		LOGE("recvResp 8\n");
		sendAck();
		return false;
	}

	return true;
}


/**
 * ACK送信
 */
void NfcPcd::sendAck()
{
	LOGD("sendAck\n");
	DevAccess::write(ACK, sizeof(ACK));

	// wait 1ms
	msleep(1);
}
