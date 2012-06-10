/**
 * @file	nfcpcd.h
 * @brief	NFCのPCD(Proximity Coupling Device)アクセス用ヘッダ
 */

#ifndef NFCPCD_H
#define NFCPCD_H

#include "HkNfcRw.h"

// forward
class HkNfcRw;
class HkNfcA;
class HkNfcB;
class HkNfcF;


/**
 * @class		NfcPcd
 * @brief		NFCのPCDアクセスクラス
 * @defgroup	gp_NfcPcd	NfcPcdクラス
 *
 * NFCのPCDにアクセスするPHY部
 */
class NfcPcd {
    friend class HkNfcRw;
	friend class HkNfcA;
	friend class HkNfcB;
	friend class HkNfcF;

public:
	static NfcPcd* getInstance();

public:

	/// @enum	ActPass
	/// @brief	inJumpForDep()参照
	enum ActPass {
		AP_PASSIVE	= 0x00,		///< パッシブモード(受動通信)
        AP_ACTIVE	= 0x01		///< アクティブモード(能動通信)
	};

	/// @enum	BaudRate
	/// @brief	inJumpForDep()参照
	enum BaudRate {
		BR_106K		= 0x00,		///< 106kbps(MIFAREなど)
		BR_212K		= 0x01,		///< 212kbps(FeliCa)
        BR_424K		= 0x02		///< 424kbps(FeliCa)
	};

private:
	/// @addtogroup gp_port	Device Port Control
	/// @ingroup gp_NfcPcd
	/// @{

	/// オープン
	bool portOpen();
	/// オープン済みかどうか
	bool isOpened() const { return m_bOpened; }
	/// クローズ
	void portClose();
	/// @}

	/// @addtogroup gp_nfcidi	NFCID3 for Initiator
	/// @ingroup gp_NfcPcd
	/// @{
	/**
	 * @brief NFCID3の取得(Initiator)
	 *
	 * イニシエータ向けNFCID3を取得する。
	 *
	 * @return		NFCID3t(10byte)
	 */
	const uint8_t* getNfcId3i() const { return m_NfcId3i; }

	/**
	 * @brief NFCID3の設定(Initiator)
	 *
	 * イニシエータ向けNFCID3を設定する。
	 *
	 * @param	[in]	pId		設定するNFCID3(10byte)
	 * @attention		NFCID2iを書き換える
	 */
	void setNfcId3i(const uint8_t* pId) { memcpy(m_NfcId3i, pId, HkNfcRw::NFCID3_LEN); }

	/**
	 * @brief NFCID2の設定(Initiator)
	 *
	 * イニシエータ向けNFCID2を設定する。
	 * NFCID3iは上書きされる。
	 *
	 * @param	[in]	pIdm		設定するNFCID8(8byte)
	 * @attention		NFCID3iを書き換える
	 */
	void setNfcId3iAsId2(const uint8_t* pIdm) {
        memcpy(m_NfcId3i, pIdm, HkNfcRw::NFCID2_LEN);
		m_NfcId3i[8] = 0x00;
		m_NfcId3i[9] = 0x00;
	}
	/// @}


	/// @addtogroup gp_nfcidt	NFCID3 for Target
	/// @ingroup gp_NfcPcd
	/// @{
	/**
	 * @brief NFCID3の取得(Target)
	 *
	 * ターゲット向けNFCID3を取得する
	 *
	 * @return		NFCID3i(10byte)
	 */
	const uint8_t* getNfcId3t() const { return m_NfcId3t; }

	/**
	 * @brief NFCID3の設定(Target)
	 *
	 * ターゲット向けNFCID3を設定する
	 *
	 * @param	[in]	pId		設定するNFCID3(10byte)
	 * @attention		NFCID2tを書き換える
	 */
	void setNfcId3t(const uint8_t* pId) { memcpy(m_NfcId3t, pId, HkNfcRw::NFCID3_LEN); }

	/**
	 * @brief NFCID2の設定(Target)
	 *
	 * ターゲット向けNFCID2を設定する。
	 * NFCID3tは上書きされる。
	 *
	 * @param	[in]	pIdm		設定するNFCID8(8byte)
	 * @attention		NFCID3tを書き換える
	 */
	void setNfcId3tAsId2(const uint8_t* pIdm) {
        memcpy(m_NfcId3t, pIdm, HkNfcRw::NFCID2_LEN);
		m_NfcId3t[8] = 0x00;
		m_NfcId3t[9] = 0x00;
	}
	/// @}


private:
	NfcPcd();
	virtual ~NfcPcd();

	/// @addtogroup gp_commoncmd	Common Command
	/// @ingroup gp_NfcPcd
	/// @{

	/// デバイス初期化
	bool init();
	/// RF出力停止
	bool rfOff();
	/// RFConfiguration
	bool rfConfiguration(uint8_t cmd, const uint8_t* pCommand, uint8_t CommandLen);
	/// Reset
	bool reset();
	/// Diagnose
	bool diagnose(
			uint8_t cmd, const uint8_t* pCommand, uint8_t CommandLen,
			uint8_t* pResponse, uint8_t* pResponseLen);
	/// SetParameters
	bool setParameters(uint8_t val);
	/// GetFirmware
	static const int GF_IC = 0;				///< GetFirmware:IC
	static const int GF_VER = 1;			///< GetFirmware:Ver
	static const int GF_REV = 2;			///< GetFirmware:Rev
	static const int GF_SUPPORT = 3;		///< GetFirmware:Support
	bool getFirmwareVersion(uint8_t* pResponse);
	/// GetGeneralStatus
	static const int GGS_ERR = 0;			///< GetGeneralStatus:??
	static const int GGS_FIELD = 1;			///< GetGeneralStatus:??
	static const int GGS_NBTG = 2;			///< GetGeneralStatus:??
	static const int GGS_TG = 3;			///< GetGeneralStatus:??
	static const int GGS_TXMODE = 4;		///< GetGeneralStatus:??
	static const int GGS_TXMODE_DEP = 0x03;			///< GetGeneralStatus:DEP
	static const int GGS_TXMODE_FALP = 0x05;		///< GetGeneralStatus:FALP
	bool getGeneralStatus(uint8_t* pResponse);
	/// CommunicateThruEX
	bool communicateThruEx(
			const uint8_t* pCommand, uint8_t CommandLen,
			uint8_t* pResponse, uint8_t* pResponseLen);
	/// CommunicateThruEX
	bool communicateThruEx(
			uint16_t Timeout,
			const uint8_t* pCommand, uint8_t CommandLen,
			uint8_t* pResponse, uint8_t* pResponseLen);
	/// @}


	/// @addtogroup gp_initiatorcmd	Initiator Command
	/// @ingroup gp_NfcPcd
	/// @{

	/// InJumpForDEP or ImJumpForPSL
	bool _inJump(uint8_t Cmd,
			ActPass Ap, BaudRate Br, bool bNfcId3,
			const uint8_t* pGt, uint8_t GtLen);
	/// InJumpForDEP
	bool inJumpForDep(
			ActPass Ap, BaudRate Br, bool bNfcId3,
			const uint8_t* pGt, uint8_t GtLen);
	/// InJumpForPSL
	bool inJumpForPsl(
			ActPass Ap, BaudRate Br, bool bNfcId3,
			const uint8_t* pGt, uint8_t GtLen);
	/// InListPassiveTarget
	bool inListPassiveTarget(
			const uint8_t* pInitData, uint8_t InitLen,
			uint8_t** ppTgData, uint8_t* pTgLen);
	/// InDataExchange
	bool inDataExchange(
			const uint8_t* pCommand, uint8_t CommandLen,
			uint8_t* pResponse, uint8_t* pResponseLen,
			bool bCoutinue=false);
	/// InCommunicateThru
	bool inCommunicateThru(
			const uint8_t* pCommand, uint8_t CommandLen,
			uint8_t* pResponse, uint8_t* pResponseLen);
	/// @}


	/// @addtogroup gp_targetcmd	Target Command
	/// @ingroup gp_NfcPcd
	/// @{

	/// TgInitAsTarget
	bool tgInitAsTarget(
			uint16_t SystemCode,
			const uint8_t* pGt, uint8_t GtLen);
	/// TgResponseToInitiator
	bool tgResponseToInitiator(
			const uint8_t* pData, uint8_t DataLen,
			uint8_t* pResponse=0, uint8_t* pResponseLen=0);
	/// TgGetInitiatorCommand
	bool tgGetInitiatorCommand(uint8_t* pResponse, uint8_t* pResponseLen);
	/// TgGetData
	bool tgGetData(uint8_t* pResponse, uint8_t* pResponseLen);
	/// TgSetData
	bool tgSetData(const uint8_t* pCommand, uint8_t CommandLen);
	/// InRelease
	bool inRelease();
	/// @}


private:
	/// @addtogroup gp_comm		Communicate to Device
	/// @ingroup gp_NfcPcd
	/// @{

	/// パケット送受信
	bool sendCmd(
			const uint8_t* pCommand, uint8_t CommandLen,
			uint8_t* pResponse, uint16_t* pResponseLen,
			bool bRecv=true);
	/// レスポンス受信
	bool recvResp(uint8_t* pResponse, uint16_t* pResponseLen, uint8_t CmdCode=0xff);
	/// ACK送信
	void sendAck();
	/// @}


private:
    bool m_bOpened;                             ///< オープンしているかどうか
    uint8_t m_NfcId3i[HkNfcRw::NFCID3_LEN];		///< NFCID3 for Initiator
    uint8_t m_NfcId3t[HkNfcRw::NFCID3_LEN];		///< NFCID3 for Target
};

#endif /* NFCPCD_H */
