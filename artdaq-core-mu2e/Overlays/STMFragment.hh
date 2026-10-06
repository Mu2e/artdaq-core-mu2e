#ifndef MU2E_ARTDAQ_CORE_OVERLAYS_STMFRAGMENT_HH
#define MU2E_ARTDAQ_CORE_OVERLAYS_STMFRAGMENT_HH

#include <cstddef>
#include <cstdint>
#include "artdaq-core/Data/Fragment.hh"

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
	static constexpr size_t   WORDS       = 22;
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

		PRESCALE   = 15,
		RAW_LEN    = 16,
		ZS_REGIONS = 17,
		ZS_LEN     = 18,
		PH_NUM     = 19,

		DATA_FLAGS = 20,

		ANCHOR_END = 21
	};
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

namespace mu2e
{

class STMFragment
{
  public:
	explicit STMFragment(artdaq::Fragment const& f)
	    : frag_(f)
	    , data_(reinterpret_cast<int16_t const*>(f.dataBegin()))
	{
	}

	// -----------------------
	// Dataset (authoritative)
	// -----------------------
	stm::Dataset dataset() const
	{
		return static_cast<stm::Dataset>(frag_.fragmentID());
	}

	bool isRaw() const
	{
		switch(dataset())
		{
		case stm::Dataset::RAW_HPGE:
		case stm::Dataset::RAW_LABR:
			return true;

		default:
			return false;
		}
	}

	bool isZS() const
	{
		switch(dataset())
		{
		case stm::Dataset::ZS_HPGE:
		case stm::Dataset::ZS_LABR:
			return true;

		default:
			return false;
		}
	}

	bool isPH() const
	{
		switch(dataset())
		{
		case stm::Dataset::PH_HPGE:
		case stm::Dataset::PH_LABR:
			return true;

		default:
			return false;
		}
	}

	bool isHPGe() const
	{
		switch(dataset())
		{
		case stm::Dataset::RAW_HPGE:
		case stm::Dataset::ZS_HPGE:
		case stm::Dataset::PH_HPGE:
			return true;

		default:
			return false;
		}
	}

	bool isLaBr() const
	{
		switch(dataset())
		{
		case stm::Dataset::RAW_LABR:
		case stm::Dataset::ZS_LABR:
		case stm::Dataset::PH_LABR:
			return true;

		default:
			return false;
		}
	}

	bool isHPGeContainer() const { return dataset() == stm::Dataset::CONTAINER_HPGE; }
	bool isLaBrContainer() const { return dataset() == stm::Dataset::CONTAINER_LABR; }

	bool isRaw_HPGe() const { return dataset() == stm::Dataset::RAW_HPGE; }
	bool isZS_HPGe() const { return dataset() == stm::Dataset::ZS_HPGE; }
	bool isPH_HPGe() const { return dataset() == stm::Dataset::PH_HPGE; }
	bool isRaw_LaBr() const { return dataset() == stm::Dataset::RAW_LABR; }
	bool isZS_LaBr() const { return dataset() == stm::Dataset::ZS_LABR; }
	bool isPH_LaBr() const { return dataset() == stm::Dataset::PH_LABR; }

	// -----------------------
	// Header integrity
	// -----------------------
	bool hasValidAnchors() const
	{
		if(!isRaw())
			return true;
		return static_cast<uint16_t>(data_[stm::RawHeader::ANCHOR_START]) == stm::RawHeader::ANCHOR_WORD &&
		       static_cast<uint16_t>(data_[stm::RawHeader::ANCHOR_END]) == stm::RawHeader::ANCHOR_WORD;
	}

	// -----------------------
	// RAW header access
	// -----------------------

	uint16_t channel() const
	{
		return data_[stm::RawHeader::Ch_DTCclk_0] & 0xFF;
	}

	uint64_t eventWindowTag() const
	{
		return uint64_t(uint16_t(data_[stm::RawHeader::EWT_0])) |
		       (uint64_t(uint16_t(data_[stm::RawHeader::EWT_1])) << 16) |
		       (uint64_t(uint16_t(data_[stm::RawHeader::EWT_2])) << 32);
	}

	uint64_t adcClock() const
	{
		return uint64_t(uint16_t(data_[stm::RawHeader::ADCclk_0])) |
		       (uint64_t(uint16_t(data_[stm::RawHeader::ADCclk_1])) << 16) |
		       (uint64_t(uint16_t(data_[stm::RawHeader::ADCclk_2])) << 32) |
		       (uint64_t(uint16_t(data_[stm::RawHeader::ADCclk_3])) << 48);
	}

	uint64_t dtcClock() const
	{
		return uint64_t((uint16_t(data_[stm::RawHeader::Ch_DTCclk_0]) >> 8)) |
		       (uint64_t(uint16_t(data_[stm::RawHeader::DTCclk_1])) << 8) |
		       (uint64_t(uint16_t(data_[stm::RawHeader::DTCclk_2])) << 24) |
		       (uint64_t(uint16_t(data_[stm::RawHeader::DTCclk_3])) << 40);
	}

	bool spillFlag() const
	{
		return data_[stm::RawHeader::EM_2_DRTDC] & 0x1;
	}

	uint16_t rawLength() const
	{
		return data_[stm::RawHeader::RAW_LEN];
	}

	bool rawPrescaled() const
	{
		return (data_[stm::RawHeader::PRESCALE] >> 15) & 0x1;
	}

	bool zsPrescaled() const
	{
		return (data_[stm::RawHeader::PRESCALE] >> 7) & 0x1;
	}

	uint8_t rawPrescaleValue() const
	{
		return (data_[stm::RawHeader::PRESCALE] >> 8) & 0x7F;
	}

	uint8_t zsPrescaleValue() const
	{
		return data_[stm::RawHeader::PRESCALE] & 0x7F;
	}

	uint16_t zsRegions() const
	{
		return data_[stm::RawHeader::ZS_REGIONS];
	}

	uint16_t zsLength() const
	{
		return data_[stm::RawHeader::ZS_LEN];
	}

	uint16_t phCount() const
	{
		return data_[stm::RawHeader::PH_NUM];
	}  // PH from Raw Header

	bool badData() const
	{
		return data_[stm::RawHeader::DATA_FLAGS] & 0x1;
	}

	bool missing() const
	{
		return data_[stm::RawHeader::DATA_FLAGS] & 0x2;
	}

	// ----------------
	// Full data (including header)
	// ----------------

	int16_t const* dataBegin() const
	{
		return data_;
	}

	size_t dataWords() const
	{
		return frag_.dataSizeBytes() / sizeof(int16_t);
	}

	//---------
	// ZS data
	//----------
	uint16_t zsIndex() const
	{
		return data_[stm::ZSHeader::ZS_rawIndex];
	}  // ZS index from ZS micro header

	uint16_t zsPulseLength() const
	{
		return data_[stm::ZSHeader::ZS_pulseLength];
	}  // single zs pulse length from ZS micro header

	// -----------------------
	// Payload access
	// -----------------------
	int16_t const* payloadBegin() const
	{
		return isRaw()  ? data_ + stm::RawHeader::WORDS
		       : isZS() ? data_ + stm::ZSHeader::WORDS
		                : data_;
	}

	size_t payloadWords() const
	{
		return isRaw()  ? rawLength()
		       : isZS() ? zsPulseLength()
		                : frag_.dataSizeBytes() / sizeof(int16_t);
	}

  private:
	artdaq::Fragment const& frag_;
	int16_t const*          data_;
};

}  // namespace mu2e

#endif  // MU2E_ARTDAQ_CORE_OVERLAYS_STMFRAGMENT_HH
