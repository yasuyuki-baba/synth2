#pragma once
#include <algorithm>
#include <cmath>
#include <optional>

namespace synth2::editor
{
inline constexpr float editorMinimumScale=.5f,editorMaximumScale=4.f;

// Parent sizes are platform pixels; every platform fits the complete logical
// canvas within both bounds without requesting another parent-window resize.
inline std::optional<float> editorResizeScale(int width,int height,float platformScale,
    bool keyboardVisible)
{
    if(width<=0 || height<=0 || !std::isfinite(platformScale) || platformScale<=0)return {};
    const float logicalHeight=keyboardVisible ? 936.f : 840.f;
    const float x=width/(platformScale*1280.f);
    const float y=height/(platformScale*logicalHeight);
    return std::clamp(std::min(x,y),editorMinimumScale,editorMaximumScale);
}
}
