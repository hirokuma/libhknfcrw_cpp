#ifndef HK_NFCDEP_H
#define HK_NFCDEP_H

#include <stdint.h>

/**
 * @class		HkNfcDep
 * @brief		NFC-DEPアクセス
 * @defgroup	gp_NfcDep	NfcDepクラス
 */
class HkNfcDep {
private:
	static const uint32_t _ACT = 0x80000000;		///< Active
	static const uint32_t _PSV = 0x00000000;		///< Passive
	static const uint32_t _AP_MASK= 0x80000000;		///< Active/Passive用マスク
	
	static const uint32_t _BR106K = 0x40000000;		///< 106kbps
	static const uint32_t _BR212K = 0x20000000;		///< 212kbps
	static const uint32_t _BR424K = 0x10000000;		///< 424kbps
	static const uint32_t _BR_MASK= 0x70000000;		///< Baudrate用マスク
	

public:
	/**
	 * @enum	HkNfcDep::DepMode
	 * 
	 * DEPでの通信モード
	 */
	enum DepMode {
		DEP_NONE = 0x00000000,			///< DEP開始前(取得のみ)
		
		ACT_106K = _ACT | _BR106K,		///< 106kbps Active
		PSV_106K = _PSV | _BR106K,		///< 106kbps Passive
		
		ACT_212K = _ACT | _BR212K,		///< 212kbps Active
		PSV_212K = _PSV | _BR212K,		///< 212kbps Passive
		
		ACT_424K = _ACT | _BR424K,		///< 424kbps Active
		PSV_424K = _PSV | _BR424K,		///< 424kbps Passive
	};

private:
	/**
	 * @enum	HkNfcDep::PduType
	 * 
	 * 	PDU種別
	 */
	enum PduType {
		PDU_SYMM	= 0x00,			///< SYMM
		PDU_PAX		= 0x01,			///< PAX
		PDU_AGF		= 0x02,			///< AGF
		PDU_UI		= 0x03,			///< UI
		PDU_CONN	= 0x04,			///< CONNECT
		PDU_DISC	= 0x05,			///< DISC
		PDU_CC		= 0x06,			///< CC
		PDU_DM		= 0x07,			///< DM
		PDU_FRMR	= 0x08,			///< FRMR
		PDU_RESV1	= 0x09,			///< -
		PDU_RESV2	= 0x0a,			///< -
		PDU_RESV3	= 0x0b,			///< -
		PDU_I		= 0x0c,			///< I
		PDU_RR		= 0x0d,			///< RR
		PDU_RNR		= 0x0e,			///< RNR
		PDU_RESV4	= 0x0f,			///< -
		PDU_LAST = PDU_RESV4,		///< -

		PDU_NONE	= 0xff			///< PDU範囲外
	};


	/// @addtogroup gp_depinit	NFC-DEP(Initiator)
	/// @ingroup gp_NfcDep
	/// @{
public:
	/// InJumpForDEP
	static bool startAsInitiator(DepMode mode, bool bLlcp = true);
	/// InDataExchange
	static bool sendAsInitiator(
			const void* pCommand, uint8_t CommandLen,
			void* pResponse, uint8_t* pResponseLen);
	/// RLS_REQ
	static bool stopAsInitiator();
	/// @}


	/// @addtogroup gp_depinit	NFC-DEP(Target)
	/// @ingroup gp_NfcDep
	/// @{
public:
	/// TgInitTarget, TgSetGeneralBytes
	static bool startAsTarget(bool bLlcp=true);
	/// TgGetData
	static bool recvAsTarget(void* pCommand, uint8_t* pCommandLen);
	/// TgSetData
	static bool respAsTarget(const void* pResponse, uint8_t ResponseLen);
	/// @}


	/// @addtogroup gp_depstat	Status
	/// @ingroup gp_NfcDep
	/// @{
public:
	static DepMode getDepMode() { return m_DepMode; }
	/// @}


	/// @addtogroup gp_llcp		LLCP Target
	/// @ingroup gp_NfcDep
	/// @{
public:


private:
	static uint8_t analyzePdu(const uint8_t* pBuf, PduType* pResPdu);
	static uint8_t analyzeSymm(const uint8_t* pBuf);
	static uint8_t analyzePax(const uint8_t* pBuf);
	static uint8_t analyzeAgf(const uint8_t* pBuf);
	static uint8_t analyzeUi(const uint8_t* pBuf);
	static uint8_t analyzeConn(const uint8_t* pBuf);
	static uint8_t analyzeDisc(const uint8_t* pBuf);
	static uint8_t analyzeCc(const uint8_t* pBuf);
	static uint8_t analyzeDm(const uint8_t* pBuf);
	static uint8_t analyzeFrmr(const uint8_t* pBuf);
	static uint8_t analyzeI(const uint8_t* pBuf);
	static uint8_t analyzeRr(const uint8_t* pBuf);
	static uint8_t analyzeRnr(const uint8_t* pBuf);
	static uint8_t analyzeDummy(const uint8_t* pBuf);
	static uint8_t (*sAnalyzePdu[])(const uint8_t* pBuf);

	static uint8_t analyzeParamList(const uint8_t *pBuf);
	
	static void requestReject();
	/// @}


private:
	static DepMode		m_DepMode;			///< 現在のDepMode
	static bool			m_bInitiator;		///< true:Initiator / false:Target or not DEP mode
	static uint16_t		m_LinkTimeout;		///< Link Timeout値[msec](デフォルト:100ms)
};

#endif /* HK_NFCDEP_H */
