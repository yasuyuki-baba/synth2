#pragma once
#include <array>

namespace synth1::patch
{
    // Strategies convert one dial against the complete original source context.
    // The returned instances are immutable and require no per-load allocation.
    class NativeLoadingStrategy
    {
    public:
        virtual ~NativeLoadingStrategy() = default;
        int convertDial(int index,int value,const std::array<int,99>& source) const;
        virtual bool discardsMidiAssignments() const noexcept = 0;

    private:
        virtual int convertValue(int index,int value,const std::array<int,99>& source) const = 0;
    };

    // Source100 composes pre-105 and105 conversion. Every supported104..113
    // version selects its own pinned table row and format-specific behavior.
    const NativeLoadingStrategy& nativeLoadingStrategyFor(int version);
}
