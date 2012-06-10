#ifndef MISC_H
#define MISC_H

#include <stdint.h>

namespace HkNfcRwMisc {

/// @addtogroup gp_utils	Utilities
/// @ingroup gp_NfcPcd
/// @{
/**
 * @brief	上位8bit取得
 *
 * 16bitの上位8bitを返す
 */
inline uint8_t h16(uint16_t u16) {
	return uint8_t(u16 >> 8);
}

/**
 *  @brief	下位8bit取得
 *
 * 16bitの下位8bitを返す
 */
inline uint8_t l16(uint16_t u16) {
	return uint8_t(u16 & 0x00ff);
}

/**
 *  @brief	8bitx2→16bit
 *
 * 16bit値の作成
 */
inline uint16_t hl16(uint8_t h8, uint8_t l8) {
	return uint16_t((h8 << 8) | l8);
}


/**
 *  @brief	ミリ秒スリープ
 *
 * @param	msec	待ち時間[msec]
 */
void msleep(uint16_t msec);

/// @}

}	//namespace

#endif // MISC_H
