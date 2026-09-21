#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_Event.h"

#include "artdaq-core-mu2e/Overlays/DTC_Types/Exceptions.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/Utilities.h"

#include "TRACE/tracemf.h"

static constexpr int TLVL_CONSTRUCTOR = TLVL_DEBUG + 10;
static constexpr int TLVL_UPDATEHEADER = TLVL_DEBUG + 11;
static constexpr int TLVL_SETUP = TLVL_DEBUG + 12;
static constexpr int TLVL_WRITEEVENT = TLVL_DEBUG + 13;

DTCLib::DTC_Event::DTC_Event(const void* data)
	: header_(), sub_events_(), buffer_ptr_(data)
{
	memcpy(&header_, data, sizeof(header_));
	TLOG(TLVL_CONSTRUCTOR) << "Header of DTC_Event " << GetEventWindowTag().GetEventWindowTag(true) << " created, copy in data and call SetupEvent to finalize";
}

DTCLib::DTC_Event::DTC_Event(size_t data_size)
	: allocBytes(new std::vector<uint8_t>(data_size)), header_(), sub_events_(), buffer_ptr_(allocBytes->data())
{
	TLOG(TLVL_CONSTRUCTOR) << "Empty DTC_Event created, copy in data and call SetupEvent to finalize";
}

bool DTCLib::DTC_Event::SetupEvent()
{
	auto ptr = reinterpret_cast<const uint8_t*>(buffer_ptr_);

	memcpy(&header_, ptr, sizeof(header_));
	ptr += sizeof(header_);

	size_t byte_count = sizeof(header_);
	while (byte_count < header_.inclusive_event_byte_count)
	{
		TLOG(TLVL_SETUP) << "Current byte_count is " << byte_count << " / " << header_.inclusive_event_byte_count << ", creating sub event";
		try
		{
			sub_events_.emplace_back(ptr);
			auto subEventOK = sub_events_.back().SetupSubEvent();
			corruption_detected_ |= !subEventOK;
			ptr += sub_events_.back().GetSubEventByteCount();
			byte_count += sub_events_.back().GetSubEventByteCount();
			if (sub_events_.back().GetSubEventByteCount() == 0)
			{
				TLOG(TLVL_ERROR) << "Invalid empty sub event byte count interpretation!";
				auto ex = DTC_WrongPacketSizeException(sizeof(DTC_SubEventHeader), sub_events_.back().GetSubEventByteCount());
				throw ex;
			}
			TLOG(TLVL_SETUP) << "Found sub event byte_count of " << sub_events_.back().GetSubEventByteCount();
		}
		catch (DTC_WrongPacketTypeException const& ex)
		{
			TLOG(TLVL_ERROR) << "A DTC_WrongPacketTypeException occurred while setting up the event at location 0x" << std::hex << byte_count;
			corruption_detected_ = true;
			break;
		}
		catch (DTC_WrongPacketSizeException const& ex)
		{
			TLOG(TLVL_ERROR) << "A DTC_WrongPacketSizeException occurred while setting up the event at location 0x" << std::hex << byte_count;
			TLOG(TLVL_ERROR) << "This event has been truncated.";
			corruption_detected_ = true;
			break;
		}
	}

	if (corruption_detected_)
	{
		TLOG(TLVL_ERROR) << "Data corruption detected in Event " << GetEventWindowTag() << "! Data may be incomplete!";
	}
	return !corruption_detected_;
}  // end SetupEvent()

DTCLib::DTC_EventWindowTag DTCLib::DTC_Event::GetEventWindowTag() const
{
	return DTC_EventWindowTag(header_.event_tag_low, header_.event_tag_high);
}

void DTCLib::DTC_Event::SetEventWindowTag(DTC_EventWindowTag const& tag)
{
	uint64_t tag_word = tag.GetEventWindowTag(true);
	header_.event_tag_low = tag_word;
	header_.event_tag_high = tag_word >> 32;
}

void DTCLib::DTC_Event::SetEventMode(DTC_EventMode const& mode)
{
	uint64_t mode_word = mode.mode0;
	mode_word += (static_cast<uint64_t>(mode.mode1) << 8);
	mode_word += (static_cast<uint64_t>(mode.mode2) << 16);
	mode_word += (static_cast<uint64_t>(mode.mode3) << 24);
	mode_word += (static_cast<uint64_t>(mode.mode4) << 32);

	header_.event_mode = mode_word;
}

void DTCLib::DTC_Event::UpdateHeader()
{
	header_.inclusive_event_byte_count = sizeof(DTC_EventHeader);
	for (auto& sub_evt : sub_events_)
	{
		sub_evt.UpdateHeader();
		header_.inclusive_event_byte_count += sub_evt.GetSubEventByteCount();
	}
	TLOG(TLVL_UPDATEHEADER) << "Inclusive Event Byte Count is now " << header_.inclusive_event_byte_count << " for event " << GetEventWindowTag().GetEventWindowTag(true);
}

void DTCLib::DTC_Event::WriteEvent(std::ostream& o, bool includeDMAWriteSize)
{
	TLOG(TLVL_WRITEEVENT) << "Updating header byte counts";
	UpdateHeader();

	if (header_.inclusive_event_byte_count + sizeof(uint64_t) + (includeDMAWriteSize ? sizeof(uint64_t) : 0) < MAX_DMA_SIZE)
	{
		TLOG(TLVL_WRITEEVENT) << "Event fits into one buffer, writing";
		auto pos = o.tellp();
		Utilities::WriteDMABufferSizeWords(o, includeDMAWriteSize, header_.inclusive_event_byte_count, pos, false);

		TLOG(TLVL_WRITEEVENT) << "Writing DTC_EventHeader for event " << GetEventWindowTag().GetEventWindowTag(true) << " sz=" << sizeof(DTC_EventHeader);
		o.write(reinterpret_cast<const char*>(&header_), sizeof(DTC_EventHeader));

		for (auto& subevt : sub_events_)
		{
			TLOG(TLVL_WRITEEVENT) << "Writing DTC_SubEventHeader for DTC " << static_cast<int>(subevt.GetDTCID()) << " sz=" << sizeof(DTC_SubEventHeader);
			o.write(reinterpret_cast<const char*>(subevt.GetHeader()), sizeof(DTC_SubEventHeader));
			auto ii = 0;
			for (auto& blk : subevt.GetDataBlocks())
			{
				TLOG(TLVL_WRITEEVENT) << "Writing Data Block " << ii << ", roc=" << blk.GetHeader()->GetLinkID() << ", sz=" << blk.byteSize;
				o.write(static_cast<const char*>(blk.blockPointer), blk.byteSize);
				++ii;
			}
		}
	}
	else
	{
		TLOG(TLVL_WRITEEVENT) << "Event spans multiple buffers, beginning write";
		auto buffer_start = o.tellp();
		size_t bytes_written = Utilities::WriteDMABufferSizeWords(o, includeDMAWriteSize, header_.inclusive_event_byte_count, buffer_start, false);

		o.write(reinterpret_cast<const char*>(&header_), sizeof(DTC_EventHeader));
		size_t buffer_data_size = sizeof(DTC_EventHeader);
		size_t total_data_size = 0;

		for (auto& subevt : sub_events_)
		{
			if (bytes_written + buffer_data_size + sizeof(DTC_SubEventHeader) > MAX_DMA_SIZE)
			{
				TLOG(TLVL_WRITEEVENT) << "Starting new buffer, writing size words " << buffer_data_size << " to old buffer";
				Utilities::WriteDMABufferSizeWords(o, includeDMAWriteSize, buffer_data_size, buffer_start, true);
				buffer_start = o.tellp();
				total_data_size += buffer_data_size;
				TLOG(TLVL_WRITEEVENT) << "Writing anticipated data size " << header_.inclusive_event_byte_count - total_data_size << " to new buffer.";
				bytes_written = Utilities::WriteDMABufferSizeWords(o, includeDMAWriteSize, header_.inclusive_event_byte_count - total_data_size, buffer_start, false);
				buffer_data_size = 0;
			}
			o.write(reinterpret_cast<const char*>(subevt.GetHeader()), sizeof(DTC_SubEventHeader));
			buffer_data_size += sizeof(DTC_SubEventHeader);
			for (auto& blk : subevt.GetDataBlocks())
			{
				if (bytes_written + buffer_data_size + blk.byteSize > MAX_DMA_SIZE)
				{
					TLOG(TLVL_WRITEEVENT) << "Starting new buffer, writing size words " << buffer_data_size << " to old buffer";
					Utilities::WriteDMABufferSizeWords(o, includeDMAWriteSize, buffer_data_size, buffer_start, true);
					buffer_start = o.tellp();
					total_data_size += buffer_data_size;
					TLOG(TLVL_WRITEEVENT) << "Writing anticipated data size " << header_.inclusive_event_byte_count - total_data_size << " to new buffer.";
					bytes_written = Utilities::WriteDMABufferSizeWords(o, includeDMAWriteSize, header_.inclusive_event_byte_count - total_data_size, buffer_start, false);
					buffer_data_size = 0;
				}
				o.write(static_cast<const char*>(blk.blockPointer), blk.byteSize);
				buffer_data_size += blk.byteSize;
			}
		}
	}
}
