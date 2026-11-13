#pragma once
#include <algorithm>
#include <cmath>
#include <optional>

namespace synth2::editor
{
inline constexpr float editorMinimumScale=.5f,editorMaximumScale=4.f;

struct EditorResizeSize
{
    int width {},height {};
    bool operator==(const EditorResizeSize&) const = default;
};

// Only observed geometry changes are authoritative. A scale request can resize
// the child before an AU host acknowledges it by changing the parent.
inline std::optional<EditorResizeSize> editorObservedResize(EditorResizeSize parent,
    EditorResizeSize previousParent,EditorResizeSize view,EditorResizeSize rendered)
{
    if(parent.width<=0 || parent.height<=0)return {};
    if(parent!=previousParent)return parent;
    if(view.width>0 && view.height>0 && view!=rendered)return view;
    return {};
}

// Parent sizes are platform pixels; every platform fits the complete logical
// canvas within both bounds without requesting another parent-window resize.
inline std::optional<float> editorResizeScale(int width,int height,float platformScale,
    bool keyboardVisible,float currentScale=0.f)
{
    if(width<=0 || height<=0 || !std::isfinite(platformScale) || platformScale<=0)return {};
    const float logicalHeight=keyboardVisible ? 936.f : 840.f;
    // The SDK truncates logical window dimensions before applying platform DPI.
    // Preserve an exact match so repeated percentage/keyboard requests do not
    // lose another fractional pixel on each conversion. A one-pixel change in
    // the parent still recomputes the fit; this is not a resize tolerance.
    if(std::isfinite(currentScale) && currentScale>=editorMinimumScale && currentScale<=editorMaximumScale &&
       std::trunc(std::trunc(1280.f*currentScale)*platformScale)==width &&
       std::trunc(std::trunc(logicalHeight*currentScale)*platformScale)==height)return currentScale;
    const float x=width/(platformScale*1280.f);
    const float y=height/(platformScale*logicalHeight);
    return std::clamp(std::min(x,y),editorMinimumScale,editorMaximumScale);
}
}
