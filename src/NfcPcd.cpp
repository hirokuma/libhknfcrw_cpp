/**
 * @file	NfcPcd.cpp
 * @brief	NFCのPCD(Proximity Coupling Device)アクセス実装.
 * 			RC-S620/S用(今のところ).
 */

#include "NfcPcd.h"
#include "devaccess.h"
#include "misc.h"

// ログ用
#define LOG_TAG "NfcPcd"
#include "nfclog.h"

using namespace HkNfcRwMisc;

/**
 * const
 */
namespace {
	const uint16_t RW_COMMAND_LEN = 265;
	const uint16_t RW_RESPONSE_LEN = 265;
	const uint16_t RESHEAD_LEN = 2;
	
	const int POS_RESDATA = 2;

	const uint8_t MAINCMD = 0xd4;
	const uint8_t ACK[] = { 0x00, 0x00, 0xff, 0x00, 0xff, 0x00 };
}

/**
 * バッファ
 */
namespace {
	uint8_t s_SendBuf[RW_RESPONSE_LEN];

	/// 送信バッファ
	uint8_t* s_CommandBuf = &(s_SendBuf[5]);

	/// 受信バッファ
	uint8_t s_ResponseBuf[RW_RESPONSE_LEN];
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
    memset(m_NfcId3t, 0xcc, HkNfcRw::NFCID3_LEN);
    memset(m_NfcId3i, 0xdd, HkNfcRw::NFCID3_LEN);

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

	// RF通信のT/O
	const uint8_t RFCONFIG1[] = {
		MAINCMD, 0x32,
		0x02,		// T/O
		0x00,		// RFU
		0x00,		// ATR_RES : no timeout
		0x00,		// 非DEP通信時 : no timeout
	};
	ret = sendCmd(RFCONFIG1, sizeof(RFCONFIG1), s_ResponseBuf, &res_len);
	if(!ret || (res_len != RESHEAD_LEN)) {
		LOGE("d4 32 02");
		return false;
	}

	// Target捕捉時のRF通信リトライ回数
	const uint8_t RFCONFIG2[] = {
		MAINCMD, 0x32,
		0x05,		// Retry
		0x00,		// ATR_REQ/RES : only once
		0x00,		// PSL_REQ/RES : only once
		0x00,		// InListPassiveTarget : only once
	};
	ret = sendCmd(RFCONFIG2, sizeof(RFCONFIG2), s_ResponseBuf, &res_len);
	if(!ret || (res_len != RESHEAD_LEN)) {
		LOGE("d4 32 05(%d)%d", ret, res_len);
		return false;
	}

	// RF出力ONからTargetID取得コマンド送信までの追加ウェイト時間
	const uint8_t RFCONFIG3[] = {
		MAINCMD, 0x32,
		0x81,		// wait
		0xb7,		// ?
	};
	ret = sendCmd(RFCONFIG3, sizeof(RFCONFIG3), s_ResponseBuf, &res_len);
	if(!ret || (res_len != RESHEAD_LEN)) {
		LOGE("d4 32 81(%d)%d", ret, res_len);
		return false;
	}

	ret = reset();
	if(!ret) {
		return false;
	}

	// nfcpyのまね1
#if 0
	const uint8_t RFCONFIG4[] = {
		MAINCMD, 0x32,
		0x82,		// ?
		0x08, 0x01, 0x08		// ?
	};
	ret = sendCmd(RFCONFIG4, sizeof(RFCONFIG4), s_ResponseBuf, &res_len);
	if(!ret || (res_len != RESHEAD_LEN)) {
		LOGE("d4 32 82(%d)%d", ret, res_len);
		//return false;
	}
#endif

	// nfcpyのまね2
	const uint8_t RFCONFIG5[] = {
		MAINCMD, 0x08,
		0x63, 0x0d, 0x00,
	};
	ret = sendCmd(RFCONFIG5, sizeof(RFCONFIG5), s_ResponseBuf, &res_len);
	if(!ret || res_len < RESHEAD_LEN + 1) {
		LOGE("WriteRegister(%d) / %d", ret, res_len);
		//return false;
	}

	// nfcpyのまね3
	const uint8_t RFCONFIG6[] = {
		MAINCMD, 0x06,
		0xa0, 0x1b,		// 0xa01b～0xa022(8byte)
		0xa0, 0x1c,
		0xa0, 0x1d,
		0xa0, 0x1e,
		0xa0, 0x1f,
		0xa0, 0x20,
		0xa0, 0x21,
		0xa0, 0x22,
	};
	const uint8_t READREG_LEN = 8;
	ret = sendCmd(RFCONFIG6, sizeof(RFCONFIG6), s_ResponseBuf, &res_len);
	if(!ret || res_len < RESHEAD_LEN + READREG_LEN) {
		LOGE("ReadRegister(%d) / %d", ret, res_len);
		//return false;
		return true;
	}

	// nfcpyのまね4
	s_CommandBuf[0] = MAINCMD;
	s_CommandBuf[1] = 0x32;		//RFConfiguration
	s_CommandBuf[2] = 0x0b;
	memcpy(s_CommandBuf + 3, s_ResponseBuf + POS_RESDATA, READREG_LEN);
	ret = sendCmd(s_CommandBuf, 3+READREG_LEN, s_ResponseBuf, &res_len);
	if(!ret || res_len != RESHEAD_LEN) {
		LOGE("32 0b(%d) / %d", ret, res_len);
		//return false;
	}

	// OFFにしておこう
	ret = rfOff();

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
	const uint8_t RFCONFIG_RFOFF[] = {
		MAINCMD, 0x32,
		0x01,		// RF field
		0x00,		// bit1 : Auto RFCA : OFF
					// bit0 : RF ON/OFF : OFF
	};
	bool ret = sendCmd(RFCONFIG_RFOFF, sizeof(RFCONFIG_RFOFF),
					s_ResponseBuf, &res_len);
	if(!ret || (res_len != RESHEAD_LEN)) {
		LOGE("rfOff ret=%d", ret);
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

	s_CommandBuf[0] = MAINCMD;
	s_CommandBuf[1] = 0x32;		//RFConfiguration
	s_CommandBuf[2] = cmd;
	memcpy(s_CommandBuf + 3, pCommand, CommandLen);

	uint16_t res_len;
	bool ret = sendCmd(s_CommandBuf, 3 + CommandLen, s_ResponseBuf, &res_len);
	if(!ret || (res_len != RESHEAD_LEN)) {
		LOGE("rfConfiguration ret=%d", ret);
		return false;
	}

	return true;
}


/**
 * [RC-S620/S]Reset
 *
 * @retval		true			成功
 * @retval		false			失敗
 */
bool NfcPcd::reset()
{
	//LOGD("%s", __PRETTY_FUNCTION__);

	const uint8_t RESET[] = { MAINCMD, 0x18, 0x01 };
	uint16_t res_len;
	bool ret = sendCmd(RESET, sizeof(RESET), s_ResponseBuf, &res_len);
	if(!ret || (res_len != RESHEAD_LEN)) {
		LOGE("reset ret=%d", ret);
		return false;
	}
	
	// execute
	sendAck();
	msleep(10);

	return true;
}


/**
 * [RC-S620/S]Diagnose
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

	s_CommandBuf[0] = MAINCMD;
	s_CommandBuf[1] = 0x00;			//Diagnose
	s_CommandBuf[2] = cmd;
	memcpy(s_CommandBuf + 3, pCommand, CommandLen);
	
	uint16_t res_len;
	bool ret = sendCmd(s_CommandBuf, 3 + CommandLen, s_ResponseBuf, &res_len);
	if(!ret || (res_len < RESHEAD_LEN)) {
		LOGE("diagnose ret=%d/%d", ret, res_len);
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

	s_CommandBuf[0] = MAINCMD;
	s_CommandBuf[1] = 0x12;
	s_CommandBuf[2] = val;

	uint16_t res_len;
	bool ret = sendCmd(s_CommandBuf, 3, s_ResponseBuf, &res_len);
	if(!ret || (res_len != RESHEAD_LEN)) {
		LOGE("setParam ret=%d", ret);
	}

	return true;
}


/**
 * [RC-S620/S]GetFirmwareVersion
 *
 * @param[out]	pResponse		レスポンス(GF_XXでアクセス)
 * @retval		true			成功
 * @retval		false			失敗
 */
bool NfcPcd::getFirmwareVersion(uint8_t* pResponse)
{
	LOGD("%s", __PRETTY_FUNCTION__);

	const uint16_t RES_LEN = 4;
	s_CommandBuf[0] = MAINCMD;
	s_CommandBuf[1] = 0x02;

	uint16_t res_len;
	bool ret = sendCmd(s_CommandBuf, 2, s_ResponseBuf, &res_len);
	if(!ret || (res_len < RESHEAD_LEN + RES_LEN)) {
		LOGE("getFirmware ret=%d", ret);
		return false;
	}
	
	memcpy(pResponse, s_ResponseBuf + RESHEAD_LEN, RES_LEN);

	return true;
}


/**
 * [RC-S620/S]GetGeneralStatus
 *
 * @param[out]	pResponse		レスポンス(GGS_XXでアクセス)
 * @retval		true			成功
 * @retval		false			失敗
 */
bool NfcPcd::getGeneralStatus(uint8_t* pResponse)
{
	LOGD("%s", __PRETTY_FUNCTION__);

	s_CommandBuf[0] = MAINCMD;
	s_CommandBuf[1] = 0x04;

	const uint16_t RES_LEN = 5;
	uint16_t res_len;
	bool ret = sendCmd(s_CommandBuf, 2, s_ResponseBuf, &res_len);
	if(!ret || (res_len < RESHEAD_LEN + RES_LEN)) {
		LOGE("getGeneralStatus ret=%d/%d", ret, res_len);
		return false;
	}
	
	memcpy(pResponse, s_ResponseBuf + RESHEAD_LEN, RES_LEN);

	return true;
}


/**
 * [RC-S620/S]CommunicateThruEX
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

	s_CommandBuf[0] = MAINCMD;
	s_CommandBuf[1] = 0xa0;		//CommunicateThruEX
	memcpy(s_CommandBuf + 2, pCommand, CommandLen);

	uint16_t res_len;
	bool ret = sendCmd(s_CommandBuf, 2 + CommandLen, s_ResponseBuf, &res_len);
	if(!ret || (res_len < RESHEAD_LEN+1)) {
		LOGE("communicateThruEx ret=%d", ret);
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
 * [RC-S620/S]CommunicateThruEX
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

	s_CommandBuf[0] = MAINCMD;
	s_CommandBuf[1] = 0xa0;		//CommunicateThruEX
	s_CommandBuf[2] = l16(Timeout);
	s_CommandBuf[3] = h16(Timeout);
	if(CommandLen) {
		s_CommandBuf[4] = (uint8_t)(CommandLen + 1);
		memcpy(s_CommandBuf + 5, pCommand, CommandLen);
		CommandLen += 5;
	} else {
		CommandLen = 4;
	}

	uint16_t res_len;
	bool ret = sendCmd(s_CommandBuf, CommandLen, s_ResponseBuf, &res_len);
	if(!ret || (res_len < RESHEAD_LEN+1)) {
		LOGE("communicateThruEx ret=%d", ret);
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
 * @param[in]	Ap			Active/Passive
 * @param[in]	Br			通信速度
 * @param[in]	bNfcId3		NFCID3を使用するかどうか
 * @param[in]	pGt			Gt(Initiator)
 * @param[in]	GtLen		Gtサイズ
 */
bool NfcPcd::_inJump(uint8_t Cmd,
		ActPass Ap, BaudRate Br, bool bNfcId3,
		const uint8_t* pGt, uint8_t GtLen)
{
	//LOGD("%s", __PRETTY_FUNCTION__);

	s_CommandBuf[0] = MAINCMD;
	s_CommandBuf[1] = Cmd;
	s_CommandBuf[2] = (uint8_t)Ap;
	s_CommandBuf[3] = (uint8_t)Br;
	s_CommandBuf[4] = 0x00;		//Next
	uint8_t len = 5;
	if(Ap == AP_PASSIVE) {
		s_CommandBuf[4] |= 0x01;
		if(Br == BR_106K) {
			const uint8_t known_id[4] = { 0x08, 0x01, 0x02, 0x03 };
			memcpy(&(s_CommandBuf[len]), known_id, sizeof(known_id));
			len += sizeof(known_id);
		} else {
			const uint8_t pol_req[5] = { 0x00, 0xff, 0xff, 0x01, 0x00 };
			memcpy(&(s_CommandBuf[len]), pol_req, sizeof(pol_req));
			len += sizeof(pol_req);
		}
	}
	if(bNfcId3) {
		s_CommandBuf[4] |= 0x02;
		memcpy(&(s_CommandBuf[len]), m_NfcId3i, HkNfcRw::NFCID3_LEN);
		len += HkNfcRw::NFCID3_LEN;
	}
	if(pGt && GtLen) {
		s_CommandBuf[4] |= 0x04;
		memcpy(&(s_CommandBuf[len]), pGt, GtLen);
		len += GtLen;
	}

//	for(int i=0; i<len; i++) {
//		LOGD("%02x ", s_CommandBuf[i]);
//	}

	uint16_t res_len;
	bool ret = sendCmd(s_CommandBuf, len, s_ResponseBuf, &res_len);

//	for(int i=0; i<res_len; i++) {
//		LOGD("%02x ", s_ResponseBuf[i]);
//	}

	if(!ret || (res_len < RESHEAD_LEN+17)) {
		LOGE("inJumpForDep ret=%d/len=%d", ret, res_len);
		return false;
	}

	return true;
}


/**
 * InJumpForDEP
 *
 * @param[in]	Ap			Active/Passive
 * @param[in]	Br			通信速度
 * @param[in]	bNfcId3		NFCID3を使用するかどうか
 * @param[in]	pGt			Gt(Initiator)
 * @param[in]	GtLen		Gtサイズ
 */
bool NfcPcd::inJumpForDep(
		ActPass Ap, BaudRate Br, bool bNfcId3,
		const uint8_t* pGt, uint8_t GtLen)
{
	LOGD("%s", __PRETTY_FUNCTION__);
	return _inJump(0x56, Ap, Br, bNfcId3, pGt, GtLen);
}


/**
 * InJumpForPSL
 *
 * @param[in]	Ap			Active/Passive
 * @param[in]	Br			通信速度
 * @param[in]	bNfcId3		NFCID3を使用するかどうか
 * @param[in]	pGt			Gt(Initiator)
 * @param[in]	GtLen		Gtサイズ
 */
bool NfcPcd::inJumpForPsl(
		ActPass Ap, BaudRate Br, bool bNfcId3,
		const uint8_t* pGt, uint8_t GtLen)
{
	//LOGD("%s", __PRETTY_FUNCTION__);
	return _inJump(0x46, Ap, Br, bNfcId3, pGt, GtLen);
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
	s_CommandBuf[0] = MAINCMD;
	s_CommandBuf[1] = 0x4a;				//InListPassiveTarget
	s_CommandBuf[2] = 0x01;
	memcpy(s_CommandBuf + 3, pInitData, InitLen);

	bool ret = sendCmd(s_CommandBuf, 3+InitLen, s_ResponseBuf, &responseLen);
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
 *
 * @retval		true			成功
 * @retval		false			失敗
 */
bool NfcPcd::inDataExchange(
			const uint8_t* pCommand, uint8_t CommandLen,
			uint8_t* pResponse, uint8_t* pResponseLen,
			bool bCoutinue/*=false*/)
{
	s_CommandBuf[0] = MAINCMD;
	s_CommandBuf[1] = 0x40;			//InDataExchange
	s_CommandBuf[2] = 0x01;			//Tg
	if(bCoutinue) {
		s_CommandBuf[2] |= 0x40;	//MI
	}
	memcpy(s_CommandBuf + 3, pCommand, CommandLen);

	uint16_t res_len;
	bool ret = sendCmd(s_CommandBuf, 3 + CommandLen, s_ResponseBuf, &res_len);
	if(!ret || (res_len < RESHEAD_LEN+1) || (s_ResponseBuf[POS_RESDATA] != 0x00)) {
		LOGE("inDataExchange ret=%d / len=%d / code=%02x", ret, res_len, s_ResponseBuf[POS_RESDATA]);
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
	s_CommandBuf[0] = MAINCMD;
	s_CommandBuf[1] = 0x42;			//InCommunicateThru
	memcpy(s_CommandBuf + 2, pCommand, CommandLen);

	uint16_t res_len;
	bool ret = sendCmd(s_CommandBuf, 2 + CommandLen, s_ResponseBuf, &res_len);
	for(int i=0; i<res_len; i++) {
		LOGD("%02x ", s_ResponseBuf[i]);
	}
	if(!ret || (res_len < RESHEAD_LEN+1) || (s_ResponseBuf[POS_RESDATA] != 0x00)) {
		LOGE("InCommunicateThru ret=%d", ret);
		return false;
	}

	*pResponseLen = res_len - (RESHEAD_LEN+1);
	memcpy(pResponse, s_ResponseBuf + RESHEAD_LEN+1, *pResponseLen);

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
 * @param[in]	SystemCode		システムコード
 * @param[in]	pGt				Gt(Target)
 * @param[in]	GtLen			Gtの長さ
 *
 * @retval		true			成功
 * @retval		false			失敗
 */
bool NfcPcd::tgInitAsTarget(
			uint16_t SystemCode,
			const uint8_t* pGt, uint8_t GtLen)
{
	LOGD("%s", __PRETTY_FUNCTION__);

	uint16_t len = 0;

	s_CommandBuf[len++] = MAINCMD;
	s_CommandBuf[len++] = 0x8c;				//TgInitAsTarget

	//mode
	s_CommandBuf[len++] = 0x00;

	//MIFARE
	//SENS_RES
	s_CommandBuf[len++] = 0x00;
	s_CommandBuf[len++] = 0x04;		//0x00はだめ
	//NFCID1
	s_CommandBuf[len++] = 0x00;		//自由
	s_CommandBuf[len++] = 0x00;
	s_CommandBuf[len++] = 0x00;
	//SEL_RES
	s_CommandBuf[len++] = 0x40;		//固定

	//FeliCaParams
	//IDm
    memcpy(&(s_CommandBuf[len]), m_NfcId3t, HkNfcRw::NFCID2_LEN);
    len += HkNfcRw::NFCID2_LEN;
	//PMm
	memset(&(s_CommandBuf[len]), 0x00, 8);
	len += 8;
	//SystemCode
	s_CommandBuf[len++] = h16(SystemCode);
	s_CommandBuf[len++] = l16(SystemCode);

	//NFCID3t
	memcpy(&(s_CommandBuf[len]), m_NfcId3t, HkNfcRw::NFCID3_LEN);
	len += HkNfcRw::NFCID3_LEN;
	s_CommandBuf[len++] = 0x00;
	s_CommandBuf[len++] = 0x00;

	s_CommandBuf[len++] = GtLen;	//Gt
	if(GtLen) {
		memcpy(&(s_CommandBuf[len]), pGt, GtLen);
		len += GtLen;
	}
	s_CommandBuf[len++] = 0x00;	//Tk


//	for(int i=0; i<len; i++) {
//		LOGD("%02x ", s_CommandBuf[i]);
//	}

	uint16_t res_len;
	bool ret = sendCmd(s_CommandBuf, len, s_ResponseBuf, &res_len);
	if(!ret || (res_len < RESHEAD_LEN+1)) {
		LOGE("TgInitAsTarget ret=%d/len=%d", ret, res_len);
		return ret;
	}

#ifdef HKNFCRW_ENABLE_DEBUG
	uint8_t mode = s_ResponseBuf[POS_RESDATA];
	LOGD("Mode : %02x ", mode);
	//memcpy(initiatorCmd, &(s_ResponseBuf[POS_RESDATA+1]), res_len - 3);
//	for(int i=0; i<res_len; i++) {
//		LOGD("%02x ", s_ResponseBuf[i]);
//	}
#endif

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
	LOGD("%s", __PRETTY_FUNCTION__);

	uint16_t len = 0;

	s_CommandBuf[len++] = MAINCMD;
	s_CommandBuf[len++] = 0x90;				//TgResponseToInitiator

	//応答
	s_CommandBuf[len++] = 0xd5;
	memcpy(&(s_CommandBuf[len]), pData, DataLen);
	len += DataLen;

	uint16_t res_len;
	bool ret = sendCmd(s_CommandBuf, len, s_ResponseBuf, &res_len);
	if(!ret || (res_len != 3) || s_ResponseBuf[POS_RESDATA] != 0x00) {
		LOGE("tgResponseToInitiator ret=%d", ret);
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
	const uint8_t CMD[] = { MAINCMD, 0x88 };

	uint16_t res_len;
	bool ret = sendCmd(CMD, 2, s_ResponseBuf, &res_len);
	if(!ret || (res_len < RESHEAD_LEN+1) || (s_ResponseBuf[POS_RESDATA] != 0x00)) {
		LOGE("tgGetInitiatorCommand ret=%d / len=%d / code=%02x", ret, res_len, s_ResponseBuf[POS_RESDATA]);
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
	const uint8_t CMD[] = { MAINCMD, 0x86 };

	uint16_t res_len;
	bool ret = sendCmd(CMD, 2, s_ResponseBuf, &res_len);
	if(!ret || (res_len < RESHEAD_LEN+1) || (s_ResponseBuf[POS_RESDATA] != 0x00)) {
		LOGE("tgGetData ret=%d / len=%d / code=%02x", ret, res_len, s_ResponseBuf[POS_RESDATA]);
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
	s_CommandBuf[0] = MAINCMD;
	s_CommandBuf[1] = 0x8e;			//TgSetData
	memcpy(s_CommandBuf + 2, pCommand, CommandLen);

	uint16_t res_len;
	bool ret = sendCmd(s_CommandBuf, 2 + CommandLen, s_ResponseBuf, &res_len);
	if(!ret || (res_len != RESHEAD_LEN+1) || (s_ResponseBuf[POS_RESDATA] != 0x00)) {
		LOGE("tgSetData ret=%d / len=%d / code=%02x", ret, res_len, s_ResponseBuf[POS_RESDATA]);
		return false;
	}

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
	const uint8_t CMD[] = { MAINCMD, 0x52, 0x01 };

	uint16_t res_len;
	bool ret = sendCmd(CMD, 3, s_ResponseBuf, &res_len);
	if(!ret || (res_len != RESHEAD_LEN+1) || (s_ResponseBuf[POS_RESDATA] != 0x00)) {
		LOGE("inRelease ret=%d / len=%d / code=%02x", ret, res_len, s_ResponseBuf[POS_RESDATA]);
		return false;
	}

	return true;
}


/////////////////////////////////////////////////////////////////////////
// RC-S620/Sとのやりとり
/////////////////////////////////////////////////////////////////////////


/**
 * [RC-S620/S]パケット送受信
 *
 * @param[in]	pCommand		送信するコマンド
 * @param[in]	CommandLen		pCommandの長さ
 * @param[out]	pResponse		レスポンス(0xd5, cmd+1含む)
 * @param[out]	pResponseLen	pResponseの長さ
 *
 * @retval		true			成功
 * @retval		false			失敗
 */
bool NfcPcd::sendCmd(
			const uint8_t* pCommand, uint8_t CommandLen,
			uint8_t* pResponse, uint16_t* pResponseLen,
			bool bRecv/*=true*/)
{
	//LOGD("CommandLen : %d", CommandLen);
	*pResponseLen = 0;

#if 0
	if(CommandLen > 255) {
		LOGE("Extended frame not support.");
		return false;
	}
#endif

	uint16_t send_len = 0;

	//パケット送信
	uint8_t dcs = _calc_dcs(pCommand, CommandLen);
	s_SendBuf[3] = CommandLen;
	s_SendBuf[4] = (uint8_t)(0 - s_SendBuf[3]);
	send_len = 5;
	if(pCommand != s_SendBuf + 5) {
		memcpy(s_SendBuf + send_len, pCommand, CommandLen);
	}
	send_len += CommandLen;
	s_SendBuf[send_len++] = dcs;
	s_SendBuf[send_len++] = 0x00;

#if 0
	LOGD("------------");
	for(int i=0; i<send_len; i++) {
		LOGD("[W]%02x ", s_SendBuf[i]);
	}
	LOGD("------------");
#endif

	if(DevAccess::write(s_SendBuf, send_len) != send_len) {
		LOGE("write error.");
		return false;
	}

	//ACK受信
	uint8_t res_buf[6];
	uint16_t ret_len = DevAccess::read(res_buf, 6);
	if((ret_len != 6) || (memcmp(res_buf, ACK, sizeof(ACK)) != 0)) {
		LOGE("sendCmd 0: ret=%d", ret_len);
		sendAck();
		return false;
	}

	// レスポンス
	return (bRecv) ? recvResp(pResponse, pResponseLen, pCommand[1]) : true;
}


/**
 * [RC-S620/S]レスポンス受信
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
		LOGE("recvResp 1: ret=%d", ret_len);
		sendAck();
		return false;
	}

	if ((res_buf[0] != 0x00) || (res_buf[1] != 0x00) || (res_buf[2] != 0xff)) {
		LOGE("recvResp 2");
		return false;
	}
	if((res_buf[3] == 0xff) && (res_buf[4] == 0xff)) {
		// extend frame
		ret_len = DevAccess::read(res_buf, 3);
		if((ret_len != 3) || (((res_buf[0] + res_buf[1] + res_buf[2]) & 0xff) != 0)) {
			LOGE("recvResp 3: ret=%d", ret_len);
			return false;
		}
		*pResponseLen = (((uint16_t)res_buf[0] << 8) | res_buf[1]);
	} else {
		// normal frame
		if(((res_buf[3] + res_buf[4]) & 0xff) != 0) {
			LOGE("recvResp 4");
			return false;
		}
		*pResponseLen = res_buf[3];
	}
	if(*pResponseLen > RW_RESPONSE_LEN) {
		LOGE("recvResp 5  len:%d", *pResponseLen);
		return false;
	}

	ret_len = DevAccess::read(pResponse, *pResponseLen);

#if 0
	LOGD("------------");
	for(int i=0; i<ret_len; i++) {
		LOGD("[R]%02x ", pResponse[i]);
	}
	LOGD("------------");
#endif

	if((ret_len != *pResponseLen) || (pResponse[0] != 0xd5)) {
		if((ret_len == 1) && (pResponse[0] == 0x7f)) {
			LOGE("recvResp 6 : Error Frame");
		} else {
			LOGE("recvResp 6 :[%02x] ret_len=%d", pResponse[0], ret_len);
		}
		sendAck();
		return false;
	}
	if((CmdCode != 0xff) && (pResponse[1] != CmdCode+1)) {
		LOGE("recvResp 7 : ret=%d", pResponse[1]);
		sendAck();
		return false;
	}

	uint8_t dcs = _calc_dcs(pResponse, *pResponseLen);
	ret_len = DevAccess::read(res_buf, 2);
	if((ret_len != 2) || (res_buf[0] != dcs) || (res_buf[1] != 0x00)) {
		LOGE("recvResp 8");
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
	DevAccess::write(ACK, sizeof(ACK));

	// wait 1ms
	msleep(1);
}
