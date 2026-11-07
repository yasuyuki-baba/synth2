#include "EditorKeyboardFocus.h"
#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#import <AppKit/AppKit.h>
#elif defined(__linux__)
#include <X11/Xlib.h>
#include <cstdint>
#endif

namespace synth2::editor::native {
#if defined(__linux__)
namespace {
// Focus is polled from the UI thread; reuse its own X11 connection.
struct FocusDisplay {
    Display* display=XOpenDisplay(nullptr);
    ~FocusDisplay() { if(display)XCloseDisplay(display); }
};
}
#endif
bool editorKeyboardHasFocus(void* view) {
    if(!view)return false;
#ifdef _WIN32
    const auto window=static_cast<HWND>(view);
    const auto focused=GetFocus();
    return focused==window || (focused && IsChild(window,focused));
#elif defined(__APPLE__)
    NSView* editor=static_cast<NSView*>(view);
    NSWindow* window=[editor window];
    NSResponder* responder=[window firstResponder];
    return [NSApp isActive] && [window isKeyWindow] &&
        [responder isKindOfClass:[NSView class]] &&
        (responder==editor || [(NSView*)responder isDescendantOf:editor]);
#elif defined(__linux__)
    thread_local FocusDisplay connection;
    auto* display=connection.display;
    if(!display)return false;
    const auto editor=static_cast<Window>(reinterpret_cast<std::uintptr_t>(view));
    Window focused=None;
    int revert=0;
    XGetInputFocus(display,&focused,&revert);
    while(focused!=None && focused!=PointerRoot) {
        if(focused==editor)return true;
        Window root=None,parent=None,*children=nullptr;
        unsigned count=0;
        if(!XQueryTree(display,focused,&root,&parent,&children,&count))return false;
        if(children)XFree(children);
        if(focused==root || parent==focused)break;
        focused=parent;
    }
    return false;
#else
    return false;
#endif
}
}
