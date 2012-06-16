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
public:
	static const uint16_t DATA_MAX = 265;		///< データ部最大長

	static const uint8_t NFCID2_LEN = 8;		///< NFCID2サイズ
	static const uint8_t NFCID3_LEN = 10;		///< NFCID3サイズ


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

	/// @struct	DepInitiatorParam
	/// @brief	NFC-DEPイニシエータパラメータ
	struct DepInitiatorParam {
		ActPass Ap;						///< Active/Passive
		BaudRate Br;					///< 通信速度
		const uint8_t*		pNfcId3;	///< NFCID3(不要ならnull)
		const uint8_t*		pGt;		///< GeneralBytes(GtLenが0:未使用)
		uint8_t				GtLen;		///< pGtサイズ(不要なら0)
	};
	
	/// @struct	TargetParam
	/// @brief	ターゲットパラメータ
	struct TargetParam {
//		uint16_t			SystemCode;	///< [NFC-F]システムコード
//		const uint8_t*		pUId;		///< [NFC-A]UID(3byte)。
//										///  UID 4byteの先頭は強制的に0x04。
//										///  null時には乱数生成する。
//		const uint8_t*		pIDm;		///< [NFC-F]IDm
//										///  null時には6byteを乱数生成する(先頭は01fe)。
		const uint8_t*		pGt;		///< GeneralBytes(GtLenが0:未使用)
		uint8_t				GtLen;		///< pGtサイズ(不要なら0)
	};

public:
	static NfcPcd* getInstance();

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


private:
	NfcPcd();
	virtual ~NfcPcd();

public:
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
	/// WriteRegister
	bool writeRegister(const uint8_t* pCommand, uint8_t CommandLen);
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
	bool communicateThruEx();
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
private:
	bool _inJump(uint8_t Cmd,
			const DepInitiatorParam* pParam);

public:
	/// InJumpForDEP
	bool inJumpForDep(
			const DepInitiatorParam* pParam);
	/// InJumpForPSL
	bool inJumpForPsl(
			const DepInitiatorParam* pParam);
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
			const TargetParam* pParam,
			uint8_t* pResponse=0, uint8_t* pResponseLen=0);
	/// TgSetGeneralBytes
	bool tgSetGeneralBytes(const TargetParam* pParam);
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
			const uint8_t* pCommand, uint16_t CommandLen,
			uint8_t* pResponse, uint16_t* pResponseLen,
			bool bRecv=true);
	/// レスポンス受信
	bool recvResp(uint8_t* pResponse, uint16_t* pResponseLen, uint8_t CmdCode=0xff);
	/// ACK送信
	void sendAck();
	/// @}


private:
	bool m_bOpened;                             ///< オープンしているかどうか

    friend class HkNfcRw;
	friend class HkNfcA;
	friend class HkNfcB;
	friend class HkNfcF;

};

#endif /* NFCPCD_H */
