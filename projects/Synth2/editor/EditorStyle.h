#pragma once
#include "editor/EditorAppearance.h"
#include "editor/EditorLayout.h"
#include "IGraphicsStructs.h"
#include <string>

namespace synth2::editor::native
{
using namespace iplug::igraphics;
inline IRECT rect(const synth2::editor::EditorBounds& b) { return IRECT(b.x,b.y,b.x+b.width,b.y+b.height); }
inline IColor color(const char* hex)
{
    const auto value=std::stoul(hex+1,nullptr,16);
    return IColor(255,(value>>16)&255,(value>>8)&255,value&255);
}
inline const auto& appearance=synth2::editor::editorAppearance();
inline const IColor background=color(appearance.background),panel=color(appearance.panel),ink=color(appearance.text);
inline const IColor muted=color(appearance.muted),accent=color(appearance.accent);
}
