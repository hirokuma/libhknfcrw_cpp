#ifndef QHKNFCRW_H
#define QHKNFCRW_H

#include <stdint.h>
#include <cstring>


/**
 * @class	HkNfcRw
 * @brief	NFCのR/W(Sony RC-S956系)アクセスクラス
 */
class HkNfcRw {
public:
	/**
	 * @enum	Type
	 * @brief	Polling時のNFCカードタイプ
	 */
	enum Type {
		NFC_NONE,		///< 未設定
		NFC_A,			///< NFC-A
		NFC_B,			///< NFC-B
        NFC_F			///< NFC-F
	};

	/// 最大UID長(NFC-Bでの値)
	static const uint8_t NFCID2_LEN = 8;				///< NFCID2サイズ
	static const uint8_t NFCID3_LEN = 10;				///< NFCID3サイズ
	static const uint8_t MAX_NFCID_LEN = 12;

	static const uint16_t CARD_COMMAND_LEN = 265;		///< コマンドバッファサイズ
	static const uint16_t CARD_RESPONSE_LEN = 265;		///< レスポンスバッファサイズ


public:

	/// 選択解除
	static void release();
#if 0	//ここはNDEF対応してからじゃないと意味がなさそうだな
	/// データ読み込み
	virtual bool read(uint8_t* buf, uint8_t blockNo=0x00) { return false; }
	/// データ書き込み
	virtual bool write(const uint8_t* buf, uint8_t blockNo=0x00) { return false; }
#endif

public:
	/// オープン
	static bool open();
	/// クローズ
	static void close();

public:
	/// @addtogroup gp_CardDetect	カード探索
	/// @{

	/// ターゲットの探索
	static Type detect(bool bNfcA, bool bNfcB, bool bNfcF);

	/**
	 * @brief UIDの取得
	 *
	 * UIDを取得する
	 *
	 * @param[out]	pBuf		UID値
	 * @return					pBufの長さ
	 *
	 * @note		- 戻り値が0の場合、UIDは未取得
	 */
	static uint8_t getNfcId(uint8_t* pBuf) {
		std::memcpy(pBuf, m_NfcId, m_NfcIdLen);
		return m_NfcIdLen;
	}

	/// デバッグ用
	static void debug_nfcid();

	/**
	 * @brief NFCタイプの取得
	 *
	 * NFCタイプを取得する
	 *
	 * @retval		Nfc::Type 参照
	 */
	static Type getType() {
		return m_Type;
	}
	/// @}


private:
	static uint8_t* commandBuf() { return s_CommandBuf; }
	static uint8_t* responseBuf() { return s_ResponseBuf; }


private:
	static Type			m_Type;				///< アクティブなNFCタイプ
	static uint8_t		s_CommandBuf[CARD_COMMAND_LEN];		///< PCDへの送信バッファ
	static uint8_t		s_ResponseBuf[CARD_RESPONSE_LEN];	///< PCDからの受信バッファ
	static uint8_t		m_NfcId[MAX_NFCID_LEN];		///< 取得したNFCID
	static uint8_t		m_NfcIdLen;					///< 取得済みのNFCID長。0の場合は未取得。


	friend class HkNfcA;
	friend class HkNfcB;
	friend class HkNfcF;
	friend class HkNfcDep;
};

#endif // QHKNFCRW_H
