#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_SubEvent.h"

#include "artdaq-core-mu2e/Overlays/DTC_Types/Exceptions.h"

#include "TRACE/tracemf.h"

const uint8_t DTCLib::DTC_SubEvent::REQUIRED_SUBEVENT_FORMAT_VERSION = 1;

static constexpr int TLVL_CONSTRUCTOR = TLVL_DEBUG + 10;
static constexpr int TLVL_UPDATEHEADER = TLVL_DEBUG + 11;
static constexpr int TLVL_SETUP = TLVL_DEBUG + 12;
static constexpr int TLVL_SETUP_VERBOSE = TLVL_DEBUG + 13;
static constexpr int TLVL_SETUP_VERBOSE_2 = TLVL_DEBUG + 14;

DTCLib::DTC_SubEvent::DTC_SubEvent(const void *data)
	: header_(), data_blocks_(), buffer_ptr_(data)
{
	memcpy(&header_, data, sizeof(header_));
	TLOG(TLVL_CONSTRUCTOR) << "Header of DTC_SubEvent created, copy in data and call SetupSubEvent to finalize";
	// Moved remainder to SetupSubEvent() to allow for SubEvents to cross DMA transfers
	if (header_.subevent_format_version != REQUIRED_SUBEVENT_FORMAT_VERSION)
	{
		auto ptr = reinterpret_cast<const uint8_t *>(buffer_ptr_);
		TLOG_SCOPED(TLVL_ERROR)
		{
			TLOG_ADD << "SUBEVENT FORMAT VERSION MISMATCH - Subevent header raw data:";
			for (size_t i = 0; i < sizeof(header_); i += 4)
				TLOG_ADD << "\n"
						 << std::dec << "#" << std::setw(2) << std::setfill('0') << i << "/" << sizeof(header_) << ": 0x" << std::hex << std::setw(8) << std::setfill('0') << *((uint32_t *)(&(ptr[i])));
		}

		TLOG(TLVL_ERROR) << "A DTC_WrongPacketTypeException occurred while setting up a DTC Subevent in the header format version 0x" << std::hex << header_.subevent_format_version << " != 0x" << static_cast<uint16_t>(REQUIRED_SUBEVENT_FORMAT_VERSION) << ". Check that your DTC FPGA version matches the software expectation.";
		throw DTC_WrongPacketTypeException(REQUIRED_SUBEVENT_FORMAT_VERSION, header_.subevent_format_version);
	}
}

DTCLib::DTC_SubEvent::DTC_SubEvent(size_t data_size)
	: allocBytes(new std::vector<uint8_t>(data_size)), header_(), data_blocks_(), buffer_ptr_(allocBytes->data())
{
	TLOG(TLVL_CONSTRUCTOR) << "Empty DTC_SubEvent created, copy in data and call SetupSubEvent to finalize, data_size = " << data_size;
}

DTCLib::DTC_EventWindowTag DTCLib::DTC_SubEvent::GetEventWindowTag() const
{
	return DTC_EventWindowTag(header_.event_tag_low, header_.event_tag_high);
}

void DTCLib::DTC_SubEvent::SetEventWindowTag(DTC_EventWindowTag const &tag)
{
	uint64_t tag_word = tag.GetEventWindowTag(true);
	header_.event_tag_low = tag_word;
	header_.event_tag_high = tag_word >> 32;
}

void DTCLib::DTC_SubEvent::SetEventMode(DTC_EventMode const &mode)
{
	uint64_t mode_word = mode.mode0;
	mode_word += (static_cast<uint64_t>(mode.mode1) << 8);
	mode_word += (static_cast<uint64_t>(mode.mode2) << 16);
	mode_word += (static_cast<uint64_t>(mode.mode3) << 24);
	mode_word += (static_cast<uint64_t>(mode.mode4) << 32);

	header_.event_mode = mode_word;
}

uint8_t DTCLib::DTC_SubEvent::GetDTCID() const
{
	return header_.source_dtc_id;
}

void DTCLib::DTC_SubEvent::UpdateHeader()
{
	header_.inclusive_subevent_byte_count = sizeof(DTC_SubEventHeader);

	for (auto &block : data_blocks_)
	{
		header_.inclusive_subevent_byte_count += block.byteSize;
	}
	TLOG(TLVL_UPDATEHEADER) << "Inclusive SubEvent Byte Count is now " << header_.inclusive_subevent_byte_count << " for subevent " << static_cast<int>(GetDTCID());
}

bool DTCLib::DTC_SubEvent::SetupSubEvent(optional_string accumulatedErrors)
{
	auto ptr = reinterpret_cast<const uint8_t *>(buffer_ptr_);

	// Building a std::stringstream is slow, so none is built for a good subevent:
	// the error stream is created only when the first error is found.
	std::optional<std::stringstream> optionalErrorStream;
	auto errorStream = [&optionalErrorStream]() -> std::stringstream & {
		if (!optionalErrorStream) optionalErrorStream.emplace();
		return *optionalErrorStream;
	};

	memcpy(&header_, ptr, sizeof(header_));
	if (header_.subevent_format_version != REQUIRED_SUBEVENT_FORMAT_VERSION)
	{
		TLOG(TLVL_ERROR) << "A DTC_WrongPacketTypeException occurred while setting up a DTC Subevent in the header format version 0x" << std::hex << header_.subevent_format_version << ". Check that your DTC FPGA version matches the software expecation.";
		throw DTC_WrongPacketTypeException(REQUIRED_SUBEVENT_FORMAT_VERSION, header_.subevent_format_version);
	}

	// Quick walk for a good subevent: read the ROC header fields straight from the bytes and
	// build the block list, with no packet objects and no log lines.  It applies the same
	// checks as the full parse below (packet type, packet count against byte count, ROC index,
	// ROC tag, valid bit on every header after the first, number of ROCs, total byte count).
	// If any check fails, the block list is cleared and the full parse below runs from the
	// start, so the error messages, dumps and recovery are unchanged.
	// Not used when the SETUP trace levels are on, so their printouts still appear.
	if (!TTEST(TLVL_SETUP - TLVL_DEBUG))
	{
		static constexpr size_t  ROC_HEADER_PACKET_BYTES = 16;
		static constexpr uint8_t DATA_HEADER_PACKET_TYPE = 5;  // DTC_PacketType_DataHeader
		const uint64_t           subeventTag             = GetEventWindowTag().GetEventWindowTag(true);
		const size_t             subeventByteCount       = header_.inclusive_subevent_byte_count;
		const uint8_t           *rocHeaderBytes          = ptr + sizeof(header_);
		size_t                   walkedByteCount         = sizeof(header_);
		size_t                   rocIndex                = 0;
		bool                     allRocHeadersGood       = true;

		data_blocks_.reserve(header_.num_rocs);
		while (walkedByteCount < subeventByteCount)
		{
			if (subeventByteCount - walkedByteCount < ROC_HEADER_PACKET_BYTES)
			{
				allRocHeadersGood = false;
				break;
			}
			const size_t  blockByteCount = static_cast<size_t>(rocHeaderBytes[0]) + (static_cast<size_t>(rocHeaderBytes[1]) << 8);
			const uint8_t packetType     = (rocHeaderBytes[2] >> 4) & 0xF;
			const uint8_t linkID         = rocHeaderBytes[3] & 0x7;
			const bool    validBit       = (rocHeaderBytes[3] & 0x80) != 0;
			const size_t  packetCount    = static_cast<size_t>(rocHeaderBytes[4]) + (static_cast<size_t>(rocHeaderBytes[5] & 7) << 8);
			uint64_t      rocTag         = 0;
			memcpy(&rocTag, rocHeaderBytes + 6, sizeof(rocTag));
			rocTag &= 0x0000FFFFFFFFFFFFull;

			if (packetType != DATA_HEADER_PACKET_TYPE || (packetCount + 1) * 16 != blockByteCount || linkID != rocIndex || rocTag != subeventTag || (rocIndex != 0 && !validBit))
			{
				allRocHeadersGood = false;
				break;
			}

			data_blocks_.emplace_back(static_cast<const void *>(rocHeaderBytes), blockByteCount);
			walkedByteCount += blockByteCount;
			rocHeaderBytes += blockByteCount;
			++rocIndex;
		}

		if (allRocHeadersGood && rocIndex == header_.num_rocs && walkedByteCount == subeventByteCount)
		{
			if (accumulatedErrors) accumulatedErrors->get() = std::string();
			return !corruption_detected_;
		}
		data_blocks_.clear();  // something is wrong: let the full parse below find and report it
	}

	// printout SubEvent header
	if (TTEST(TLVL_SETUP_VERBOSE - TLVL_DEBUG))
	{
		std::stringstream testss;
		testss << "subevent header Tag=" << GetEventWindowTag().GetEventWindowTag(true) << " (0x" << std::hex << GetEventWindowTag().GetEventWindowTag(true) << ") bytes=" << std::dec << sizeof(header_) << ": 0x ";
		for (size_t i = 0; i < sizeof(header_); i += 4)
			testss << std::hex << std::setw(8) << std::setfill('0') << *((uint32_t *)(&(ptr[i]))) << ' ';
		TLOG(TLVL_SETUP_VERBOSE) << testss.str();
		TLOG(TLVL_SETUP_VERBOSE) << header_.toJson();
	}
	ptr += sizeof(header_);  // moving ptr past subevent header

	TLOG(TLVL_SETUP) << "Found sub event inclusive byte count as: " << header_.inclusive_subevent_byte_count << " 0x" << std::hex << std::setw(4) << std::setfill('0') << header_.inclusive_subevent_byte_count << ". i.e., " << std::dec << std::setw(0) << (header_.inclusive_subevent_byte_count - sizeof(header_)) / 16 << " subevent packets.";

	size_t byte_count = sizeof(header_);
	uint8_t roc_fragi = -1;
	// one allocation for the block list (num_rocs is 8 bits, so at most 255 entries)
	data_blocks_.reserve(header_.num_rocs);
	while (byte_count < header_.inclusive_subevent_byte_count)
	{
		++roc_fragi;
		TLOG(TLVL_SETUP) << "Current byte_count is " << byte_count << " / " << header_.inclusive_subevent_byte_count << ", creating block";
		try
		{
			// Parse the ROC header packet once, on the stack, and use it for the block size and
			// for the checks below.  (DTC_DataBlock::GetHeader() would parse it a second time
			// into a heap allocation per ROC.)  Throws the same exceptions, before a block is added.
			const DTC_DataHeaderPacket rocHeaderPacket{DTC_DataPacket(static_cast<const void *>(ptr))};
			data_blocks_.emplace_back(static_cast<const void *>(ptr), static_cast<size_t>(rocHeaderPacket.GetByteCount()));
			auto data_block_byte_count = data_blocks_.back().byteSize;
			byte_count += data_block_byte_count;
			TLOG(TLVL_SETUP) << "Found ROC fragment #" << static_cast<int>(roc_fragi) << " block of byte_count " << data_block_byte_count << " 0x" << std::hex << data_block_byte_count << " (i.e., " << std::dec << data_block_byte_count / 16 << " fragment packets).";

			// printout ROC fragment data block
			if (data_block_byte_count > 16 * 2)  // more than 2 packets
				TLOG(TLVL_SETUP_VERBOSE_2) << "Beginning "
										   << std::hex << std::setw(8) << std::setfill('0')
										   << *((uint32_t *)(&(ptr[0 * 4]))) << ' ' << *((uint32_t *)(&(ptr[1 * 4]))) << ' '
										   << *((uint32_t *)(&(ptr[2 * 4]))) << ' ' << *((uint32_t *)(&(ptr[3 * 4]))) << ' '
										   << *((uint32_t *)(&(ptr[4 * 4]))) << ' ' << *((uint32_t *)(&(ptr[5 * 4]))) << ' '
										   << *((uint32_t *)(&(ptr[6 * 4]))) << ' ' << *((uint32_t *)(&(ptr[7 * 4])));
			else
				TLOG(TLVL_SETUP_VERBOSE_2) << "Beginning "
										   << std::hex << std::setw(8) << std::setfill('0')
										   << *((uint32_t *)(&(ptr[0 * 4]))) << ' ' << *((uint32_t *)(&(ptr[1 * 4]))) << ' '
										   << *((uint32_t *)(&(ptr[2 * 4]))) << ' ' << *((uint32_t *)(&(ptr[3 * 4])));

			if (data_block_byte_count > 8 * 4)
			{
				// save to print entire packet (for debugging and verifying emulator ROC):
				// for(size_t i = 0; i < data_block_byte_count; i += 4)
				// {
				// 	std::cout << std::dec << "#" << i << "/" << data_block_byte_count <<
				// 		"(" << i/16 << "/" << data_block_byte_count/16 << ")" << std::hex << std::setw(8) << std::setfill('0') << *((uint32_t *)(&(ptr[i]))) << std::endl;
				// 	if( i/16 > 0 && ((*((uint32_t *)(&(ptr[i]))) >> 4) & 0x0FFF) != (data_block_byte_count/16 - 1 - i/16))
				// 	{
				// 		TLOG(TLVL_ERROR) << "A DTC_WrongPacketTypeException occurred while setting up a ROC payload at packet" <<
				// 			i/16;
				// 		throw DTC_WrongPacketTypeException((data_block_byte_count/16 - 1 - i/16),((*((uint32_t *)(&(ptr[i]))) >> 4) & 0x0FFF));
				// 	}
				// }

				size_t i = data_block_byte_count - 8 * 4;
				TLOG(TLVL_SETUP_VERBOSE_2) << "End (starting at data block word #" << i << ") "
										   << std::hex << std::setw(8) << std::setfill('0')
										   << *((uint32_t *)(&(ptr[i + 0 * 4]))) << ' ' << *((uint32_t *)(&(ptr[i + 1 * 4]))) << ' '
										   << *((uint32_t *)(&(ptr[i + 2 * 4]))) << ' ' << *((uint32_t *)(&(ptr[i + 3 * 4]))) << ' '
										   << *((uint32_t *)(&(ptr[i + 4 * 4]))) << ' ' << *((uint32_t *)(&(ptr[i + 5 * 4]))) << ' '
										   << *((uint32_t *)(&(ptr[i + 6 * 4]))) << ' ' << *((uint32_t *)(&(ptr[i + 7 * 4])));
			}

			if (rocHeaderPacket.GetLinkID() != roc_fragi)
			{
				errorStream() <<"\nA DTC_WrongPacketTypeException, mismatch of ROC Index, occurred while setting up a ROC header packet. Expected " << static_cast<int>(roc_fragi) << ", but data stream contained " << static_cast<int>(rocHeaderPacket.GetLinkID());
				TLOG(TLVL_ERROR) << errorStream().str();
				throw DTC_WrongPacketTypeException(roc_fragi, rocHeaderPacket.GetLinkID());
			}
			if (rocHeaderPacket.GetEventWindowTag().GetEventWindowTag(true) != GetEventWindowTag().GetEventWindowTag(true))
			{
				errorStream() <<"\nA DTC_WrongPacketTypeException, mismatch of ROC Event Tag, occurred while setting up a ROC #" << static_cast<int>(roc_fragi) << " header packet. Expected " << GetEventWindowTag().GetEventWindowTag(true) << ", but data stream contained " << rocHeaderPacket.GetEventWindowTag().GetEventWindowTag(true);
				TLOG(TLVL_ERROR) << errorStream().str();
				throw DTC_WrongPacketTypeException(GetEventWindowTag().GetEventWindowTag(true), rocHeaderPacket.GetEventWindowTag().GetEventWindowTag(true));
			}

			if (byte_count < header_.inclusive_subevent_byte_count)
			{
				if (DTC_DataHeaderPacket::IsDataHeaderPacket(ptr + data_block_byte_count, GetEventWindowTag(), GetDTCID()))
				{
					ptr += data_block_byte_count;  // moving ptr past the ROC fragment data block
				}
				else
				{
					errorStream() <<"\nCorruption (unable to find next Header packet) detected in block " << static_cast<int>(roc_fragi) << " of SubEvent EWT: " << GetEventWindowTag() << ", DTC " << static_cast<int>(GetDTCID());
					TLOG(TLVL_ERROR) << errorStream().str();
					corruption_detected_ = true;

					TLOG(TLVL_DEBUG + 6) << "Trying to recover...";
					ptr += 16;  // Advance one packet;
					size_t offset = 16;
					while (byte_count - data_block_byte_count + offset < header_.inclusive_subevent_byte_count && !DTC_DataHeaderPacket::IsDataHeaderPacket(ptr, GetEventWindowTag(), GetDTCID()))
					{
						ptr += 16;
						offset += 16;
					}
					if (!DTC_DataHeaderPacket::IsDataHeaderPacket(ptr, GetEventWindowTag(), GetDTCID()))
					{
						errorStream() <<"\nCorruption recovery failed, this SubEvent cannot be processed further";
						TLOG(TLVL_ERROR) << errorStream().str();
						throw DTC_WrongPacketTypeException(5, 0);
					}

					byte_count += offset - data_block_byte_count;
					TLOG(TLVL_WARNING) << "Successfully recovered from corruption, will continue decoding rest of SubEvent (but will be marked as corrupt)";
				}
			}
		}
		catch (DTC_WrongPacketTypeException const &ex)
		{
			corruption_detected_ = true;
			errorStream() <<"\nA DTC_WrongPacketTypeException occurred while setting up a ROC Fragment #" << static_cast<int>(roc_fragi) << " in the subevent at location " << byte_count << " / " << header_.inclusive_subevent_byte_count << " 0x" << std::hex << byte_count << " / 0x" << header_.inclusive_subevent_byte_count;
			TLOG(TLVL_ERROR) << errorStream().str();
			// printout SubEvent header
			{
				std::stringstream testss;
				testss << "ROC Data Header (w/overrun) for tag=" << GetEventWindowTag().GetEventWindowTag(true) << " (0x" << std::hex << GetEventWindowTag().GetEventWindowTag(true) << ") bytes=" << std::dec << sizeof(header_) << ": 0x ";
				for (size_t i = 0; i < sizeof(header_); i += 4)
					testss << std::hex << std::setw(8) << std::setfill('0') << *((uint32_t *)(&(ptr[i]))) << ' ';
				TLOG(TLVL_ERROR) << testss.str();
				TLOG(TLVL_ERROR) << header_.toJson();
			}
			{
				uint32_t roci = 0;
				for (auto &data_block : data_blocks_)
				{
					std::stringstream testss;
					testss << "ROC header #" << roci++ << " ROC byte count = " << data_block.GetHeader()->GetByteCount() << ": 0x ";
					ptr = reinterpret_cast<const uint8_t *>(data_block.GetRawBufferPointer());
					for (size_t i = 0; i < sizeof(header_); i += 4)
						testss << std::hex << std::setw(8) << std::setfill('0') << *((uint32_t *)(&(ptr[i]))) << ' ';
					testss << "\n End: ";
					for (size_t i = 0; i < sizeof(header_) * 3; i += 4)
						testss << std::hex << std::setw(8) << std::setfill('0') << *((uint32_t *)(&(ptr[i + data_block.GetHeader()->GetByteCount() - 128]))) << ' ';

					// testss << "\n All: ";
					// for(size_t i = 0; i < data_block.GetHeader()->GetByteCount(); i+=4)
					// {
					// 	if(i%( 24*4) == 0) testss << "\n" << i << "\t";
					// 	testss << std::hex << std::setw(8) << std::setfill('0') << *((uint32_t *)(&(ptr[i]))) << ' ';
					// }

					std::cout << testss.str() << "\n"
							  << std::flush;
				}
			}
			// Stop parsing: the read position is not reliably past the failing block,
			// so another pass of the loop would re-read the same bytes
			break;
		}
		catch (DTC_WrongPacketSizeException const &ex)
		{
			corruption_detected_ = true;
			errorStream() <<"\nA DTC_WrongPacketSizeException occurred while setting up a ROC Fragment #" << static_cast<int>(roc_fragi) << " in the sub event at location " << byte_count << " / " << header_.inclusive_subevent_byte_count << " 0x" << std::hex << byte_count << " / 0x" << header_.inclusive_subevent_byte_count;
			TLOG(TLVL_ERROR) << errorStream().str();
			// printout SubEvent header
			{
				std::stringstream testss;
				testss << "subevent header Tag=" << GetEventWindowTag() << " (0x" << std::hex << GetEventWindowTag() << ") bytes=" << std::dec << sizeof(header_) << ": 0x ";
				for (size_t i = 0; i < sizeof(header_); i += 4)
					testss << std::hex << std::setw(8) << std::setfill('0') << *((uint32_t *)(&(ptr[i]))) << ' ';
				TLOG(TLVL_ERROR) << testss.str();
				TLOG(TLVL_ERROR) << header_.toJson();
			}
			// Neither byte_count nor ptr advanced: the constructor threw before a block was
			// added, so looping again would re-parse the same bytes forever (2026-09-22 bench:
			// one bad ROC header produced 254k identical passes / 1.5 GB of log before the
			// process was killed).  The remaining bytes of this subevent cannot be walked
			// without a trustworthy header, so stop here and let the caller see IsCorrupt().
			errorStream() <<"\nCannot continue past a ROC header with a bad packet count; abandoning the rest of this SubEvent.";
			TLOG(TLVL_ERROR) << errorStream().str();
			break;
		}
	}

	if (++roc_fragi != header_.num_rocs)
	{
		errorStream() <<"\nCorruption detected in SubEvent! Expected 6 ROC fragments, found " << static_cast<int>(roc_fragi) << ". Check for corrupted byte/packet counts.";
		TLOG(TLVL_ERROR) << errorStream().str();
		auto ptr = reinterpret_cast<const uint8_t *>(buffer_ptr_);
		std::cout << "--> Printing entire buffer ptr data: 0x ";
		for (size_t i = 0; i < header_.inclusive_subevent_byte_count; i += 4)
			std::cout << std::hex << std::setw(8) << std::setfill('0') << *((uint32_t *)(&(ptr[i]))) << ' ';
		std::cout << std::endl;
		std::cout << "--> Printing beyond buffer ptr data: 0x ";
		for (size_t i = static_cast<size_t>(header_.inclusive_subevent_byte_count); i < static_cast<size_t>(header_.inclusive_subevent_byte_count) + 64; i += 4)
			std::cout << std::hex << std::setw(8) << std::setfill('0') << *((uint32_t *)(&(ptr[i]))) << ' ';
		std::cout << std::endl;
		corruption_detected_ = true;
	}

	if (byte_count != header_.inclusive_subevent_byte_count)
	{
		errorStream() <<"\nCorruption detected in SubEvent! Expected byte count " << header_.inclusive_subevent_byte_count << ", found " << byte_count;
		TLOG(TLVL_ERROR) << errorStream().str();
		corruption_detected_ = true;
	}

	if (corruption_detected_)
	{
		errorStream() <<"\nData Corruption Detected in SubEvent! EWT: " << GetEventWindowTag() << ", DTCID: " << static_cast<int>(GetDTCID());
		TLOG(TLVL_ERROR) << errorStream().str();
	}
	if (accumulatedErrors) accumulatedErrors->get() = optionalErrorStream ? optionalErrorStream->str() : std::string();
	return !corruption_detected_;

}  // end SetupSubEvent()
