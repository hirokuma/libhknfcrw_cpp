#include <cstring>
#include <cstdlib>

#include "HkNfcLlcpT.h"
#include "NfcPcd.h"
#include "misc.h"

#define LOG_TAG "HkNfcLlcpT"
#include "nfclog.h"


using namespace HkNfcRwMisc;


/**
 * LLCP(Target)�J�n.
 * �Ăяo���ƁAInitiator����v��������܂Ŗ߂�Ȃ�.
 * ��������ƁAATR_RES�̕ԐM�܂ŏI����Ă���.
 *
 * @retval	true	�J�n����
 * @retval	false	�J�n���s
 */
bool HkNfcLlcpT::start()
{
	bool ret = HkNfcDep::startAsTarget(true);
	if(ret) {
		LOGD("%s -- success\n", __PRETTY_FUNCTION__);
		
		//PDU��M��
		m_bSend = false;
		m_LlcpStat = LSTAT_NOT_CONNECT;

		startTimer(m_LinkTimeout);
	}
	
	return ret;
}


/**
 * LLCP(Target)��~�v��.
 * ���̃^�C�~���O��DM PDU�𑗐M����\�������.
 * ���ۂ̑��M�� #poll() �ōs�����߁A
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
 * LLCP(Target)�������.
 * �J�n��A��邱�Ƃ��Ȃ��Ԃ� #poll() ���Ăяo�������邱��.
 * ��{�I�ɂ́A #getDepMode() �� #DEP_NONE �ɂȂ�܂ŌĂяo��������.
 */
void HkNfcLlcpT::poll()
{
	if(!m_bSend) {
		//PDU��M��
		uint8_t len;
		bool b = recvAsTarget(NfcPcd::responseBuf(), &len);
		if(isTimeout()) {
			//���肩��ʐM���Ԃ��Ă��Ȃ�
			LOGE("Link timeout\n");
			m_bSend = true;
			m_LlcpStat = LSTAT_DISC;
			m_DSAP = SAP_MNG;
			m_SSAP = SAP_MNG;
		} else if(b) {
			PduType type;
			uint8_t pdu = analyzePdu(NfcPcd::responseBuf(), &type);
			//PDU���M���ɂȂ�
			m_bSend = true;
		} else {
			//�������߂��낤
			LOGE("recv fail\n");
			killConnection();
		}
	} else {
		//PDU���M��
		uint8_t len;
		switch(m_LlcpStat) {
		case LSTAT_NONE:
			//
			break;
		
		case LSTAT_NOT_CONNECT:
			//CONNECT�O��SYMM�𓊂���
			m_CommandLen = PDU_INFOPOS;
			createPdu(PDU_SYMM);
			LOGD("send SYMM\n");
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
			LOGD("send DISC\n");
			m_CommandLen = PDU_INFOPOS;
			createPdu(PDU_DISC);
			break;
		
		case LSTAT_DM:
			//�ؒf
			LOGD("send DM\n");
			NfcPcd::commandBuf(PDU_INFOPOS) = NfcPcd::commandBuf(0);
			m_CommandLen = PDU_INFOPOS + 1;
			createPdu(PDU_DM);
			break;
		}
		if(m_CommandLen) {
			bool b = respAsTarget(NfcPcd::commandBuf(), m_CommandLen);
			if(m_LlcpStat == LSTAT_DM) {
				//DM���M��͋����I������
				LOGD("DM send\n");
				killConnection();
			} else if(b) {
				//PDU��M���ɂȂ�
				m_bSend = false;
				m_CommandLen = 0;
				
				startTimer(m_LinkTimeout);
			} else {
				LOGE("error\n");
				if(m_LlcpStat == LSTAT_DM) {
					//��������
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
