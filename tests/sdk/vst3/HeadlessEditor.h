#pragma once
// Keep real popup ownership types while leaving drawing and OS windows out of
// the native host-boundary test. No editor interaction is exercised here.
#include "IPlugPlatform.h"
#include <functional>
namespace iplug::igraphics {
class IPopupMenu;
using IPopupFunction=std::function<void(IPopupMenu*)>;
}
#include "IGraphicsPopupMenu.h"
namespace iplug::igraphics { class IGraphics; class IControl; }
