#ifndef MU2E_ARTDAQ_CORE_OVERLAYS_STMFRAGMENT_HH
#define MU2E_ARTDAQ_CORE_OVERLAYS_STMFRAGMENT_HH

#include "artdaq-core-mu2e/Overlays/STMHeaderLayout.hh"
#include "artdaq-core/Data/Fragment.hh"

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
		return stm::RawHeader::hasValidAnchors(data_);
	}

	// -----------------------
	// RAW header access
	// -----------------------

	// TODO: remove prescale value helpers
	// after updates in Offline/DAQ/src/STMPrintFragments_module.cc
	uint16_t channel() const { return stm::RawHeader::channel(data_); }
	uint64_t eventWindowTag() const { return stm::RawHeader::eventWindowTag(data_); }
	uint64_t adcClock() const { return stm::RawHeader::adcClock(data_); }
	uint64_t dtcClock() const { return stm::RawHeader::dtcClock(data_); }
	bool     spillFlag() const { return stm::RawHeader::spillFlag(data_); }
	bool     subrunFlag() const { return stm::RawHeader::subrunFlag(data_); }
	uint16_t rawLength() const { return stm::RawHeader::rawLength(data_); }
	bool     rawPrescaled() const { return stm::RawHeader::rawPrescaled(data_); }
	uint8_t  rawPrescaleValue() const { return 0; }
	bool     zsPrescaled() const { return stm::RawHeader::zsPrescaled(data_); }
	uint8_t  zsPrescaleValue() const { return 0; }
	uint16_t zsRegions() const { return stm::RawHeader::zsRegions(data_); }
	uint16_t zsLength() const { return stm::RawHeader::zsLength(data_); }
	uint16_t phCount() const { return stm::RawHeader::phCount(data_); }
	bool     badData() const { return stm::RawHeader::badData(data_); }
	bool     missing() const { return stm::RawHeader::missing(data_); }

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
