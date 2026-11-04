#pragma once
#include "HistoryQueue.h"
#include "LfoHistoryEvent.h"
namespace synth2::dsp
{
    template<uint32_t Capacity=4096>
    using LfoHistory=HistoryQueue<LfoHistoryEvent,Capacity>;
}
