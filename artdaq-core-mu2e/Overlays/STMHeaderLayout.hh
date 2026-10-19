#ifndef MU2E_ARTDAQ_CORE_OVERLAYS_STMHEADERLAYOUT_HH
#define MU2E_ARTDAQ_CORE_OVERLAYS_STMHEADERLAYOUT_HH

#include <cstddef>
#include <cstdint>

namespace stm
{

// ---------------------------
// Dataset identifiers
// ---------------------------

enum class Dataset : uint16_t
{
	RAW_HPGE       = 100,
	ZS_HPGE        = 101,
	PH_HPGE        = 102,
	CONTAINER_HPGE = 103,

	RAW_LABR       = 200,
	ZS_LABR        = 201,
	PH_LABR        = 202,
	CONTAINER_LABR = 203
};

// ---------------------------
// RAW header layout
// ---------------------------
struct RawHeader
{
	static constexpr size_t   WORDS       = 21;
	static constexpr uint16_t ANCHOR_WORD = 0xCAFE;

	enum Index : size_t
	{
		ANCHOR_START = 0,

		EWT_0 = 1,
		EWT_1 = 2,
		EWT_2 = 3,

		ADCclk_0 = 4,
		ADCclk_1 = 5,
		ADCclk_2 = 6,
		ADCclk_3 = 7,

		Ch_DTCclk_0 = 8,
		DTCclk_1    = 9,
		DTCclk_2    = 10,
		DTCclk_3    = 11,

		EM_0       = 12,
		EM_1       = 13,
		EM_2_DRTDC = 14,

		FLAGS      = 15,
		RAW_LEN    = 16,
		ZS_REGIONS = 17,
		ZS_LEN     = 18,
		PH_NUM     = 19,

		ANCHOR_END = 20
	};

	// ---------------------------
	// Static field accessors
	// ---------------------------

	static uint64_t eventWindowTag(int16_t const* d)
	{
		return uint64_t(uint16_t(d[EWT_0])) |
		       (uint64_t(uint16_t(d[EWT_1])) << 16) |
		       (uint64_t(uint16_t(d[EWT_2])) << 32);
	}

	static uint64_t adcClock(int16_t const* d)
	{
		return uint64_t(uint16_t(d[ADCclk_0])) |
		       (uint64_t(uint16_t(d[ADCclk_1])) << 16) |
		       (uint64_t(uint16_t(d[ADCclk_2])) << 32) |
		       (uint64_t(uint16_t(d[ADCclk_3])) << 48);
	}

	static uint64_t dtcClock(int16_t const* d)
	{
		return uint64_t(uint16_t(d[Ch_DTCclk_0]) >> 8) |
		       (uint64_t(uint16_t(d[DTCclk_1])) << 8) |
		       (uint64_t(uint16_t(d[DTCclk_2])) << 24) |
		       (uint64_t(uint16_t(d[DTCclk_3])) << 40);
	}

	static uint16_t channel(int16_t const* d)
	{
		return uint16_t(d[Ch_DTCclk_0]) & 0xFF;
	}

	static bool spillFlag(int16_t const* d)
	{
		return d[EM_2_DRTDC] & 0x1;
	}

	static bool subrunFlag(int16_t const* d)
	{
		return (d[EM_2_DRTDC] >> 1) & 0x1;
	}

	static bool rawPrescaled(int16_t const* d)
	{
		return (uint16_t(d[FLAGS]) >> 2) & 0x1;
	}

	static bool zsPrescaled(int16_t const* d)
	{
		return (uint16_t(d[FLAGS]) >> 3) & 0x1;
	}

	static uint16_t rawLength(int16_t const* d)
	{
		return uint16_t(d[RAW_LEN]);
	}

	static uint16_t zsRegions(int16_t const* d)
	{
		return uint16_t(d[ZS_REGIONS]);
	}

	static uint16_t zsLength(int16_t const* d)
	{
		return uint16_t(d[ZS_LEN]);
	}

	static uint16_t phCount(int16_t const* d)
	{
		return uint16_t(d[PH_NUM]);
	}

	static bool badData(int16_t const* d)
	{
		return d[FLAGS] & 0x1;
	}

	static bool missing(int16_t const* d)
	{
		return d[FLAGS] & 0x2;
	}

	static bool hasValidAnchors(int16_t const* d)
	{
		return uint16_t(d[ANCHOR_START]) == ANCHOR_WORD &&
		       uint16_t(d[ANCHOR_END]) == ANCHOR_WORD;
	}
};

// --------------------
// ZS Header Layout
// --------------------
// Information for one single pulse
struct ZSHeader
{
	static constexpr size_t WORDS = 2;
	enum Index : size_t
	{
		ZS_rawIndex    = 0,  // raw index value of the ZS
		ZS_pulseLength = 1,  // length of ZS
	};
};

}  // namespace stm

#endif  // MU2E_ARTDAQ_CORE_OVERLAYS_STMHEADERLAYOUT_HH
