/**
 * @file main.cpp
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

#include <stdio.h>
#include <stdint.h>

#include "hardware.h"

#include "networkh3emac.h"
#include "networkconst.h"
#include "ledblink.h"

#include "displayudf.h"
#include "displayudfparams.h"
#include "storedisplayudf.h"

#include "e131bridge.h"
#include "e131params.h"
#include "e131reboot.h"
#include "e131msgconst.h"
#include "lightset.h"

#include "ws28xxdmxparams.h"
#include "ws28xxdmxmulti.h"
#include "ws28xx.h"
#include "h3/ws28xxdmxstartstop.h"
#include "handleroled.h"
#include "storews28xxdmx.h"

// RDMNet LLRP Device Only
#include "rdmnetdevice.h"
#include "rdmpersonality.h"
#include "rdm_e120.h"
#include "factorydefaults.h"
#include "rdmdeviceparams.h"
#include "storerdmdevice.h"

#include "spiflashinstall.h"
#include "spiflashstore.h"
#include "storee131.h"
#include "remoteconfig.h"
#include "remoteconfigparams.h"
#include "storeremoteconfig.h"

#include "firmwareversion.h"
#include "software_version.h"

#include "displayudfnetworkhandler.h"
#include "displayhandler.h"

extern "C" {

void notmain(void) {
	Hardware hw;
	NetworkH3emac nw;
	LedBlink lb;
	DisplayUdf display;
	FirmwareVersion fw(SOFTWARE_VERSION, __DATE__, __TIME__);

	SpiFlashInstall spiFlashInstall;
	SpiFlashStore spiFlashStore;

	fw.Print();

	console_puts("Ethernet sACN E1.31 ");;
	console_set_fg_color(CONSOLE_GREEN);
	console_puts("Pixel controller {4x/8x 4 Universes}");
	console_set_fg_color(CONSOLE_WHITE);
	console_putc('\n');

	hw.SetLed(hardware::LedStatus::ON);
	hw.SetRebootHandler(new E131Reboot);
	lb.SetLedBlinkDisplay(new DisplayHandler);

	display.TextStatus(NetworkConst::MSG_NETWORK_INIT, Display7SegmentMessage::INFO_NETWORK_INIT, CONSOLE_YELLOW);

	nw.SetNetworkStore(StoreNetwork::Get());
	nw.SetNetworkDisplay(new DisplayUdfNetworkHandler);
	nw.Init(StoreNetwork::Get());
	nw.Print();

	display.TextStatus(E131MsgConst::PARAMS, Display7SegmentMessage::INFO_BRIDGE_PARMAMS, CONSOLE_YELLOW);

	E131Bridge bridge;

	StoreE131 storeE131;
	E131Params e131params(&storeE131);

	if (e131params.Load()) {
		e131params.Set(&bridge);
		e131params.Dump();
	}

	WS28xxDmxMulti ws28xxDmxMulti;
	WS28xxMulti::Get()->SetJamSTAPLDisplay(new HandlerOled);

	StoreWS28xxDmx storeWS28xxDmx;
	WS28xxDmxParams ws28xxparms(&storeWS28xxDmx);

	if (ws28xxparms.Load()) {
		ws28xxparms.Set(&ws28xxDmxMulti);
		ws28xxparms.Dump();
	}

	ws28xxDmxMulti.Initialize();
	ws28xxDmxMulti.SetLightSetHandler(new WS28xxDmxStartSop);

	bridge.SetDirectUpdate(true);
	bridge.SetOutput(&ws28xxDmxMulti);

	const auto nActivePorts = ws28xxDmxMulti.GetActivePorts();
	const auto nUniverseStart = e131params.GetUniverse();
	const auto nUniverses = ws28xxDmxMulti.GetUniverses();

	uint8_t nPortProtocolIndex = 0;

	for (uint32_t nOutportIndex = 0; nOutportIndex < nActivePorts; nOutportIndex++) {
		auto isSet = false;
		const auto nStartUniversePort = ws28xxparms.GetStartUniversePort(nOutportIndex, isSet);
		for (uint32_t u = 0; u < nUniverses; u++) {
			if (isSet) {
				bridge.SetUniverse(nPortProtocolIndex, E131_OUTPUT_PORT, nStartUniversePort + u);
			} else {
				bridge.SetUniverse(nPortProtocolIndex, E131_OUTPUT_PORT, nUniverseStart + nPortProtocolIndex);
			}
			nPortProtocolIndex++;
		}
	}

	auto bRunTestPattern = false;
	uint8_t nTestPattern;
	if ((nTestPattern = ws28xxparms.GetTestPattern()) != 0) {
		bRunTestPattern = true;
		ws28xxDmxMulti.SetTestPattern(static_cast<pixelpatterns::Pattern>(nTestPattern));
		ws28xxDmxMulti.Start(0);
		ws28xxDmxMulti.Blackout(true);
		bridge.SetOutput(nullptr);
	}

	StoreRDMDevice storeRdmDevice;
	RDMDeviceParams rdmDeviceParams(&storeRdmDevice);

	char aDescription[RDM_PERSONALITY_DESCRIPTION_MAX_LENGTH + 1];
	snprintf(aDescription, sizeof(aDescription) - 1, "sACN Pixel %d-%s:%d", ws28xxDmxMulti.GetActivePorts(), WS28xx::GetLedTypeString(ws28xxDmxMulti.GetLEDType()), ws28xxDmxMulti.GetLEDCount());

	char aLabel[RDM_DEVICE_LABEL_MAX_LENGTH + 1];
	const auto nLength = snprintf(aLabel, sizeof(aLabel) - 1, "Orange Pi Zero Pixel");

	RDMNetDevice llrpOnlyDevice(new RDMPersonality(aDescription, 0));

	llrpOnlyDevice.SetLabel(RDM_ROOT_DEVICE, aLabel, nLength);
	llrpOnlyDevice.SetRDMDeviceStore(&storeRdmDevice);
	llrpOnlyDevice.SetProductCategory(E120_PRODUCT_CATEGORY_FIXTURE);
	llrpOnlyDevice.SetProductDetail(E120_PRODUCT_DETAIL_ETHERNET_NODE);
	llrpOnlyDevice.SetRDMFactoryDefaults(new FactoryDefaults);

	if (rdmDeviceParams.Load()) {
		rdmDeviceParams.Set(&llrpOnlyDevice);
		rdmDeviceParams.Dump();
	}

	llrpOnlyDevice.Init();
	llrpOnlyDevice.Start();
	llrpOnlyDevice.Print();

	bridge.Print();
	ws28xxDmxMulti.Print();

	display.SetTitle("sACN Pixel %c:%dx%d", ws28xxDmxMulti.GetBoard() == ws28xxmulti::Board::X8 ? '8' : (ws28xxDmxMulti.GetBoard() == ws28xxmulti::Board::X4 ? '4' : ' '), ws28xxDmxMulti.GetActivePorts(), ws28xxDmxMulti.GetLEDCount());
	display.Set(2, DISPLAY_UDF_LABEL_HOSTNAME);
	display.Set(3, DISPLAY_UDF_LABEL_IP);
	display.Set(4, DISPLAY_UDF_LABEL_VERSION);
	display.Set(5, DISPLAY_UDF_LABEL_UNIVERSE);
	display.Set(6, DISPLAY_UDF_LABEL_BOARDNAME);
	display.Printf(7, "%d-%s:%d", ws28xxDmxMulti.GetActivePorts(), WS28xx::GetLedTypeString(ws28xxDmxMulti.GetLEDType()), ws28xxDmxMulti.GetLEDCount());

	StoreDisplayUdf storeDisplayUdf;
	DisplayUdfParams displayUdfParams(&storeDisplayUdf);

	if(displayUdfParams.Load()) {
		displayUdfParams.Set(&display);
		displayUdfParams.Dump();
	}

	display.Show(&bridge);

	RemoteConfig remoteConfig(remoteconfig::Node::E131, remoteconfig::Output::PIXEL, bridge.GetActiveOutputPorts());

	StoreRemoteConfig storeRemoteConfig;
	RemoteConfigParams remoteConfigParams(&storeRemoteConfig);

	if(remoteConfigParams.Load()) {
		remoteConfigParams.Set(&remoteConfig);
		remoteConfigParams.Dump();
	}

	while (spiFlashStore.Flash())
		;

	display.TextStatus(E131MsgConst::START, Display7SegmentMessage::INFO_BRIDGE_START, CONSOLE_YELLOW);

	bridge.Start();

	display.TextStatus(E131MsgConst::STARTED, Display7SegmentMessage::INFO_BRIDGE_STARTED, CONSOLE_GREEN);

	hw.WatchdogInit();

	for (;;) {
		hw.WatchdogFeed();
		nw.Run();
		bridge.Run();
		remoteConfig.Run();
		llrpOnlyDevice.Run();
		spiFlashStore.Flash();
		lb.Run();
		display.Run();
		if (__builtin_expect((bRunTestPattern), 0)) {
			ws28xxDmxMulti.RunTestPattern();
		}
	}
}

}
