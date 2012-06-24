#include <unistd.h>
#include <sys/time.h>
#include "misc.h"

namespace HkNfcRwMisc {

static timeval s_TimeVal;
static uint16_t s_Timeout;

/**
 *  @brief	ミリ秒スリープ
 *
 * @param	msec	待ち時間[msec]
 */
void msleep(uint16_t msec) {
	usleep(msec * 1000);
}


/**
 * タイマ開始
 *
 * @param[in]	tmval	タイムアウト時間[msec]
 */
void startTimer(uint16_t tmval)
{
	gettimeofday(&s_TimeVal, 0);
	s_Timeout = tmval;
}


/**
 *  タイムアウト監視
 * 
 * @retval	true	タイムアウト発生
 */
bool isTimeout()
{
	timeval tm;
	timeval res;
	gettimeofday(&tm, 0);
	timersub(&tm, &s_TimeVal, &res);
	uint32_t tt = res.tv_sec * 1000 + res.tv_usec / 1000;
	return tt >= s_Timeout;
}

}	//namespace HkNfcRwMisc
