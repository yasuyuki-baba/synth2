#include "Synth1NativeLoadingStrategy.h"
#include "Synth1NativeLoadingTables.h"
#include "Synth1NativePre105Tables.h"
#include <stdexcept>

namespace synth1::patch
{
    namespace
    {
        class TableLoadingStrategy : public NativeLoadingStrategy
        {
        public:
            explicit TableLoadingStrategy(int version) : sourceVersion(version) {}
            bool discardsMidiAssignments() const noexcept override { return false; }

        protected:
            int convertValue(int index,int value,const std::array<int,99>& source) const override
            {
                if(value==128 && index==21)return extendedFilterDepth();
                // OSC2 coarse source/getter words are retained even outside the
                // host knob range. The pitch receiver applies its own limits.
                if(index==2 && value>=-1 && value<=255)return value;
                const int mode=source[38]==0 ? 0 : 1;
                const auto& curve=detail::nativeLoadingCurves[detail::nativeLoadingCurveIds[sourceVersion-104][mode][index]];
                if(index==9 && value>=-24 && value<=24)return value;
                if(index>=86 && index<=89 && value>=-1 && value<=65535)return value;
                if(value<0 || value>127)throw std::invalid_argument("Invalid native loading dial");
                return curve[static_cast<size_t>(value)];
            }
            virtual int extendedFilterDepth() const noexcept { return 128; }

        private:
            int sourceVersion;
        };

        class LegacyLoadingStrategy final : public TableLoadingStrategy
        {
        public:
            LegacyLoadingStrategy(int version,int filterDepth) : TableLoadingStrategy(version),maximumFilterDepth(filterDepth) {}
            bool discardsMidiAssignments() const noexcept override { return true; }

        private:
            int extendedFilterDepth() const noexcept override { return maximumFilterDepth; }
            int convertValue(int index,int value,const std::array<int,99>& source) const override
            {
                if(index!=50 && index!=51)return TableLoadingStrategy::convertValue(index,value,source);
                const int dependency=source[index==50 ? 44 : 43];
                if(value<0 || value>127 || dependency<0 || dependency>127)
                    throw std::invalid_argument("Invalid native sensitivity context");
                // RVA0x277ed: dependent fields read the original source and
                // retain single-precision operation order without contraction.
                volatile float normalized=static_cast<float>(dependency)*(1.0f/127.0f);
                volatile float fraction=1.0f-normalized;
                volatile float scaled=fraction*static_cast<float>(value);
                scaled=scaled*(-1.0f/127.0f);
                scaled=scaled*63.0f;
                return 64-static_cast<int>(scaled);
            }
            int maximumFilterDepth;
        };

        class Pre105LoadingStrategy final : public NativeLoadingStrategy
        {
        public:
            explicit Pre105LoadingStrategy(const NativeLoadingStrategy& next) : following(next) {}
            bool discardsMidiAssignments() const noexcept override { return true; }

        private:
            int convertValue(int index,int value,const std::array<int,99>& source) const override
            {
                const int converted=value>=0 && value<=127
                    ? detail::nativePre105Curves[detail::nativePre105CurveIds[index]][value] : value;
                return following.convertDial(index,converted,source);
            }
            const NativeLoadingStrategy& following;
        };
    }

    int NativeLoadingStrategy::convertDial(int index,int value,const std::array<int,99>& source) const
    {
        if(index<0 || index>=99)throw std::invalid_argument("Unsupported native loading version or index");
        // Legacy files discard even arbitrary signed serialized MIDI words.
        if(discardsMidiAssignments() && index>=86 && index<=89)
            return detail::nativeLoadingDefaults[static_cast<size_t>(index)];
        return convertValue(index,value,source);
    }

    const NativeLoadingStrategy& nativeLoadingStrategyFor(int version)
    {
        static const TableLoadingStrategy source104(104),source109(109),source110(110),source111(111),source112(112),source113(113);
        static const LegacyLoadingStrategy source105(105,127),source106(106,127),source107(107,128),source108(108,128);
        static const Pre105LoadingStrategy source100(source105);
        switch(version)
        {
            case 100:return source100;
            case 104:return source104;
            case 105:return source105;
            case 106:return source106;
            case 107:return source107;
            case 108:return source108;
            case 109:return source109;
            case 110:return source110;
            case 111:return source111;
            case 112:return source112;
            case 113:return source113;
            default:throw std::invalid_argument("Native loading supports source versions100 and104..113");
        }
    }
}
