#ifndef artdaq_core_mu2e_Overlays_DTC_Types_DTC_EVBStatsType_h
#define artdaq_core_mu2e_Overlays_DTC_Types_DTC_EVBStatsType_h

#include <cstdint>  // uint8_t, uint16_t

#define DTC_EVBStatsType_BRAM_TYPE_SIZE  5  /* 5-bits to handle stats from 32 DTCs */
#define DTC_EVBStatsType_BRAM_TYPE_COUNT 10 /* 10 defined types enumerated below */

namespace DTCLib
{
// clang-format off
enum DTC_EVBStatsType : uint8_t /* in hardware, defined as 4 bits*/
{
	DTC_EVBStatsType_RxCount 				= 0x0,
	DTC_EVBStatsType_RxLastSequenceTag 		= 0x1,
	DTC_EVBStatsType_RxMissingPacketCount 	= 0x2,
	DTC_EVBStatsType_RxByteCount 			= 0x3,
	DTC_EVBStatsType_RxLastPacketArrival 	= 0x4,
	DTC_EVBStatsType_TxLastSequenceTag 		= 0x5,
	DTC_EVBStatsType_TravelTime 			= 0x6,
	DTC_EVBStatsType_TxIdleCount 			= 0x7,
	DTC_EVBStatsType_RxIdleCount 			= 0x8,
	DTC_EVBStatsType_TxPacketCount 			= 0x9,

	DTC_EVBStatsType_All 					= 0xff /* not defined in hardware, so 8 bits */
};
// clang-format on

}  // namespace DTCLib

#endif  // artdaq_core_mu2e_Overlays_DTC_Types_DTC_EVBStatsType_h
