#include <cstdio>
#include <cstring>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include "devaccess.h"
#include "nfclog.h"

// デバッグ設定
//#define DBG_WRITEDATA
//#define DBG_READDATA


/**
* @brief		RC-S620/S用のI/F
*/
namespace DevAccess {
	const char* NFCPCD_DEV = "/dev/ttyS15";		//COM16
//	const char* NFCPCD_DEV = "/dev/ttyUSB0";

	int s_fd = -1;		///< シリアルポートのファイルディスクリプタ

/**
 * ポートオープン
 *
 * @retval	true	オープン成功
 */
bool open()
{
	if(s_fd != -1) {
		LOGI("already opened");
		return true;
	}

	//シリアルオープン
#ifndef __CYGWIN__
	s_fd = ::open(NFCPCD_DEV, O_RDWR /*| O_NONBLOCK*/);
#else
	s_fd = ::open(NFCPCD_DEV, O_RDWR);	//cygwinだとnon-blockがうまく動かん
#endif
	if(s_fd == -1) {
		LOGE("open fail.");
		return false;
	}

	//シリアル通信設定
	struct termios ter;
	std::memset(&ter, 0, sizeof(ter));

	ter.c_iflag = IGNPAR;
	//ter.c_oflag = 0;
	ter.c_cflag = CLOCAL | CS8 | B115200 | CREAD;
	//ter.c_lflag = 0;
	//ter.c_cc[VMIN] = 0;
	//ter.c_cc[VTIME] = 0;

	tcsetattr(s_fd, TCSANOW, &ter);

	return true;
}


/**
 * ポートクローズ
 */
void close()
{
	::close(s_fd);
	s_fd = -1;
}


/**
 * ポート送信
 *
 * @param[in]	data		送信データ
 * @param[in]	len			dataの長さ
 * @return					送信したサイズ
 */
uint16_t write(const uint8_t* data, uint16_t len)
{
	//LOGD("[NfcPcd]_port_write");

#ifdef DBG_WRITEDATA
	LOGD("write(%d):", len);
	for(int i=0; i<len; i++) {
		LOGD("[W]%02x ", data[i]);
	}
#endif

	errno = 0;

	uint16_t ret_len = 0;

	do {
		ret_len = ::write(s_fd, data, len);
		if(errno) {
			LOGE("_port_write:%s", strerror(errno));
		}
		len -= ret_len;
		data += ret_len;
	} while(len);
	fsync(s_fd);

	return ret_len;
}

/**
 * 受信
 *
 * @param[out]	data		受信バッファ
 * @param[in]	len			受信サイズ
 *
 * @return					受信したサイズ
 *
 * @attention	- len分のデータを受信するか失敗するまで処理がブロックされる。
 */
uint16_t read(uint8_t* data, uint16_t len)
{
	//LOGD("[NfcPcd]_port_read");
	uint16_t ret_len = 0;

#ifndef __CYGWIN__

	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(s_fd, &fds);

	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	do {
		errno = 0;
		int result  = ::select(FD_SETSIZE, &fds, (fd_set *)0, (fd_set *)0, &tv);
		if(result <= 0) {
			LOGE("read err: result=%d ret_len=%d [%s]", result, ret_len, strerror(errno));
			break;
		}
		else {
			if(FD_ISSET(s_fd, &fds)) {
				int nr;
				ioctl(s_fd, FIONREAD, &nr);
				//LOGD("ret : %d", ret);
				if(ret_len + nr > len) {
					nr = len - ret_len;
				}
				ssize_t sz = ::read(s_fd, data, nr);
				ret_len += sz;
			}
		}
	} while(ret_len < len);

#ifdef DBG_READDATA
	LOGD("read(%d)", ret_len);
	for(int i=0; i<ret_len; i++) {
		LOGD("[R]%02x", data[i]);
	}
#endif

#else	//__CYGWIN__

#ifdef DBG_READDATA
	LOGD("read(%d): ", len);
#endif
	ret_len = len;
	do {
		size_t sz = ::read(s_fd, data, 1);
		if(sz == 1) {
#ifdef DBG_READDATA
			LOGD("[R]%02x ", *data);
#endif
			len--;
			data++;
		}
	} while(len);
#endif	//__CYGWIN__

	return ret_len;
}

}	//namespace DevAccess

