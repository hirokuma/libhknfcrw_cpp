#ifndef HK_NFCDEP_H
#define HK_NFCDEP_H

#include "HkNfcRw.h"


/**
 * @class	HkNfcDep
 * @brief	NFC-DEPアクセス
 */
class HkNfcDep {
private:
	static const uint32_t _ACT = 0x80000000;
	static const uint32_t _PSV = 0x00000000;
	static const uint32_t _AP_MASK= 0x80000000;
	
	static const uint32_t _BR106K = 0x40000000;
	static const uint32_t _BR212K = 0x20000000;
	static const uint32_t _BR424K = 0x10000000;
	static const uint32_t _BR_MASK= 0x70000000;
	

public:
	enum DepMode {
		ACT_106K = _ACT | _BR106K,
		PSV_106K = _PSV | _BR106K,
		
		ACT_212K = _ACT | _BR212K,
		PSV_212K = _PSV | _BR212K,
		
		ACT_424K = _ACT | _BR424K,
		PSV_424K = _PSV | _BR424K,
	};

public:
	HkNfcDep(HkNfcRw* pRw);
	virtual ~HkNfcDep();

public:
	bool startAsInitiator(DepMode mode, const uint8_t* pGt = 0, uint8_t GtLen = 0);
	bool startAsTarget(const uint8_t* pGt = 0, uint8_t GtLen = 0);

private:
	HkNfcRw*		m_pHkNfcRw;
	NfcPcd*			m_pNfcPcd;

	friend class HkNfcRw;
};

#endif /* HK_NFCDEP_H */
