/** @file	qnfcrw.cpp
 *	@brief	NFCアクセスのインターフェース実装.
 * 	@author	uenokuma@gmail.com
 */
#include "HkNfcRw.h"
#include "NfcPcd.h"
#include "misc.h"

// ログ用
#define LOG_TAG "HkNfcRw"
#include "nfclog.h"

using namespace HkNfcRwMisc;


/**
 * インスタンス取得(singleton)
 */
HkNfcRw* HkNfcRw::getInstance()
{
	static HkNfcRw g_HkNfcRw;

	return &g_HkNfcRw;
}


/**
 * コンストラクタ
 */
HkNfcRw::HkNfcRw()
	: m_Type(NFC_NONE)
{
	m_pNfcPcd = NfcPcd::getInstance();
}


/**
 * デストラクタ
 */
HkNfcRw::~HkNfcRw()
{
}


/**
 * NFCデバイスオープン
 *
 * @retval	true		成功
 * @retval	false		失敗
 *
 * @attention	- 初回に必ず呼び出すこと。
 * 				- falseの場合、呼び出し側が#close()すること。
 * 				- 終了時には#close()を呼び出すこと。
 *
 * @note		- 呼び出しただけでは搬送波を出力しない。
 */
bool HkNfcRw::open()
{
	LOGD("%s\n", __PRETTY_FUNCTION__);

	if(!m_pNfcPcd->portOpen()) {
		return false;
	}

	bool ret = m_pNfcPcd->init();
	if(ret) {
		ret = m_pNfcPcd->getFirmwareVersion(s_ResponseBuf);
		if(ret) {
			LOGD("IC:%02x / Ver:%02x / Rev:%02x / Support:%02x\n",
				s_ResponseBuf[NfcPcd::GF_IC], s_ResponseBuf[NfcPcd::GF_VER],
				s_ResponseBuf[NfcPcd::GF_REV], s_ResponseBuf[NfcPcd::GF_SUPPORT]);
		}

		ret = m_pNfcPcd->getGeneralStatus(s_ResponseBuf);
		if(ret) {
			LOGD("Err:%02x / Field:%02x / NbTg:%02x / Tg:%02x / TxMode:%02x\n",
				s_ResponseBuf[NfcPcd::GGS_ERR],
				s_ResponseBuf[NfcPcd::GGS_FIELD],
				s_ResponseBuf[NfcPcd::GGS_NBTG],
				s_ResponseBuf[NfcPcd::GGS_TG],
				s_ResponseBuf[NfcPcd::GGS_TXMODE]);
		}
		ret = true;		//気にしない

#if 0
		uint8_t res_len;
		ret = m_pNfcPcd->diagnose(0x00, 0, 0, s_ResponseBuf, &res_len);
		if(ret) {
			LOGD("res_len = %d\n", res_len);
		}
		ret = true;		//気にしない
#endif
		msleep(1000);

	} else {
		close();
	}
	
	return ret;
}


/**
 * NFCデバイスをクローズする。
 *
 * @attention	- 使用が終わったら、必ず呼び出すこと。
 * 				- 呼び出さない場合、搬送波を出力したままとなる。
 * 				- 呼び出し後、再度使用する場合は#open()を呼び出すこと。
 */
void HkNfcRw::close()
{
	if(m_pNfcPcd->isOpened()) {
		m_pNfcPcd->reset();
		m_pNfcPcd->rfOff();
		m_pNfcPcd->portClose();
	}
}


/**
 * 選択しているカードを内部的に解放する。
 *
 * @attention	たぶん、これではだめ.
 */
void HkNfcRw::release()
{
	m_NfcIdLen = 0;
	memset(m_NfcId, 0x00, sizeof(m_NfcId));
}


/**
 * ターゲットの探索
 *
 * @param[in]	pNfcA	NfcA(null時は探索しない)
 * @param[in]	pNfcB	NfcB(null時は探索しない)
 * @param[in]	pNfcF	NfcF(null時は探索しない)
 *
 * @retval	true		成功
 * @retval	false		失敗
 *
 * @note		- #open()後に呼び出すこと。
 * 				- アクティブなカードが変更された場合に呼び出すこと。
 *
 * @sa			getInstance()
 * 				getInstanceA()
 * 				getInstanceB()
 * 				getInstanceF()
 */
HkNfcRw::Type HkNfcRw::detect(HkNfcA* pNfcA, HkNfcB* pNfcB, HkNfcF* pNfcF)
{
	m_Type = NFC_NONE;

	if(pNfcF) {
        bool ret = pNfcF->polling();
		if(ret) {
			LOGD("PollingF");
			m_Type = NFC_F;
			return m_Type;
		}
	}

	if(pNfcA) {
        bool ret = pNfcA->polling();
		if(ret) {
			LOGD("PollingA");
			m_Type = NFC_A;
			return m_Type;
		}
	}

	if(pNfcB) {
        bool ret = pNfcB->polling();
		if(ret) {
			LOGD("PollingB");
			m_Type = NFC_B;
			return m_Type;
		}
	}
	LOGD("Detect fail");

	return NFC_NONE;
}


void HkNfcRw::debug_nfcid()
{
	LOGD("%s:", __FUNCTION__);
	for(int i=0; i<m_NfcIdLen; i++) {
		LOGD("%02x ", m_NfcId[i]);
	}
}


/**
 * ターゲット起動
 */
bool HkNfcRw::initAsTarget(const uint8_t* id2, uint16_t syscode)
{
	m_pNfcPcd->reset();

	//IDm設定
	m_pNfcPcd->setNfcId3tAsId2(id2);

	//Initiator待ち
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
}


/**
 * イニシエータからのDEP開始
 */
bool HkNfcRw::jumpForDep(const uint8_t* id2)
{
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
}
