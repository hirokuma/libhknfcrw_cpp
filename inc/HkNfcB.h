#ifndef HKNFCB_H
#define HKNFCB_H

#include "HkNfcRw.h"


/**
 * @class	HkNfcB
 * @brief	NFC-Bアクセス
 */
class HkNfcB {
	friend class HkNfcRw;
public:
	bool polling();

	virtual bool read(uint8_t* buf, uint8_t blockNo=0x00) { return false; }
	virtual bool write(const uint8_t* buf, uint8_t blockNo=0x00) { return false; }

private:
    HkNfcB(HkNfcRw* pRw);
    virtual ~HkNfcB();

private:
	HkNfcRw*	m_pHkNfcRw;
	NfcPcd*		m_pNfcPcd;
};


#endif /* HKNFCB_H */
