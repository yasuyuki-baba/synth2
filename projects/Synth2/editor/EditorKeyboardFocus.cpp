#include "EditorKeyboardFocus.h"
#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#import <AppKit/AppKit.h>
#endif

namespace synth2::editor::native {
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
#else
    return false;
#endif
}
}
