#ifndef ARTDAQ_CORE_MU2E_DATA_CALORIMETERDATADECODER_HH
#define ARTDAQ_CORE_MU2E_DATA_CALORIMETERDATADECODER_HH

#include "artdaq-core-mu2e/Overlays/Decoders/DTCDataDecoder.hh"

#include <messagefacility/MessageLogger/MessageLogger.h>  // Putting this here so that Offline/DAQ/src/FragmentAna_module.cc can use it
#include <memory>

namespace mu2e
{
class CalorimeterDataDecoder : public DTCDataDecoder
{
  public:
	CalorimeterDataDecoder(DTCLib::DTC_SubEvent const& f);

	// Class to swap pairs of 16-bit words and extract 12-bit words without memory buffers
	class Data12bitReader
	{
	  private:
		const uint16_t* dataPtr;
		bool            debugPacket;

	  public:
		Data12bitReader(const uint16_t* dataPtr, bool debug = false)
		    : dataPtr(dataPtr)
		    , debugPacket(debug) {}

		uint16_t operator[](size_t index) const
		{
			uint firstWordIndex;
			uint startingLetter;

			if(debugPacket)
			{
				int wordInTwoPackets = index % 21;
				int nTwoPackets      = (index - wordInTwoPackets) / 21;
				firstWordIndex       = nTwoPackets * 16 + ((wordInTwoPackets * 3) / 4);
				startingLetter       = wordInTwoPackets % 4;
			}
			else
			{
				firstWordIndex = (index * 3) / 4;
				startingLetter = index % 4;
			}

			// Find the two 16-bit words relative to this 12-bit word (swap their position first)
			uint16_t word1 = dataPtr[firstWordIndex ^ 0x1];
			uint16_t word2 = dataPtr[(firstWordIndex + 1) ^ 0x1];

			uint16_t temp;
			switch(startingLetter)
			{
			case 0:  // FFF0
				temp = (word1 >> 4) & 0x0FFF;
				break;
			case 1:  // 000F FF00
				temp = ((word1 & 0x000F) << 8) | ((word2 & 0xFF00) >> 8);
				break;
			case 2:  // 00FF F000
				temp = ((word1 & 0x00FF) << 4) | ((word2 & 0xF000) >> 12);
				break;
			case 3:  // 0FFF
				temp = word1 & 0x0FFF;
				break;
			}

			return temp;
		}
	};

	struct CalorimeterHitDataPacket
	{
		uint16_t Reserved1 : 12;
		uint16_t BoardID : 8;
		uint16_t DetectorID : 3;
		uint16_t ChannelID : 5;
		uint16_t Time : 16;
		uint16_t InPayloadEventWindowTag : 16;
		uint16_t Baseline : 12;
		uint16_t IndexOfMaxDigitizerSample : 10;
		uint16_t ErrorFlags : 4;
		uint16_t NumberOfSamples : 10;

		CalorimeterHitDataPacket()
		    : Reserved1(0)
		    , BoardID(0)
		    , DetectorID(0)
		    , ChannelID(0)
		    , Time(0)
		    , InPayloadEventWindowTag(0)
		    , Baseline(0)
		    , IndexOfMaxDigitizerSample(0)
		    , ErrorFlags(0)
		    , NumberOfSamples(0) {}

		uint32_t extractBits(const uint16_t* words, size_t startBit, size_t bitLength)
		{
			uint32_t result = 0;
			for(size_t bitIndex = startBit; bitIndex < startBit + bitLength; bitIndex++)
			{
				size_t wordIndex = (bitIndex / 16) ^ 0x1;  // Swap pairs of 16-bit words (just flip the last bit)
				size_t bitOffset = 15 - (bitIndex % 16);   // Big-endian

				uint16_t bit = (words[wordIndex] >> bitOffset) & 0x1;
				result       = (result << 1) | bit;
			}
			return result;
		}

		void mapFromRaw(const uint16_t* words)
		{
			// clang-format off
			Reserved1                 = static_cast<uint16_t>(extractBits(words, 0, 12));
			BoardID                   = static_cast<uint8_t>(extractBits(words, 12, 8));
			DetectorID                = static_cast<uint8_t>(extractBits(words, 20, 3));
			ChannelID                 = static_cast<uint8_t>(extractBits(words, 23, 5));
			Time                      = static_cast<uint16_t>(extractBits(words, 28, 16));
			InPayloadEventWindowTag   = static_cast<uint16_t>(extractBits(words, 44, 16));
			Baseline                  = static_cast<uint16_t>(extractBits(words, 60, 12));
			IndexOfMaxDigitizerSample = static_cast<uint16_t>(extractBits(words, 72, 10));
			ErrorFlags                = static_cast<uint8_t>(extractBits(words, 82, 4));
			NumberOfSamples           = static_cast<uint16_t>(extractBits(words, 86, 10));
			// clang-format on
		}
	};

	struct CalorimeterHitTestDataPacket
	{
		uint16_t BeginMarker : 12;  // 0xAAA
		uint16_t BoardID : 12;      // unique board ID from 0 - 255
		uint16_t ChannelID : 12;    // channel ID from 0-19
		uint16_t InPayloadEventWindowTag : 12;

		uint16_t LastSampleMarker : 12;  // 0xFFF
		uint16_t ErrorFlags : 12;
		uint32_t Time : 24;
		uint16_t IndexOfMaxDigitizerSample : 12;
		uint16_t NumberOfSamples : 12;

		CalorimeterHitTestDataPacket()
		    : BeginMarker(0)
		    , BoardID(0)
		    , ChannelID(0)
		    , InPayloadEventWindowTag(0)
		    , LastSampleMarker(0)
		    , ErrorFlags(0)
		    , Time(0)
		    , IndexOfMaxDigitizerSample(0)
		    , NumberOfSamples(0) {}
	};

	// CalorimeterFooterPacket: after transmission of all the hits the event is closed by a final packet containing the informaton about all the chanels that are reconstructed online in the FPGA:
	struct CalorimeterFooterPacket
	{
		uint16_t DetectorType : 3;  // subdetector type e.g. CALO=0, CAPHRI = 1, TRAD = 2, LASER = 3
		uint16_t BoardID : 8;       // unique board ID from 0 - 255
		uint16_t unused : 1;
		uint16_t ChannelStatusFlagA : 4;
		uint16_t ChannelStatusFlagC;
		uint16_t DIRACA;
		uint16_t DIRACB;
		uint16_t DIRACC;
		uint16_t DIRACD;
		uint16_t DIRACE;
		uint16_t DIRACF;
		CalorimeterFooterPacket()
		    : DetectorType(0)
		    , BoardID(0)
		    , unused(0)
		    , ChannelStatusFlagA(0)
		    , ChannelStatusFlagC(0) {}
	};

	struct CalorimeterCountersDataPacket
	{
		uint16_t numberOfCounters;
		CalorimeterCountersDataPacket()
		    : numberOfCounters(0) {}
	};

	std::unique_ptr<std::vector<std::pair<CalorimeterHitDataPacket, std::vector<uint16_t>>>>      GetCalorimeterHitData(size_t blockIndex) const;
	std::unique_ptr<std::vector<std::pair<CalorimeterHitTestDataPacket, std::vector<uint16_t>>>>  GetCalorimeterHitTestData(size_t blockIndex) const;
	std::unique_ptr<std::vector<std::pair<CalorimeterCountersDataPacket, std::vector<uint32_t>>>> GetCalorimeterCountersData(size_t blockIndex) const;
	std::unique_ptr<std::vector<std::pair<CalorimeterCountersDataPacket, std::vector<uint32_t>>>> GetEmulatedCountersData(size_t blockIndex) const;
	std::unique_ptr<CalorimeterFooterPacket>                                      GetCalorimeterFooter(size_t blockIndex) const;
	std::unique_ptr<std::vector<std::pair<CalorimeterHitDataPacket, uint16_t>>>                   GetCalorimeterHitsForTrigger(size_t blockIndex) const;
	std::unique_ptr<std::vector<std::pair<CalorimeterHitTestDataPacket, uint16_t>>>               GetCalorimeterHitTestForTrigger(size_t blockIndex) const;
};

using CalorimeterDataDecoders = std::vector<CalorimeterDataDecoder>;
}  // namespace mu2e
#endif /* mu2e_artdaq_Data_CalorimeterDataDecoder_hh */
