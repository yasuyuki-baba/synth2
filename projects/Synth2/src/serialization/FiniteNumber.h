#pragma once

#include <cmath>
#include <cstdlib>
#include <locale.h>
#include <optional>
#include <string>
#include <string_view>
#if defined(__APPLE__)
#include <xlocale.h>
#endif

namespace synth2::serialization
{
// A private C numeric locale preserves strtod syntax without changing host-global
// locale state. Used only by non-realtime patch/state I/O.
struct NumericCLocale
{
#if defined(_WIN32)
    _locale_t handle=_create_locale(LC_NUMERIC,"C");
    ~NumericCLocale(){if(handle)_free_locale(handle);}
#else
    locale_t handle=newlocale(LC_NUMERIC_MASK,"C",nullptr);
    ~NumericCLocale(){if(handle)freelocale(handle);}
#endif
    NumericCLocale()=default;
    NumericCLocale(const NumericCLocale&)=delete;
    NumericCLocale& operator=(const NumericCLocale&)=delete;
};

inline std::optional<double> parseFiniteNumber(std::string_view text)
{
    static const NumericCLocale locale;
    if(!locale.handle)return std::nullopt;
    const std::string input(text);char* end=nullptr;
#if defined(_WIN32)
    const double value=_strtod_l(input.c_str(),&end,locale.handle);
#else
    const double value=strtod_l(input.c_str(),&end,locale.handle);
#endif
    if(end==input.c_str() || end!=input.c_str()+input.size() || !std::isfinite(value))return std::nullopt;
    return value;
}
}
