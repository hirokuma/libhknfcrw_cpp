#ifndef HK_NFCLLCPT_H
#define HK_NFCLLCPT_H

#include <stdint.h>
#include "HkNfcDep.h"

class HkNfcLlcpT : public HkNfcDep {
public:
	static bool start();
	static bool stopRequest();
	static bool sendRequest(const void* pBuf, uint8_t len);

	static void poll();
	

private:
	HkNfcLlcpT();
	HkNfcLlcpT(const HkNfcLlcpT&);
	~HkNfcLlcpT();
};


#endif /* HK_NFCLLCPT_H */
