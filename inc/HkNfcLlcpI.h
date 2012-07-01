#ifndef HK_NFCLLCPI_H
#define HK_NFCLLCPI_H

#include <stdint.h>
#include "HkNfcDep.h"

class HkNfcLlcpI : public HkNfcDep {
public:
	static bool start(DepMode mode, void (*pRecvCb)(const void* pBuf, uint8_t len));
	static bool stopRequest();
	static bool addSendData(const void* pBuf, uint8_t len);
	static bool sendRequest();

	static bool poll();

private:
	HkNfcLlcpI();
	HkNfcLlcpI(const HkNfcLlcpI&);
	~HkNfcLlcpI();
};


#endif /* HK_NFCLLCP_INITIATOR_H */
