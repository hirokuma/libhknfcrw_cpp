#include <cstring>
#include <sys/types.h>
#include <libusbx-1.0/libusb.h>

#include "devaccess.h"
#include "nfclog.h"

// デバッグ設定
//#define DBG_WRITEDATA
//#define DBG_READDATA

namespace {

	const uint16_t VID = 0x054c;
	const uint16_t PID = 0x02e1;
	const unsigned int TIMEOUT = 65535 * 2;

	libusb_context*			m_pContext;
	libusb_device_handle*	m_pHandle;

	uint8_t m_EndPntIn = 0xff;
	uint8_t m_EndPntOut = 0xff;

	int m_ReadSize;
	uint8_t m_ReadBuf[256];
	int m_ReadPtr;

	class librcs370 {
	public:
		librcs370();
		virtual ~librcs370();

	public:
		bool open();
		void close();

		uint8_t write(const uint8_t *pData, uint8_t size);
		uint8_t read(uint8_t *pData, uint8_t size);
	};

	librcs370::librcs370()
	{
	}

	librcs370::~librcs370()
	{
		close();
	}


	bool librcs370::open()
	{
		//LOGD("%s\n", __PRETTY_FUNCTION__);

		if(m_pContext) {
			// open済み
			LOGD("already opened\n");
			return true;
		}

		int r;

		r = libusb_init(&m_pContext);
		if (r < 0) {
			LOGE("cannot init\n");
			return false;
		}
	//	libusb_set_debug(NULL, 3);

#if 1
		libusb_device **devs;
		ssize_t cnt = libusb_get_device_list(m_pContext, &devs);
		if (cnt < 0) {
			LOGE("cannot list\n");
			close();
			return false;
		}

		libusb_device *dev;
		int i = 0;

		while ((dev = devs[i++]) != NULL) {
			struct libusb_device_descriptor desc;
			int r = libusb_get_device_descriptor(dev, &desc);
			if (r < 0) {
				LOGE("failed to get device descriptor\n");
				i = -1;
				break;
			}
			if((desc.idVendor == VID) && (desc.idProduct == PID)) {
				i = 0;
				break;
			}
		}
		if(i != 0) {
			LOGE("cannot find\n");
			close();
			return false;
		}
		r = libusb_open(dev, &m_pHandle);
		if(r != 0) {
			LOGE("cannot open : %d\n", r);
			close();
			return false;
		}
		libusb_free_device_list(devs, 1);

#else
		m_pHandle = libusb_open_device_with_vid_pid(NULL, VID, PID);
		if(m_pHandle == NULL) {
			LOGE("cannot open\n");
			close();
			return false;
		}
#endif

		libusb_config_descriptor* pConfig;
		r = libusb_get_config_descriptor(dev, 0, &pConfig);
		if(r != 0) {
			LOGE("cannot getdesc : %d\n", r);
			close();
			return false;
		}
		for(int inf = 0; inf < pConfig->bNumInterfaces; inf++) {
			const libusb_interface& inter = pConfig->interface[inf];
			for(int alt = 0; alt < inter.num_altsetting; alt++) {
				const libusb_interface_descriptor& idesc = inter.altsetting[alt];
				for(int desc = 0; desc < idesc.bNumEndpoints; desc++) {
					const libusb_endpoint_descriptor& EndPnt = idesc.endpoint[desc];
					if(EndPnt.bEndpointAddress & LIBUSB_ENDPOINT_IN) {
						LOGD("[IN]type : %x / addr : %x\n", EndPnt.bDescriptorType, EndPnt.bEndpointAddress);
						m_EndPntIn = EndPnt.bEndpointAddress;
					}
					else {
						LOGD("[OUT]type : %x / addr : %x\n", EndPnt.bDescriptorType, EndPnt.bEndpointAddress);
						m_EndPntOut = EndPnt.bEndpointAddress;
					}
				}
			}
		}

		r = libusb_claim_interface(m_pHandle, 0);
		if(r != 0) {
			LOGE("cannot claim\n");
			close();
			return false;
		}

		//Debug Out
		libusb_set_debug(m_pContext, 3);

		//LOGD("open!\n");
		return true;
	}


	void librcs370::close()
	{
		if(m_pHandle) {
			libusb_release_interface(m_pHandle, 0);
			libusb_close(m_pHandle);
			m_pHandle = NULL;
		}
		if(m_pContext) {
			libusb_exit(m_pContext);
			m_pContext = NULL;
		}

		LOGD("close\n");
	}

	uint8_t librcs370::write(const uint8_t *pData, uint8_t size)
	{
		if(m_pHandle == NULL) {
			return 0;
		}

		int transferred = 0;
		int r = libusb_bulk_transfer(m_pHandle, m_EndPntOut, const_cast<uint8_t*>(pData), size, &transferred, TIMEOUT);
		if(r) {
			LOGE("err : %d / transferred : %d\n", r, transferred);
		}
		return transferred;
	}

	uint8_t librcs370::read(uint8_t *pData, uint8_t size)
	{
		if(m_pHandle == NULL) {
			return 0;
		}

		if(m_ReadSize) {
			std::memcpy(pData, &m_ReadBuf[m_ReadPtr], size);
			m_ReadSize -= size;
			m_ReadPtr += size;
			return size;
		}

		m_ReadPtr = 0;
		int r = libusb_bulk_transfer(m_pHandle, m_EndPntIn, m_ReadBuf, sizeof(m_ReadBuf), &m_ReadSize, TIMEOUT);
		if(r) {
			LOGE("err : %d / transferred : %d\n", r, m_ReadSize);
		}

		std::memcpy(pData, &m_ReadBuf[m_ReadPtr], size);
		m_ReadSize -= size;
		m_ReadPtr = size;

		return size;
	}

}	//namespace


/**
 * @brief		PaSoRi I/F
 */

namespace DevAccess {


librcs370		m_Rcs;

/**
 * ポートオープン
 *
 * @retval	true	オープン成功
 */
bool open()
{
	return m_Rcs.open();
}


/**
 * ポートクローズ
 */
void close()
{
	m_Rcs.close();
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

	return m_Rcs.write(data, len);
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

	ret_len = m_Rcs.read(data, len);

#ifdef DBG_READDATA
	LOGD("read(%d)", ret_len);
	for(int i=0; i<ret_len; i++) {
		LOGD("[R]%02x ", data[i]);
	}
#endif

	return ret_len;
}

}	//namespace DevAccess
