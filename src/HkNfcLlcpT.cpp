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
	LOGD("%s\n", __PRETTY_FUNCTION__);
	
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
 * LLCP(Target)�I���v��
 *
 * @return		true:�v���󂯓���
 */
bool HkNfcLlcpT::stopRequest()
{
	LOGD("%s(%d)\n", __PRETTY_FUNCTION__, m_LlcpStat);
	
	switch(m_LlcpStat) {
	case LSTAT_NOT_CONNECT:
	case LSTAT_CONNECTING:
		LOGD("==>LSTAT_DM\n");
		m_LlcpStat = LSTAT_DM;
		break;
	
	case LSTAT_NONE:
	case LSTAT_TERM:
	case LSTAT_DM:
	case LSTAT_WAIT_DM:
		break;
	
	case LSTAT_NORMAL:
	case LSTAT_BUSY:
		LOGD("==>LSTAT_TERM\n");
		m_LlcpStat = LSTAT_TERM;
		break;
	}
	
	return true;
}


/**
 * LLCP(Target)�f�[�^���M�v��
 *
 * @param[in]	pBuf	���M�f�[�^�B�ő�#LLCP_MIU[byte]
 * @param[in]	len		���M�f�[�^���B�ő�#LLCP_MIU.
 * @retval		true	���M�f�[�^�󂯓���
 */
bool HkNfcLlcpT::sendRequest(const void* pBuf, uint8_t len)
{
	LOGD("%s(%d)\n", __PRETTY_FUNCTION__, m_LlcpStat);
	
	return setSendData(pBuf, len);
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
			m_LlcpStat = LSTAT_TERM;
			m_DSAP = SAP_MNG;
			m_SSAP = SAP_MNG;
		} else if(b) {
			PduType type;
			uint8_t pdu = analyzePdu(NfcPcd::responseBuf(), len, &type);
			//PDU���M���ɂȂ�
			m_bSend = true;
		} else {
			//�������߂��낤
			LOGE("recv error\n");
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
			//LOGD("send SYMM\n");
			LOGD("*");
			break;
		
		case LSTAT_CONNECTING:
			//CONNECT or CC�̓f�[�^�ݒ�ς�
			if(m_CommandLen) {
				LOGD("send CONNECT or CC\n");
			}
			break;

		case LSTAT_NORMAL:
			//
			if(m_SendLen) {
				//���M�f�[�^����
				LOGD("send I(VR:%d / VS:%d)\n", m_ValueR, m_ValueS);
				m_CommandLen = PDU_INFOPOS + 1 + m_SendLen;
				createPdu(PDU_I);
				NfcPcd::commandBuf(PDU_INFOPOS) = (uint8_t)((m_ValueS << 4) | m_ValueR);
				std::memcpy(NfcPcd::commandBuf() + PDU_INFOPOS + 1, m_SendBuf, m_SendLen);
				m_SendLen = 0;
				m_ValueS++;
			} else {
				m_CommandLen = PDU_INFOPOS;
				createPdu(PDU_RR);
			}
			break;

		case LSTAT_TERM:
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
		if(m_CommandLen == 0) {
			//SYMM�ł��̂�
			m_CommandLen = PDU_INFOPOS;
			createPdu(PDU_SYMM);
		}
		bool b = respAsTarget(NfcPcd::commandBuf(), m_CommandLen);
		if(m_LlcpStat == LSTAT_DM) {
			//DM���M��͋����I������
			LOGD("DM sent\n");
			killConnection();
		} else if(b) {
			if(m_LlcpStat == LSTAT_TERM) {
				//DISC���M��
				if((m_DSAP == 0) && (m_SSAP == 0)) {
					LOGD("fin : Link Deactivation\n");
					killConnection();
				} else {
					LOGD("wait DM\n");
					m_LlcpStat = LSTAT_WAIT_DM;
				}
			} else {
				if((m_LlcpStat == LSTAT_CONNECTING) && (m_LastSentPdu == PDU_CC)) {
					m_LlcpStat = LSTAT_NORMAL;
				}
				//PDU��M���ɂȂ�
				m_bSend = false;
				m_CommandLen = 0;
				
				startTimer(m_LinkTimeout);
			}
		} else {
			LOGE("send error\n");
			//��������
			killConnection();
		}
	}
}
