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
	HkNfcF();
	virtual ~HkNfcF();

public:
	bool polling(uint16_t systemCode = 0xffff);

public:
	virtual void release();
	virtual bool read(uint8_t* buf, uint8_t blockNo=0x00);
	virtual bool write(const uint8_t* buf, uint8_t blockNo=0x00);

	void setServiceCode(uint16_t svccode);

#ifdef QHKNFCRW_USE_FELICA
	/// Request System Code
	bool reqSystemCode(uint8_t* pNums);
	/// Search Service Code
	bool searchServiceCode();
	/// Push
	bool push(const uint8_t* data, uint8_t dataLen);
#endif	//QHKNFCRW_USE_FELICA


private:
	uint16_t		m_SystemCode;		///< システムコード
	uint16_t		m_SvcCode;

#ifdef QHKNFCRW_USE_FELICA
	uint16_t		m_syscode[16];
	uint8_t			m_syscode_num;
#endif	//QHKNFCRW_USE_FELICA


#ifdef QHKNFCRW_USE_FALP
public:
	/// FALP
	bool falpSendString(const uint8_t* data, uint16_t dataLen);

private:
	uint8_t* createVnote(const uint8_t* data, uint16_t dataLen, uint16_t* pVnoteLen);
	uint8_t falpPacket(uint16_t UId, uint16_t SId, uint8_t Cmd, uint8_t Len);
	bool falpCheck(uint8_t* pFalpLen, uint8_t* pRes, uint8_t ChkCode=0x01);
	bool falpObexWait(uint8_t responseLen);
	bool falpInit();
	bool falpPrepare(uint16_t uid);
	bool falpFinish();
	bool falpObexConnect();
	bool falpObexPut(const uint8_t* data, uint16_t dataLen);
	bool falpObexDisconnect();
private:
	uint16_t		m_FalpUId;
	uint16_t		m_FalpSId;
	uint16_t		m_FalpRId;
#endif	//QHKNFCRW_USE_FALP

};

#endif /* HKNFC_F_H */
