#ifndef HK_NFCLLCPI_H
#define HK_NFCLLCPI_H

#include <stdint.h>
#include "HkNfcDep.h"

class HkNfcLlcpI : public HkNfcDep {
public:
	static bool start(DepMode mode);
	static bool stopRequest();

	static void poll();

private:
	HkNfcLlcpI();
	HkNfcLlcpI(const HkNfcLlcpI&);
	~HkNfcLlcpI();
};


#endif /* HK_NFCLLCP_INITIATOR_H */
