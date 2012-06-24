#include <cstring>
#include <cstdlib>

#include "HkNfcLlcpI.h"
#include "NfcPcd.h"
#include "misc.h"

#define LOG_TAG "HkNfcLlcpI"
#include "nfclog.h"


using namespace HkNfcRwMisc;


bool HkNfcLlcpI::start(DepMode mode)
{
	bool ret = HkNfcDep::startAsInitiator(mode, true);
	if(ret) {
		//PDU���M��
		m_bSend = true;
		m_LlcpStat = LSTAT_NOT_CONNECT;
	}

}



bool HkNfcLlcpI::stop()
{
	if(m_LlcpStat != LSTAT_NONE) {
		m_LlcpStat = LSTAT_DM;
	}
	
	return true;
}


void HkNfcLlcpI::poll()
{
	if(m_bSend) {
		//PDU���M��
		switch(m_LlcpStat) {
		case LSTAT_NONE:
			//
			break;
		
		case LSTAT_NOT_CONNECT:
			//CONNECT�O��SYMM�𓊂���
			m_CommandLen = PDU_INFOPOS;
			createPdu(PDU_SYMM);
			break;
		
		case LSTAT_CONNECTING:
			//CONNECT or CC�̓f�[�^�ݒ�ς�
			break;

		case LSTAT_NORMAL:
			//
			if(m_SendLen) {
				//���M�f�[�^����
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
			//�ؒf�V�[�P���X
			m_CommandLen = PDU_INFOPOS;
			createPdu(PDU_DISC);
			break;
		
		case LSTAT_DM:
			//�ؒf
			NfcPcd::commandBuf(PDU_INFOPOS) = NfcPcd::commandBuf(0);
			m_CommandLen = PDU_INFOPOS + 1;
			createPdu(PDU_DM);
			break;
		}
		if(m_CommandLen) {
			uint8_t len;
			bool b = sendAsInitiator(NfcPcd::commandBuf(), m_CommandLen, NfcPcd::responseBuf(), &len);
			if(m_LlcpStat == LSTAT_DM) {
				//DM���M��͋����I������
				LOGD("DM send\n");
				stopAsInitiator();
				killConnection();
			} else if(b) {
				//PDU��M���ɂȂ�(��u)
				m_bSend = false;
				m_CommandLen = 0;

				PduType type;
				uint8_t pdu = analyzePdu(NfcPcd::responseBuf(), &type);
				//PDU���M���ɂȂ�
				m_bSend = true;
			} else {
				LOGE("error\n");
				if(m_LlcpStat == LSTAT_DM) {
					//��������
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
