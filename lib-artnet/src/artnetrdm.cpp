/**
 * @file artnetrdm.h
 *
 */
/**
 * Art-Net Designed by and Copyright Artistic Licence Holdings Ltd.
 */
/* Copyright (C) 2017-2021 by Arjan van Vught mailto:info@orangepi-dmx.nl
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

#include <string.h>
#include <stdio.h>
#include <cassert>

#include "artnetrdm.h"

#include "artnetnode.h"
#include "network.h"

#include "artnetnode_internal.h"

#include "debug.h"

void ArtNetNode::HandleTodControl() {
	DEBUG_ENTRY

	const struct TArtTodControl *pArtTodControl =  &(m_ArtNetPacket.ArtPacket.ArtTodControl);
	const auto portAddress = static_cast<uint16_t>((pArtTodControl->Net << 8)) | static_cast<uint16_t>((pArtTodControl->Address));

	for (uint32_t i = 0; i < ArtNet::MAX_PORTS; i++) {
		if ((portAddress == m_OutputPorts[i].port.nPortAddress) && m_OutputPorts[i].bIsEnabled) {
			if (m_IsLightSetRunning[i] && (!m_IsRdmResponder)) {
				m_pLightSet->Stop(i);
			}

			if (pArtTodControl->Command == 0x01) {	// AtcFlush
				m_pArtNetRdm->Full(i);
			}

			SendTod(i);

			if (m_IsLightSetRunning[i] && (!m_IsRdmResponder)) {
				m_pLightSet->Start(i);
			}
		}
	}

	DEBUG_EXIT
}

void ArtNetNode::HandleTodRequest() {
	DEBUG_ENTRY

	const struct TArtTodRequest *pArtTodRequest = &(m_ArtNetPacket.ArtPacket.ArtTodRequest);
	const auto portAddress = static_cast<uint16_t>((pArtTodRequest->Net << 8)) | static_cast<uint16_t>((pArtTodRequest->Address[0]));

	for (uint32_t i = 0; i < ArtNet::MAX_PORTS; i++) {
		if ((portAddress == m_OutputPorts[i].port.nPortAddress) && m_OutputPorts[i].bIsEnabled) {
			SendTod(i);
		}
	}

	DEBUG_EXIT
}

void ArtNetNode::SendTod(uint8_t nPortId) {
	DEBUG_ENTRY

	assert(nPortId < ArtNet::MAX_PORTS);

	m_pTodData->Net = m_Node.NetSwitch[0];
	m_pTodData->Address = m_OutputPorts[nPortId].port.nDefaultAddress;

	const auto discovered = m_pArtNetRdm->GetUidCount(nPortId);

	m_pTodData->UidTotalHi = 0;
	m_pTodData->UidTotalLo = discovered;
	m_pTodData->BlockCount = 0;
	m_pTodData->UidCount = discovered;
	m_pTodData->Port = 1 + nPortId;

	m_pArtNetRdm->Copy(nPortId, reinterpret_cast<uint8_t*>(m_pTodData->Tod));

	const auto nLength = sizeof(struct TArtTodData) - (sizeof m_pTodData->Tod) + (discovered * 6U);

	Network::Get()->SendTo(m_nHandle, m_pTodData, nLength, m_Node.IPAddressBroadcast, ArtNet::UDP_PORT);

	DEBUG_EXIT
}

void ArtNetNode::SetRdmHandler(ArtNetRdm *pArtNetTRdm, bool IsResponder) {
	DEBUG_ENTRY

	assert(pArtNetTRdm != nullptr);

	if (pArtNetTRdm != nullptr) {
		m_pArtNetRdm = pArtNetTRdm;
		m_IsRdmResponder = IsResponder;

		m_pTodData = new TArtTodData;
		assert(m_pTodData != nullptr);

		if (m_pTodData != nullptr) {
			m_Node.Status1 |= STATUS1_RDM_CAPABLE;
			memset(m_pTodData, 0, sizeof(struct TArtTodData));

			memcpy(m_pTodData->Id, artnet::NODE_ID, sizeof(m_pTodData->Id));
			m_pTodData->OpCode = OP_TODDATA;
			m_pTodData->ProtVerLo = ArtNet::PROTOCOL_REVISION;
			m_pTodData->RdmVer = 0x01; // Devices that support RDM STANDARD V1.0 set field to 0x01.
		}
	}

	DEBUG_EXIT
}

void ArtNetNode::HandleRdm() {
	DEBUG_ENTRY

	auto *pArtRdm = &(m_ArtNetPacket.ArtPacket.ArtRdm);
	const auto portAddress = static_cast<uint16_t>((pArtRdm->Net << 8)) | static_cast<uint16_t>((pArtRdm->Address));

	for (uint32_t i = 0; i < ArtNet::MAX_PORTS; i++) {
		if ((portAddress == m_OutputPorts[i].port.nPortAddress) && m_OutputPorts[i].bIsEnabled) {
			if (!m_IsRdmResponder) {
				if ((m_OutputPorts[i].tPortProtocol == PORT_ARTNET_SACN) && (m_pArtNet4Handler != nullptr)) {
					const uint8_t nMask = GO_OUTPUT_IS_MERGING | GO_DATA_IS_BEING_TRANSMITTED | GO_OUTPUT_IS_SACN;
					m_IsLightSetRunning[i] = (m_pArtNet4Handler->GetStatus(i) & nMask) != 0;
				}

				if (m_IsLightSetRunning[i]) {
					m_pLightSet->Stop(i); // Stop DMX if was running
				}

			}

			const auto *pRdmResponse = const_cast<uint8_t*>(m_pArtNetRdm->Handler(i, pArtRdm->RdmPacket));

			if (pRdmResponse != nullptr) {
				pArtRdm->RdmVer = 0x01;

				const uint16_t nMessageLength = pRdmResponse[2] + 1;
				memcpy(pArtRdm->RdmPacket, &pRdmResponse[1], nMessageLength);

				const auto nLength = sizeof(struct TArtRdm) - sizeof(pArtRdm->RdmPacket) + nMessageLength;

				Network::Get()->SendTo(m_nHandle, pArtRdm, nLength, m_ArtNetPacket.IPAddressFrom, ArtNet::UDP_PORT);
			} else {
				printf("No RDM response\n");
			}

			if (m_IsLightSetRunning[i] && (!m_IsRdmResponder)) {
				m_pLightSet->Start(i); // Start DMX if was running
			}
		}
	}

	DEBUG_EXIT
}
