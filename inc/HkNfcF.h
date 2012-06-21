#ifndef HKNFC_F_H
#define HKNFC_F_H

#include "HkNfcRw.h"


/**
 * @class	HkNfcF
 * @brief	NFC-F(FeliCa)アクセス
 */
class HkNfcF {
public:
	static const uint8_t NFCID_LEN = HkNfcRw::NFCID2_LEN;	///< NFCID長さ
	static const uint16_t SVCCODE_RW = 0x0009;		///< サービスコード:R/W
	static const uint16_t SVCCODE_RO = 0x000b;		///< サービスコード:RO

public:
	static bool polling(uint16_t systemCode = 0xffff);

public:
	static void release();
	static bool read(uint8_t* buf, uint8_t blockNo=0x00);
	static bool write(const uint8_t* buf, uint8_t blockNo=0x00);

	static void setServiceCode(uint16_t svccode);

#ifdef QHKNFCRW_USE_FELICA
	/// Request System Code
	static bool reqSystemCode(uint8_t* pNums);
	/// Search Service Code
	static bool searchServiceCode();
	/// Push
	static bool push(const uint8_t* data, uint8_t dataLen);
#endif	//QHKNFCRW_USE_FELICA


private:
	static uint16_t		m_SystemCode;		///< システムコード
	static uint16_t		m_SvcCode;

#ifdef QHKNFCRW_USE_FELICA
	static uint16_t		m_syscode[16];
	static uint8_t			m_syscode_num;
#endif	//QHKNFCRW_USE_FELICA
};

#endif /* HKNFC_F_H */
