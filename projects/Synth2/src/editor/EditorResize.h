#pragma once
#include <algorithm>
#include <cmath>
#include <optional>

namespace synth2::editor
{
inline constexpr float editorMinimumScale=.5f,editorMaximumScale=4.f;

// Parent sizes are platform pixels; the graphics canvas keeps logical bounds.
// Windows/macOS standalone windows follow the dragged dimension and resize the
// other axis. Linux standalone and plugin views fit within parent-owned bounds.
inline std::optional<float> editorResizeScale(int width,int height,float platformScale,
    float currentScale,bool keyboardVisible,bool followChangedAxis)
{
    if(width<=0 || height<=0 || !std::isfinite(platformScale) || platformScale<=0)return {};
    const float logicalHeight=keyboardVisible ? 936.f : 840.f;
    // Rounded platform sizes must not feed back as repeated one-pixel resizes.
    if(std::abs(width-platformScale*1280.f*currentScale)<=1.f &&
       std::abs(height-platformScale*logicalHeight*currentScale)<=1.f)return currentScale;
    const float x=width/(platformScale*1280.f);
    const float y=height/(platformScale*logicalHeight);
    const auto requested=followChangedAxis
        ? (std::abs(x-currentScale)>std::abs(y-currentScale) ? x : y)
        : std::min(x,y);
    return std::clamp(requested,editorMinimumScale,editorMaximumScale);
}
}
