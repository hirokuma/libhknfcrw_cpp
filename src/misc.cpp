#include <unistd.h>
#include <sys/time.h>
#include "nfclog.h"
#include "misc.h"

namespace HkNfcRwMisc {

static timeval s_TimeVal;
static uint16_t s_Timeout = 0;

/**
 *  @brief	�~���b�X���[�v
 *
 * @param	msec	�҂�����[msec]
 */
void msleep(uint16_t msec) {
	usleep(msec * 1000);
}


/**
 * �^�C�}�J�n
 *
 * @param[in]	tmval	�^�C���A�E�g����[msec]
 */
void startTimer(uint16_t tmval)
{
	gettimeofday(&s_TimeVal, 0);
	s_Timeout = tmval;
	//LOGD("  startTimer : (%ld, %ld)\n", s_TimeVal.tv_sec, s_TimeVal.tv_usec);
}


/**
 *  �^�C���A�E�g�Ď�
 * 
 * @retval	true	�^�C���A�E�g����
 */
bool isTimeout()
{
	if(s_Timeout == 0) {
		return false;
	}

	timeval now;
	timeval res;
	gettimeofday(&now, 0);
	timersub(&now, &s_TimeVal, &res);
	uint32_t tt = res.tv_sec * 1000 + res.tv_usec / 1000;
	bool b = tt >= s_Timeout;
	if(b) {
		LOGD("  isTimeout : (%ld, %ld) - (%ld, %ld)\n", now.tv_sec, now.tv_usec, s_TimeVal.tv_sec, s_TimeVal.tv_usec);
	}
	s_Timeout = 0;
	return b;
}

}	//namespace HkNfcRwMisc
