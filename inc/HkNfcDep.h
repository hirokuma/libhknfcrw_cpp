#ifndef HK_NFCDEP_H
#define HK_NFCDEP_H

#include "HkNfcRw.h"


/**
 * @class		HkNfcDep
 * @brief		NFC-DEPアクセス
 * @defgroup	gp_NfcDep	NfcDepクラス
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
		DEP_NONE = 0x00000000,
		
		ACT_106K = _ACT | _BR106K,
		PSV_106K = _PSV | _BR106K,
		
		ACT_212K = _ACT | _BR212K,
		PSV_212K = _PSV | _BR212K,
		
		ACT_424K = _ACT | _BR424K,
		PSV_424K = _PSV | _BR424K,
	};

public:
	enum PduType {
		PDU_SYMM	= 0x00,
		PDU_PAX		= 0x01,
		PDU_AGF		= 0x02,
		PDU_UI		= 0x03,
		PDU_CONN	= 0x04,
		PDU_DISC	= 0x05,
		PDU_CC		= 0x06,
		PDU_DM		= 0x07,
		PDU_FRMR	= 0x08,
		PDU_RESV1	= 0x09,
		PDU_RESV2	= 0x0a,
		PDU_RESV3	= 0x0b,
		PDU_I		= 0x0c,
		PDU_RR		= 0x0d,
		PDU_RNR		= 0x0e,
		PDU_RESV4	= 0x0f
	};

public:
	HkNfcDep(HkNfcRw* pRw);
	virtual ~HkNfcDep();

public:
	/// @addtogroup gp_depinit	NFC-DEP(Initiator)
	/// @ingroup gp_NfcDep
	/// @{

	/// InJumpForDEP
	bool startAsInitiator(DepMode mode, bool bLlcp = true);
	/// InDataExchange
	bool sendAsInitiator(
			const void* pCommand, uint8_t CommandLen,
			void* pResponse, uint8_t* pResponseLen);
	/// RLS_REQ
	bool stopAsInitiator();
	/// @}

public:
	/// @addtogroup gp_depinit	NFC-DEP(Target)
	/// @ingroup gp_NfcDep
	/// @{

	/// TgInitTarget, TgSetGeneralBytes
	bool startAsTarget(bool bLlcp=true);
	/// TgGetData
	bool recvAsTarget(void* pCommand, uint8_t* pCommandLen);
	/// TgSetData
	bool respAsTarget(const void* pResponse, uint8_t ResponseLen);
	/// @}


public:
	/// @addtogroup gp_depstat	Status
	/// @ingroup gp_NfcDep
	/// @{
	DepMode getDepMode() const { return m_DepMode; }
	/// @}


public:
	/// @addtogroup gp_llcp		LLCP Target
	/// @ingroup gp_NfcDep
	/// @{

	/// @}

public:
	uint8_t analyzePdu(const uint8_t* pBuf, PduType* pResPdu);
	uint8_t analyzeSymm(const uint8_t* pBuf);
	uint8_t analyzePax(const uint8_t* pBuf);
	uint8_t analyzeAgf(const uint8_t* pBuf);
	uint8_t analyzeUi(const uint8_t* pBuf);
	uint8_t analyzeConn(const uint8_t* pBuf);
	uint8_t analyzeDisc(const uint8_t* pBuf);
	uint8_t analyzeCc(const uint8_t* pBuf);
	uint8_t analyzeDm(const uint8_t* pBuf);
	uint8_t analyzeFrmr(const uint8_t* pBuf);
	uint8_t analyzeI(const uint8_t* pBuf);
	uint8_t analyzeRr(const uint8_t* pBuf);
	uint8_t analyzeRnr(const uint8_t* pBuf);
	uint8_t analyzeDummy(const uint8_t* pBuf);
	static uint8_t (HkNfcDep::*sAnalyzePdu[])(const uint8_t* pBuf);

	uint8_t analyzeParamList(const uint8_t *pBuf);


private:
	HkNfcRw*		m_pHkNfcRw;
	NfcPcd*			m_pNfcPcd;
	DepMode			m_DepMode;
	bool			m_bInitiator;
};

#endif /* HK_NFCDEP_H */
