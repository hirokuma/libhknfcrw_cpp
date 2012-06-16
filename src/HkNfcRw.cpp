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

#if 0
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
#endif

#if 0
		uint8_t res_len;
		ret = m_pNfcPcd->diagnose(0x00, 0, 0, s_ResponseBuf, &res_len);
		if(ret) {
			LOGD("res_len = %d\n", res_len);
		}
		ret = true;		//気にしない
#endif

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
