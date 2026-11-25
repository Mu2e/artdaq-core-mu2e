#ifndef artdaq_core_mu2e_Overlays_DTC_Packets_DTC_Event_h
#define artdaq_core_mu2e_Overlays_DTC_Packets_DTC_Event_h

#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_EventHeader.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_SubEvent.h"

#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_EventMode.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_EventWindowTag.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_Subsystem.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace DTCLib
{

class DTC_Event
{
  public:
	/// <summary>
	/// Construct a DTC_Event in "overlay" mode using the given DMA buffer pointer. Flag will be set that the packet
	/// is read-only.
	/// </summary>
	/// <param name="data">Pointer data</param>
	explicit DTC_Event(const void* data);

	explicit DTC_Event(size_t data_size);

	/// <summary>
	/// Construct a DTC_Event that takes over a buffer the caller has already filled (event header
	/// followed by the subevents). Nothing is copied and nothing is zeroed; call SetupEvent to finalize.
	/// </summary>
	/// <param name="filledEventBytes">The complete event bytes; the event keeps them alive</param>
	explicit DTC_Event(std::shared_ptr<std::vector<uint8_t>> filledEventBytes);

	DTC_Event()
	    : header_()
	    , sub_events_()
	    , buffer_ptr_(nullptr) {}

	static const int MAX_DMA_SIZE = 0x8000;  // 32k

	bool               SetupEvent();
	size_t             GetEventByteCount() const { return header_.inclusive_event_byte_count; }
	DTC_EventWindowTag GetEventWindowTag() const;
	void               SetEventWindowTag(DTC_EventWindowTag const& tag);
	void               SetEventMode(DTC_EventMode const& mode);
	const void*        GetRawBufferPointer() const { return buffer_ptr_; }

	std::vector<DTC_SubEvent> const& GetSubEvents() const
	{
		return sub_events_;
	}
	size_t GetSubEventCount() const { return sub_events_.size(); }

	size_t GetSubEventCount(DTC_Subsystem subsys) const
	{
		size_t count = 0;
		for(size_t ii = 0; ii < sub_events_.size(); ++ii)
		{
			if(sub_events_[ii].GetSubsystem() == subsys)
				++count;
		}
		return count;
	}

	size_t GetBlockCount(DTC_Subsystem subsys) const
	{
		size_t count = 0;
		for(size_t ii = 0; ii < sub_events_.size(); ++ii)
		{
			if(sub_events_[ii].GetSubsystem() == subsys)
			{
				count += sub_events_[ii].GetDataBlockCount();
			}
		}
		return count;
	}

	DTC_SubEvent* GetSubEvent(size_t idx)
	{
		if(idx >= sub_events_.size())
			throw std::out_of_range("Index " + std::to_string(idx) + " is out of range (max: " + std::to_string(sub_events_.size() - 1) + ")");
		return &sub_events_[idx];
	}

	void AddSubEvent(DTC_SubEvent subEvt)
	{
		sub_events_.push_back(subEvt);
		header_.num_dtcs++;
		UpdateHeader();
	}
	DTC_SubEvent* GetSubEventByDTCID(uint8_t dtc, DTC_Subsystem subsys)
	{
		for(size_t ii = 0; ii < sub_events_.size(); ++ii)
		{
			if(sub_events_[ii].GetDTCID() == dtc && sub_events_[ii].GetSubsystem() == static_cast<uint8_t>(subsys))
				return &sub_events_[ii];
		}
		return nullptr;
	}

	std::vector<DTC_SubEvent> GetSubsystemData(DTC_Subsystem subsys) const
	{
		std::vector<DTC_SubEvent> output;
		for(auto& subevt : sub_events_)
		{
			if(subevt.HasSubsystem(subsys))
			{
				output.push_back(subevt);
			}
		}
		return output;
	}

	std::vector<DTC_DataBlock> GetSubsystemBlocks(DTC_Subsystem subsys) const
	{
		std::vector<DTC_DataBlock> output;
		for(auto& subevt : sub_events_)
		{
			if(subevt.HasSubsystem(subsys))
			{
				for(auto& datablock : subevt.GetDataBlocks())
				{
					if(datablock.GetHeader()->GetSubsystem() == subsys)
					{
						output.push_back(datablock);
					}
				}
			}
		}
		return output;
	}

	DTC_EventHeader* GetHeader() { return &header_; }

	void UpdateHeader();
	void WriteEvent(std::ostream& output, bool includeDMAWriteSize = true);
	bool IsCorrupt() const { return corruption_detected_; }

  private:
	std::shared_ptr<std::vector<uint8_t>> allocBytes{nullptr};  ///< Used if the block owns its memory
	DTC_EventHeader                       header_;
	std::vector<DTC_SubEvent>             sub_events_;
	const void*                           buffer_ptr_;
	bool                                  corruption_detected_{false};
};

}  // namespace DTCLib

#endif  // artdaq_core_mu2e_Overlays_DTC_Packets_DTC_Event_h
