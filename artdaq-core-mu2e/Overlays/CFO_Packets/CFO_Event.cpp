#include "artdaq-core-mu2e/Overlays/CFO_Packets/CFO_Event.h"

#include "artdaq-core-mu2e/Overlays/DTC_Types/Utilities.h"

#include "TRACE/tracemf.h"

#include <cstring>

// Copied structure from DTC_SubEvent.cpp

CFOLib::CFO_Event::CFO_Event(const void* data)
	: record_()
{
	memcpy(&record_, data, sizeof(record_));
}

// NOTE: do not need constructor for allocBytes because record_ is always a copy and memory is owned by instance

// NOTE: do not need SetupEvent because the CFO record is always received atomically in one buffer, and never spans multiple buffers

DTCLib::DTC_EventWindowTag CFOLib::CFO_Event::GetEventWindowTag() const
{
	return DTCLib::DTC_EventWindowTag(record_.event_tag);
}

DTCLib::DTC_EventMode CFOLib::CFO_Event::GetEventMode() const
{
	// DTC_EventMode is an aggregate: DTC_EventMode(event_mode) would be parenthesized aggregate
	// initialisation (C++20) and fill only mode0 with the low byte, leaving mode4 (on-spill, subrun
	// bits) and the delivery ring TDC at zero. Unpack the 48-bit record field byte by byte, same byte
	// order as the heartbeat packet.
	const uint64_t        eventModeBits = record_.event_mode;
	DTCLib::DTC_EventMode eventMode;
	eventMode.mode0           = static_cast<uint8_t>(eventModeBits & 0xFF);
	eventMode.mode1           = static_cast<uint8_t>((eventModeBits >> 8) & 0xFF);
	eventMode.mode2           = static_cast<uint8_t>((eventModeBits >> 16) & 0xFF);
	eventMode.mode3           = static_cast<uint8_t>((eventModeBits >> 24) & 0xFF);
	eventMode.mode4           = static_cast<uint8_t>((eventModeBits >> 32) & 0xFF);
	eventMode.deliveryRingTDC = static_cast<uint8_t>((eventModeBits >> 40) & 0xFF);
	return eventMode;
}

void CFOLib::CFO_Event::SetEventWindowTag(DTCLib::DTC_EventWindowTag const& tag)
{
	record_.event_tag = tag.GetEventWindowTag(true);
}

void CFOLib::CFO_Event::SetEventMode(DTCLib::DTC_EventMode const& mode)
{
	uint64_t mode_word = mode.mode0;
	mode_word += (static_cast<uint64_t>(mode.mode1) << 8);
	mode_word += (static_cast<uint64_t>(mode.mode2) << 16);
	mode_word += (static_cast<uint64_t>(mode.mode3) << 24);
	mode_word += (static_cast<uint64_t>(mode.mode4) << 32);
	mode_word += (static_cast<uint64_t>(mode.deliveryRingTDC) << 40);  // bits [47:40], read back by GetEventMode()

	record_.event_mode = mode_word;
}
