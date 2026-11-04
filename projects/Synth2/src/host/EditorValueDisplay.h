#pragma once
#include "../patch/Synth1EnvelopeTime.h"
#include "../patch/Synth1Tuning.h"
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>

namespace synth2::host
{
// Presentation only: host automation, text-entry prompts and source words retain
// their original units. Times are nominal envelope endpoints, not elapsed time.
inline bool editorHasUnit(int parameter)
{
    switch(parameter)
    {
        case 2: case 3: case 72: case 15: case 16: case 18:
        case 25: case 26: case 28: case 61: case 62: case 63: return true;
        default: return false;
    }
}
inline std::string editorDecimal(double value,int precision,const char* suffix="")
{
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::fixed << std::setprecision(precision) << value << suffix;
    return out.str();
}
inline std::string editorValueDisplay(int parameter,double source)
{
    const int raw=std::clamp(static_cast<int>(std::lround(source)),0,127);
    const float dial=static_cast<float>(std::clamp(source,0.,127.));
    switch(parameter)
    {
        case 2: return editorDecimal(coarseTuneSemitones(raw),0," st");
        case 3: case 72: {const auto cents=fineTuneCents(dial);return editorDecimal(cents,cents==std::round(cents) ? 0 : 2," ct");}
        case 15: case 25: case 16: case 18: case 26: case 28:
        {
            const double seconds=parameter==15 || parameter==25
                ? 0.0005f*std::pow(56000.0f,dial/127.0f)
                : 0.01f*std::pow(6000.0f,dial/127.0f);
            return seconds<1. ? editorDecimal(seconds*1000.,seconds<.01 ? 2 : 1," ms")
                              : editorDecimal(seconds,2," s");
        }
        case 61:
        {
            const double hz=50.*std::pow(320.,dial*(1.f/127.f));
            return hz<1000. ? editorDecimal(hz,0," Hz") : editorDecimal(hz/1000.,2," kHz");
        }
        case 62: return editorDecimal((dial-64)*(1.f/127.f)*50.f,1," dB");
        case 63: return editorDecimal(std::pow(10.,dial*(1.f/127.f)),2," Q");
        default:
        {
            // Normalized host round trips can leave tiny integer residuals.
            const double rounded=std::round(source);
            return std::abs(source-rounded)<1.e-6 ? editorDecimal(rounded,0)
                                                 : editorDecimal(source,2);
        }
    }
}
}
