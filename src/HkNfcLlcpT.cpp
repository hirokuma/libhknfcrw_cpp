#include <cstring>
#include <cstdlib>

#include "HkNfcLlcpT.h"
#include "NfcPcd.h"
#include "misc.h"

#define LOG_TAG "HkNfcLlcpT"
#include "nfclog.h"


using namespace HkNfcRwMisc;


bool HkNfcLlcpT::start()
{
	bool ret = HkNfcDep::startAsTarget(true);
	if(ret) {
		//PDU受信側
		m_bSend = false;
		m_LlcpStat = LSTAT_NOT_CONNECT;
	}
}


bool HkNfcLlcpT::stop()
{
	if(m_LlcpStat != LSTAT_NONE) {
		m_LlcpStat = LSTAT_DM;
	}
	
	return true;
}


void HkNfcLlcpT::poll()
{
	if(!m_bSend) {
		//PDU受信側
		uint8_t len;
		bool b = recvAsTarget(NfcPcd::responseBuf(), &len);
		if(b) {
			PduType type;
			uint8_t pdu = analyzePdu(NfcPcd::responseBuf(), &type);
			//PDU送信側になる
			m_bSend = true;
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
			m_CommandLen = PDU_INFOPOS;
			createPdu(PDU_DISC);
			break;
		
		case LSTAT_DM:
			//切断
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
			} else {
				LOGE("error\n");
				if(m_LlcpStat == LSTAT_DM) {
					//もうだめ
					killConnection();
				} else {
					stop();
				}
			}
		} else {
			LOGD("no send data\n");
		}
	}
}
