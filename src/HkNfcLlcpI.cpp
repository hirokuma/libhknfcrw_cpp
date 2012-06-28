#include <cstring>
#include <cstdlib>

#include "HkNfcLlcpI.h"
#include "NfcPcd.h"
#include "misc.h"

#define LOG_TAG "HkNfcLlcpI"
#include "nfclog.h"


using namespace HkNfcRwMisc;


/**
 * LLCP�J�n
 *
 * @param[in]	mode		�J�n����DepMode
 * @return		����/���s
 */
bool HkNfcLlcpI::start(DepMode mode)
{
	LOGD("%s\n", __PRETTY_FUNCTION__);
	
	bool ret = HkNfcDep::startAsInitiator(mode, true);
	if(ret) {
		//PDU���M��
		m_bSend = true;
		m_LlcpStat = LSTAT_NOT_CONNECT;
	} else {
		killConnection();
	}

	return ret;
}


/**
 * LLCP(Initiator)�I���v��
 *
 * @return		true:�v���󂯓���
 */
bool HkNfcLlcpI::stopRequest()
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
 * LLCP(Initiator)�f�[�^���M�v��
 *
 * @param[in]	pBuf	���M�f�[�^�B�ő�#LLCP_MIU[byte]
 * @param[in]	len		���M�f�[�^���B�ő�#LLCP_MIU.
 * @retval		true	���M�f�[�^�󂯓���
 */
bool HkNfcLlcpI::sendRequest(const void* pBuf, uint8_t len)
{
	LOGD("%s(%d)\n", __PRETTY_FUNCTION__, m_LlcpStat);
	
	return setSendData(pBuf, len);
}


/**
 * LLCP(Initiator)�������.
 * �J�n��A��邱�Ƃ��Ȃ��Ԃ� #poll() ���Ăяo�������邱��.
 * ��{�I�ɂ́A #getDepMode() �� #DEP_NONE �ɂȂ�܂ŌĂяo��������.
 */
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
			LOGD("*");
			createPdu(PDU_SYMM);
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
			m_CommandLen = PDU_INFOPOS;
			createPdu(PDU_DISC);
			LOGD("send DISC\n");
			break;
		
		case LSTAT_DM:
			//�ؒf
			NfcPcd::commandBuf(PDU_INFOPOS) = NfcPcd::commandBuf(0);
			m_CommandLen = PDU_INFOPOS + 1;
			createPdu(PDU_DM);
			LOGD("send DM\n");
			break;
		}
		if(m_CommandLen == 0) {
			//SYMM�ł��̂�
			m_CommandLen = PDU_INFOPOS;
			createPdu(PDU_SYMM);
			LOGD("*");
		}
		
		uint8_t len;
		startTimer(m_LinkTimeout);
		bool b = sendAsInitiator(NfcPcd::commandBuf(), m_CommandLen, NfcPcd::responseBuf(), &len);
		if(m_LlcpStat == LSTAT_DM) {
			//DM���M��͋����I������
			LOGD("DM sent\n");
			stopAsInitiator();
			killConnection();
		} else if(b) {
			if(isTimeout()) {
				//���肩��ʐM���Ԃ��Ă��Ȃ�
				LOGE("Link timeout\n");
				m_bSend = true;
				m_LlcpStat = LSTAT_TERM;
				m_DSAP = SAP_MNG;
				m_SSAP = SAP_MNG;
			} else {
				if((m_LlcpStat == LSTAT_CONNECTING) && (m_LastSentPdu == PDU_CC)) {
					m_LlcpStat = LSTAT_NORMAL;
				}

				//��M�͍ς�ł���̂ŁA����PDU���M���ɂȂ�
				m_bSend = true;
				m_CommandLen = 0;

				PduType type;
				uint8_t pdu = analyzePdu(NfcPcd::responseBuf(), len, &type);
			}
		} else {
			LOGE("error\n");
			//��������
			killConnection();
		}
	}
}
