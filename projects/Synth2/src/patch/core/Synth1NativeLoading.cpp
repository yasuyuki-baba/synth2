#include "Synth1NativeLoading.h"
#include "Synth1NativeLoadingTables.h"
#include "Synth1NativeLoadingStrategy.h"
#include <stdexcept>

namespace synth1::patch
{
    // Defaults are source dials. Apply the selected version's conversion only
    // after overlaying explicit entries; converted getter values are not defaults.
    int nativeDefaultSourceDial(int index,int /*version*/)
    { return detail::nativeLoadingDefaults.at(static_cast<size_t>(index)); }

    int convertNativeLoadingDial(int version,int index,int value,const std::array<int,99>& source)
    {
        if(!isNativeSourceVersion(version) || index<0 || index>=99)
            throw std::invalid_argument("Unsupported native loading version or index");
        return nativeLoadingStrategyFor(version).convertDial(index,value,source);
    }

    NativePatchControls resolveNativePatchControls(const Synth1PatchDocument& document)
    {
        if(!document.version() && document.name.empty())
            throw std::invalid_argument("Native loading requires a named patch or explicit version");
        NativePatchControls result;result.sourceVersion=document.version().value_or(100);
        if(!isNativeSourceVersion(result.sourceVersion))
            throw std::invalid_argument("Native loading supports source versions100 and104..113");
        const auto& strategy=nativeLoadingStrategyFor(result.sourceVersion);
        auto source=detail::nativeLoadingDefaults;
        for(const auto& entry:document.entries)
        {
            const int index=entry.index,value=entry.rawValue;
            const int minimum=index==2 ? -1 : index==9 ? -24 : index>=86 && index<=89 ? -1 : 0;
            const int maximum=index==2 ? 255 : index==9 ? 24 : index>=86 && index<=89 ? 65535 : index==21 ? 128 : 127;
            const bool discardedMidi=strategy.discardsMidiAssignments() && index>=86 && index<=89;
            if(index<0 || index>=126 || (!discardedMidi && (value<minimum || value>maximum)))
                throw std::invalid_argument("Invalid native patch source value");
            result.explicitSource.set(static_cast<size_t>(index));
            // The DLL's program path does not dispatch reserved indices99..125.
            if(index<99)source[static_cast<size_t>(index)]=value;
        }
        // Conversions must read the original complete source, never a partially
        // converted array. Otherwise dependent fields can change with key order.
        for(int index=0;index<99;++index)
            result.values[index]=strategy.convertDial(index,source[index],source);
        return result;
    }
}
