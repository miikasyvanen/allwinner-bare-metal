/**
 * @file ws28xxmulti.h
 *
 */
/* Copyright (C) 2019-2021 by Arjan van Vught mailto:info@orangepi-dmx.nl
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef WS28XXMULTI_H_
#define WS28XXMULTI_H_

#include <stdint.h>

#include "ws28xx.h"

#if defined (H3)
# include "h3_spi.h"
#endif

#include "rgbmapping.h"

namespace ws28xxmulti {
enum class Board {
	X4, X8, UNKNOWN
};
namespace defaults {
static constexpr auto BOARD = Board::X4;
}  // namespace defaults
}  // namespace ws28xxmulti

struct JamSTAPLDisplay;

class WS28xxMulti {
public:
	WS28xxMulti();
	~WS28xxMulti();

	void Initialize(ws28xx::Type tWS28xxType, uint16_t nLedCount, rgbmapping::Map tRGBMapping, uint8_t nT0H, uint8_t nT1H, bool bUseSI5351A = false);

	ws28xx::Type GetLEDType() const {
		return m_tWS28xxType;
	}

	rgbmapping::Map GetRgbMapping() const {
		return m_tRGBMapping;
	}

	uint8_t GetLowCode() const {
		return m_nLowCode;
	}

	uint8_t GetHighCode() const {
		return m_nHighCode;
	}

	uint16_t GetLEDCount() const {
		return m_nLedCount;
	}

	ws28xxmulti::Board GetBoard() const {
		return m_tBoard;
	}

	void SetLED(uint8_t nPort, uint16_t nLedIndex, uint8_t nRed, uint8_t nGreen, uint8_t nBlue) {
		if (m_tBoard == ws28xxmulti::Board::X8) {
			SetLED8x(nPort, nLedIndex, nRed, nGreen, nBlue);
		} else {
			SetLED4x(nPort, nLedIndex, nRed, nGreen, nBlue);
		}
	}
	void SetLED(uint8_t nPort, uint16_t nLedIndex, uint8_t nRed, uint8_t nGreen, uint8_t nBlue, uint8_t nWhite) {
		if (m_tBoard == ws28xxmulti::Board::X8) {
			SetLED8x(nPort, nLedIndex, nRed, nGreen, nBlue, nWhite);
		} else {
			SetLED4x(nPort, nLedIndex, nRed, nGreen, nBlue, nWhite);
		}
	}

#if defined (H3)
	bool IsUpdating() {
		if (m_tBoard == ws28xxmulti::Board::X8) {
			return h3_spi_dma_tx_is_active();  // returns TRUE while DMA operation is active
		} else {
			return false;
		}
	}
#else
	bool IsUpdating(void) {
		return false;
	}
#endif

	void Update();
	void Blackout();

// 8x
	void SetJamSTAPLDisplay(JamSTAPLDisplay *pJamSTAPLDisplay) {
		m_pJamSTAPLDisplay = pJamSTAPLDisplay;
	}

	static WS28xxMulti *Get() {
		return s_pThis;
	}

private:
	uint8_t ReverseBits(uint8_t nBits);
// 4x
	bool IsMCP23017();
	bool SetupMCP23017(uint8_t nT0H, uint8_t nT1H);
	bool SetupSI5351A();
	void SetupGPIO();
	void SetupBuffers4x();
	void Generate800kHz(const uint32_t *pBuffer);
	void SetColour4x(uint8_t nPort, uint16_t nLedIndex, uint8_t nColour1, uint8_t nColour2, uint8_t nColour3);
	void SetLED4x(uint8_t nPort, uint16_t nLedIndex, uint8_t nRed, uint8_t nGreen, uint8_t nBlue);
	void SetLED4x(uint8_t nPort, uint16_t nLedIndex, uint8_t nRed, uint8_t nGreen, uint8_t nBlue, uint8_t nWhite);
// 8x
	void SetupHC595(uint8_t nT0H, uint8_t nT1H);
	void SetupSPI();
	void SetupCPLD();
	void SetupBuffers8x();
	void SetColour8x(uint8_t nPort, uint16_t nLedIndex, uint8_t nColour1, uint8_t nColour2, uint8_t nColour3);
	void SetLED8x(uint8_t nPort, uint16_t nLedIndex, uint8_t nRed, uint8_t nGreen, uint8_t nBlue);
	void SetLED8x(uint8_t nPort, uint16_t nLedIndex, uint8_t nRed, uint8_t nGreen, uint8_t nBlue, uint8_t nWhite);

private:
	ws28xxmulti::Board m_tBoard { ws28xxmulti::defaults::BOARD };
	ws28xx::Type m_tWS28xxType { ws28xx::defaults::TYPE };
	uint16_t m_nLedCount { ws28xx::defaults::LED_COUNT };
	rgbmapping::Map m_tRGBMapping { rgbmapping::Map::UNDEFINED };
	uint8_t m_nLowCode { 0 };
	uint8_t m_nHighCode { 0 };
	uint32_t m_nBufSize { 0 };
	uint32_t *m_pBuffer4x { nullptr };
	uint32_t *m_pBlackoutBuffer4x { nullptr };
	uint8_t *m_pBuffer8x { nullptr };
	uint8_t *m_pBlackoutBuffer8x { nullptr };
	JamSTAPLDisplay *m_pJamSTAPLDisplay { nullptr };

	static WS28xxMulti *s_pThis;
};

#endif /* WS28XXMULTI_H_ */
