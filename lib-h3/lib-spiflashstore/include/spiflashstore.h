/**
 * @file spiflashstore.h
 *
 */
/* Copyright (C) 2018-2021 by Arjan van Vught mailto:info@orangepi-dmx.nl
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

#ifndef SPIFLASHSTORE_H_
#define SPIFLASHSTORE_H_

#include <stdint.h>

#if !defined( NO_EMAC )
# include "storenetwork.h"
#endif

namespace spiflashstore {
enum class Store {
	NETWORK,
	ARTNET,
	DMXSEND,
	WS28XXDMX,
	E131,
	LTC,
	MIDI,
	ARTNET4,
	OSC,
	TLC5711DMX,
	WIDGET,
	RDMDEVICE,
	RCONFIG,
	TCNET,
	OSC_CLIENT,
	DISPLAYUDF,
	LTCDISPLAY,
	MONITOR,
	SPARKFUN,
	SLUSH,
	MOTORS,
	SHOW,
	SERIAL,
	RDMSENSORS,
	RDMSUBDEVICES,
	GPS,
	RGBPANEL,
	LAST
};
}  // namespace spiflashstore

class SpiFlashStore {
public:
	SpiFlashStore();
	~SpiFlashStore();

	 bool HaveFlashChip() const {
		return m_bHaveFlashChip;
	}

	void Update(spiflashstore::Store tStore, uint32_t nOffset, const void *pData, uint32_t nDataLength, uint32_t nSetList = 0, uint32_t nOffsetSetList = 0);
	void Update(spiflashstore::Store tStore, const void *pData, uint32_t nDataLength) {
		Update(tStore, 0, pData, nDataLength);
	}
	void Copy(spiflashstore::Store tStore, void *pData, uint32_t nDataLength, uint32_t nOffset = 0);
	void CopyTo(spiflashstore::Store tStore, void *pData, uint32_t &nDataLength);

	void ResetSetList(spiflashstore::Store tStore);

	bool Flash();

	void Dump();

	static SpiFlashStore *Get() {
		return s_pThis;
	}

private:
	bool Init();
	uint32_t GetStoreOffset(spiflashstore::Store tStore);

private:
	bool m_bHaveFlashChip { false };
	bool m_bIsNew { false };
	enum class State {
		IDLE, CHANGED, ERASED
	};
	State m_tState { State::IDLE };
	uint32_t m_nStartAddress { 0 };
	struct FlashStore {
		static constexpr auto SIZE = 4096;
	};
	uint32_t m_nSpiFlashStoreSize { FlashStore::SIZE };
	uint8_t m_aSpiFlashData[FlashStore::SIZE];

#if !defined( NO_EMAC )
	StoreNetwork m_StoreNetwork;
#endif

	static SpiFlashStore *s_pThis;
};

#endif /* SPIFLASHSTORE_H_ */
