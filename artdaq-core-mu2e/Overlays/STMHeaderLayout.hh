#ifndef MU2E_ARTDAQ_CORE_OVERLAYS_STMHEADERLAYOUT_HH
#define MU2E_ARTDAQ_CORE_OVERLAYS_STMHEADERLAYOUT_HH

#include <cstddef>
#include <cstdint>

namespace stm {

// ---------------------------
// Dataset identifiers
// ---------------------------

enum class Dataset : uint16_t
{
	RAW_HPGE = 100,
	ZS_HPGE = 101,
	PH_HPGE = 102,
	CONTAINER_HPGE = 103,

	RAW_LABR = 200,
	ZS_LABR = 201,
	PH_LABR = 202,
	CONTAINER_LABR = 203
};

// ---------------------------
// RAW header layout
// ---------------------------
struct RawHeader
{
	static constexpr size_t WORDS = 21;
	static constexpr uint16_t ANCHOR_WORD = 0xCAFE;

	// Header format version, stored in the top nibble of FLAGS.
	//   VERSION_LEGACY : Old 22-word header
	//   0              : Current 21-word header
	// Increment CURRENT_VERSION whenever the layout changes so that
	static constexpr uint16_t VERSION_SHIFT = 12;
	static constexpr uint16_t VERSION_MASK = 0xF;
	static constexpr uint16_t CURRENT_VERSION = 0;
	static constexpr uint16_t VERSION_LEGACY = 0xFFFF;  // outside nibble range

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
		DTCclk_1 = 9,
		DTCclk_2 = 10,
		DTCclk_3 = 11,

		EM_0 = 12,
		EM_1 = 13,
		EM_2_DRTDC = 14,

		FLAGS = 15,  // bit0 badData, bit1 missing, bit2 rawPrescaled, bit3 zsPrescaled, bits12-15 version
		RAW_LEN = 16,
		ZS_REGIONS = 17,
		ZS_LEN = 18,
		PH_NUM = 19,

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

	// Version nibble only; assumes a 21-word header.  Prefer headerVersion().
	static uint16_t version(int16_t const* d)
	{
		return (uint16_t(d[FLAGS]) >> VERSION_SHIFT) & VERSION_MASK;
	}

	// Full version lookup: returns VERSION_LEGACY for a 22-word header,
	// otherwise the version from FLAGS.
	static inline uint16_t headerVersion(int16_t const* d, size_t nWords);

	// Compose a FLAGS word; used by emulators / writers.
	static uint16_t makeFlags(bool badData, bool missing, bool rawPrescaled, bool zsPrescaled,
							  uint16_t version = CURRENT_VERSION)
	{
		return uint16_t((badData ? 0x1 : 0) | (missing ? 0x2 : 0) |
						(rawPrescaled ? 0x4 : 0) | (zsPrescaled ? 0x8 : 0) |
						((version & VERSION_MASK) << VERSION_SHIFT));
	}

	static bool hasValidAnchors(int16_t const* d)
	{
		return uint16_t(d[ANCHOR_START]) == ANCHOR_WORD &&
			   uint16_t(d[ANCHOR_END]) == ANCHOR_WORD;
	}
};

// ---------------------------
// Legacy RAW header layout (22 words)
// ---------------------------
// Word 15 layout:  bit15 rawPrescaled, bits14-8 rawPrescaleValue,
//                  bit7  zsPrescaled,  bits6-0  zsPrescaleValue.
// Detected by the end-anchor position.
struct RawHeaderLegacy
{
	static constexpr size_t WORDS = 22;
	static constexpr uint16_t ANCHOR_WORD = RawHeader::ANCHOR_WORD;

	enum Index : size_t
	{
		PRESCALE = 15,
		DATA_FLAGS = 20,
		ANCHOR_END = 21
	};

	static bool rawPrescaled(int16_t const* d)
	{
		return (uint16_t(d[PRESCALE]) >> 15) & 0x1;
	}

	static uint8_t rawPrescaleValue(int16_t const* d)
	{
		return (uint16_t(d[PRESCALE]) >> 8) & 0x7F;
	}

	static bool zsPrescaled(int16_t const* d)
	{
		return (uint16_t(d[PRESCALE]) >> 7) & 0x1;
	}

	static uint8_t zsPrescaleValue(int16_t const* d)
	{
		return uint16_t(d[PRESCALE]) & 0x7F;
	}

	static bool badData(int16_t const* d)
	{
		return d[DATA_FLAGS] & 0x1;
	}

	static bool missing(int16_t const* d)
	{
		return d[DATA_FLAGS] & 0x2;
	}

	static bool hasValidAnchors(int16_t const* d)
	{
		return uint16_t(d[RawHeader::ANCHOR_START]) == ANCHOR_WORD &&
			   uint16_t(d[ANCHOR_END]) == ANCHOR_WORD;
	}
};

inline uint16_t RawHeader::headerVersion(int16_t const* d, size_t nWords)
{
	if (nWords > RawHeader::ANCHOR_END &&
		uint16_t(d[RawHeader::ANCHOR_END]) == RawHeader::ANCHOR_WORD)
		return RawHeader::version(d);
	if (nWords > RawHeaderLegacy::ANCHOR_END &&
		uint16_t(d[RawHeaderLegacy::ANCHOR_END]) == RawHeaderLegacy::ANCHOR_WORD)
		return RawHeader::VERSION_LEGACY;
	return nWords > RawHeader::FLAGS ? RawHeader::version(d) : 0;
}

// --------------------
// ZS Header Layout
// --------------------
struct ZSHeader
{
	static constexpr size_t WORDS = 2;
	enum Index : size_t
	{
		ZS_rawIndex = 0,     // raw index value of the ZS
		ZS_pulseLength = 1,  // length of ZS
	};
};

}  // namespace stm

#endif  // MU2E_ARTDAQ_CORE_OVERLAYS_STMHEADERLAYOUT_HH
