#ifndef HK_NFCDEP_H
#define HK_NFCDEP_H

#include "HkNfcRw.h"


/**
 * @class	HkNfcDep
 * @brief	NFC-DEPアクセス
 */
class HkNfcDep {
	friend class HkNfcRw;
public:


public:
	HkNfcDep(HkNfcRw* pRw);
	virtual ~HkNfcDep();

private:
	HkNfcRw*		m_pHkNfcRw;
	NfcPcd*			m_pNfcPcd;
};

#endif /* HK_NFCDEP_H */
