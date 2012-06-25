#include <cstring>
#include <cstdlib>

#include "HkNfcLlcpT.h"
#include "NfcPcd.h"
#include "misc.h"

#define LOG_TAG "HkNfcLlcpT"
#include "nfclog.h"


using namespace HkNfcRwMisc;


/**
 * LLCP(Target)開始.
 * 呼び出すと、Initiatorから要求が来るまで戻らない.
 * 成功すると、ATR_RESの返信まで終わっている.
 *
 * @retval	true	開始成功
 * @retval	false	開始失敗
 */
bool HkNfcLlcpT::start()
{
	bool ret = HkNfcDep::startAsTarget(true);
	if(ret) {
		LOGD("%s -- success\n", __PRETTY_FUNCTION__);
		
		//PDU受信側
		m_bSend = false;
		m_LlcpStat = LSTAT_NOT_CONNECT;

		startTimer(m_LinkTimeout);
	}
	
	return ret;
}


/**
 * LLCP(Target)停止要求.
 * 次のタイミングでDM PDUを送信する予約をする.
 * 実際の送信は #poll() で行うため、
 *
 * @param
 * @return
 */
bool HkNfcLlcpT::stopRequest()
{
	if(m_LlcpStat != LSTAT_NONE) {
		LOGD("request DM\n");
		m_LlcpStat = LSTAT_DM;
	}
	
	return true;
}


/**
 * LLCP(Target)定期処理.
 * 開始後、やることがない間は #poll() を呼び出し続けること.
 * 基本的には、 #getDepMode() が #DEP_NONE になるまで呼び出し続ける.
 */
void HkNfcLlcpT::poll()
{
	if(!m_bSend) {
		//PDU受信側
		uint8_t len;
		bool b = recvAsTarget(NfcPcd::responseBuf(), &len);
		if(isTimeout()) {
			//相手から通信が返ってこない
			LOGE("Link timeout\n");
			m_bSend = true;
			m_LlcpStat = LSTAT_DISC;
			m_DSAP = SAP_MNG;
			m_SSAP = SAP_MNG;
		} else if(b) {
			PduType type;
			uint8_t pdu = analyzePdu(NfcPcd::responseBuf(), &type);
			//PDU送信側になる
			m_bSend = true;
		} else {
			//もうだめだろう
			LOGE("recv fail\n");
			killConnection();
		}
	} else {
		//PDU送信側
		uint8_t len;
		switch(m_LlcpStat) {
		case LSTAT_NONE:
			//
			break;
		
		case LSTAT_NOT_CONNECT:
			//CONNECT前はSYMMを投げる
			m_CommandLen = PDU_INFOPOS;
			createPdu(PDU_SYMM);
			LOGD("send SYMM\n");
			break;
		
		case LSTAT_CONNECTING:
			//CONNECT or CCはデータ設定済み
			break;

		case LSTAT_NORMAL:
			//
			if(m_SendLen) {
				//送信データあり
				m_CommandLen = PDU_INFOPOS + m_SendLen;
				createPdu(PDU_I);
				std::memcpy(NfcPcd::commandBuf() + PDU_INFOPOS, m_pSendPtr, m_SendLen);
				m_SendLen = 0;
			} else {
				m_CommandLen = PDU_INFOPOS;
				createPdu(PDU_RR);
			}
			break;

		case LSTAT_DISC:
			//切断シーケンス
			LOGD("send DISC\n");
			m_CommandLen = PDU_INFOPOS;
			createPdu(PDU_DISC);
			break;
		
		case LSTAT_DM:
			//切断
			LOGD("send DM\n");
			NfcPcd::commandBuf(PDU_INFOPOS) = NfcPcd::commandBuf(0);
			m_CommandLen = PDU_INFOPOS + 1;
			createPdu(PDU_DM);
			break;
		}
		if(m_CommandLen) {
			bool b = respAsTarget(NfcPcd::commandBuf(), m_CommandLen);
			if(m_LlcpStat == LSTAT_DM) {
				//DM送信後は強制終了する
				LOGD("DM send\n");
				killConnection();
			} else if(b) {
				//PDU受信側になる
				m_bSend = false;
				m_CommandLen = 0;
				
				startTimer(m_LinkTimeout);
			} else {
				LOGE("error\n");
				if(m_LlcpStat == LSTAT_DM) {
					//もうだめ
					killConnection();
				} else {
					stopRequest();
				}
			}
		} else {
			LOGD("no send data\n");
		}
	}
}
