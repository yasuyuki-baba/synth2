#pragma once
#include <optional>

namespace synth2::editor::native
{
// Platform-window units, matching IGraphics::GetPlatformWindowScale().
struct EditorWindowBounds
{
    int width=0,height=0,maximumWidth=0,maximumHeight=0;
    int viewWidth=0,viewHeight=0;
};
std::optional<EditorWindowBounds> editorWindowBounds(void* view,bool standalone);
bool requestStandaloneWindowSize(void* view,int width,int height);
}
