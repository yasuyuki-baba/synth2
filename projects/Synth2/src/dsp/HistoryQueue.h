#pragma once
#include <array>
#include <atomic>
#include <cstdint>

namespace synth2::dsp
{
    // Bounded MPMC publication: host notifications may have different producers;
    // only the serialized audio/prepare consumer applies state. No heap,
    // locks or waiting for an unpublished producer on the audio consumer.
    template<class Event,uint32_t Capacity=4096>
    class HistoryQueue
    {
        static_assert(Capacity>1 && (Capacity&(Capacity-1))==0);
        struct Cell { std::atomic<uint32_t> sequence{};Event event{}; };
        std::array<Cell,Capacity> cells{};
        std::atomic<uint32_t> write{},read{};
        std::atomic<uint64_t> dropped{};
    public:
        HistoryQueue() noexcept
        { for(uint32_t i=0;i<Capacity;++i)cells[i].sequence.store(i,std::memory_order_relaxed); }
        bool push(const Event& event) noexcept
        {
            auto position=write.load(std::memory_order_relaxed);
            for(;;)
            {
                auto& cell=cells[position&(Capacity-1)];
                const auto difference=static_cast<int32_t>(cell.sequence.load(std::memory_order_acquire)-position);
                if(difference==0)
                {
                    if(write.compare_exchange_weak(position,position+1,std::memory_order_relaxed))
                    {
                        cell.event=event;cell.sequence.store(position+1,std::memory_order_release);return true;
                    }
                }
                else if(difference<0){dropped.fetch_add(1,std::memory_order_relaxed);return false;}
                else position=write.load(std::memory_order_relaxed);
            }
        }
        bool pop(Event& event) noexcept
        {
            auto position=read.load(std::memory_order_relaxed);
            for(;;)
            {
                auto& cell=cells[position&(Capacity-1)];
                const auto difference=static_cast<int32_t>(cell.sequence.load(std::memory_order_acquire)-(position+1));
                if(difference==0)
                {
                    if(read.compare_exchange_weak(position,position+1,std::memory_order_relaxed))
                    {
                        event=cell.event;cell.sequence.store(position+Capacity,std::memory_order_release);return true;
                    }
                }
                else if(difference<0)return false;
                else position=read.load(std::memory_order_relaxed);
            }
        }
        uint64_t droppedEvents() const noexcept { return dropped.load(std::memory_order_relaxed); }
        static constexpr uint32_t capacity=Capacity;
    };
}
