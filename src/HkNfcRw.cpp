/** @file	qnfcrw.cpp
 *	@brief	NFCアクセスのインターフェース実装.
 * 	@author	uenokuma@gmail.com
 */
#include "HkNfcRw.h"
#include "HkNfcA.h"
#include "HkNfcB.h"
#include "HkNfcF.h"
#include "NfcPcd.h"
#include "misc.h"

// ログ用
#define LOG_TAG "HkNfcRw"
#include "nfclog.h"

using namespace HkNfcRwMisc;

HkNfcRw::Type	HkNfcRw::m_Type = HkNfcRw::NFC_NONE;				///< アクティブなNFCタイプ
uint8_t		HkNfcRw::s_CommandBuf[HkNfcRw::CARD_COMMAND_LEN];		///< PCDへの送信バッファ
uint8_t		HkNfcRw::s_ResponseBuf[HkNfcRw::CARD_RESPONSE_LEN];	///< PCDからの受信バッファ
uint8_t		HkNfcRw::m_NfcId[HkNfcRw::MAX_NFCID_LEN];		///< 取得したNFCID
uint8_t		HkNfcRw::m_NfcIdLen;					///< 取得済みのNFCID長。0の場合は未取得。



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

	if(!NfcPcd::portOpen()) {
		return false;
	}

	bool ret = NfcPcd::init();
	if(!ret) {
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
	if(NfcPcd::isOpened()) {
		NfcPcd::reset();
		NfcPcd::rfOff();
		NfcPcd::portClose();
	}
}


/**
 * 選択しているカードを内部的に解放する。
 *
 * @attention	RLS_REQ/RLS_RESとは関係ない
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
 */
HkNfcRw::Type HkNfcRw::detect(bool bNfcA, bool bNfcB, bool bNfcF)
{
	m_Type = NFC_NONE;

	if(bNfcF) {
        bool ret = HkNfcF::polling();
		if(ret) {
			LOGD("PollingF");
			m_Type = NFC_F;
			return m_Type;
		}
	}

	if(bNfcA) {
        bool ret = HkNfcA::polling();
		if(ret) {
			LOGD("PollingA");
			m_Type = NFC_A;
			return m_Type;
		}
	}

	if(bNfcB) {
        bool ret = HkNfcB::polling();
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
