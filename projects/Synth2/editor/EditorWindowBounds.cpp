#include "EditorWindowBounds.h"
#include <algorithm>
#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#import <AppKit/AppKit.h>
#elif defined(__linux__)
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <cstdint>
#endif

namespace synth2::editor::native
{
bool requestStandaloneWindowSize(void* view,int width,int height)
{
    if(!view || width<=0 || height<=0)return false;
#ifdef _WIN32
    const auto parent=GetAncestor(static_cast<HWND>(view),GA_ROOT);
    RECT client{},frame{};
    if(!parent || !GetClientRect(parent,&client) || !GetWindowRect(parent,&frame))return false;
    return SetWindowPos(parent,nullptr,0,0,width+frame.right-frame.left-client.right,
        height+frame.bottom-frame.top-client.bottom,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE)!=0;
#elif defined(__APPLE__)
    NSView* editor=static_cast<NSView*>(view);
    NSWindow* window=[editor window];
    if(!window)return false;
    [window setContentSize:NSMakeSize(width,height)];
    return true;
#elif defined(__linux__)
    auto* display=XOpenDisplay(nullptr);
    if(!display)return false;
    const auto editor=static_cast<Window>(reinterpret_cast<std::uintptr_t>(view));
    Window root=0,parent=0,*children=nullptr;unsigned count=0;
    XWindowAttributes child{};
    const bool found=XQueryTree(display,editor,&root,&parent,&children,&count) && parent &&
        XGetWindowAttributes(display,editor,&child);
    if(children)XFree(children);
    if(found)XResizeWindow(display,parent,width,height+std::max(0,child.y));
    XCloseDisplay(display);
    return found;
#else
    return false;
#endif
}

std::optional<EditorWindowBounds> editorWindowBounds(void* view,bool standalone)
{
    if(!view)return {};
    EditorWindowBounds bounds;
#ifdef _WIN32
    const auto editor=static_cast<HWND>(view);
    const auto parent=standalone ? GetAncestor(editor,GA_ROOT) : GetParent(editor);
    RECT client{},frame{};
    if(!parent || !GetClientRect(parent,&client))return {};
    bounds.width=client.right;bounds.height=client.bottom;
    MONITORINFO monitor{sizeof(MONITORINFO)};
    if(GetMonitorInfo(MonitorFromWindow(parent,MONITOR_DEFAULTTONEAREST),&monitor) && GetWindowRect(parent,&frame)) {
        bounds.maximumWidth=monitor.rcWork.right-monitor.rcWork.left-(frame.right-frame.left-client.right);
        bounds.maximumHeight=monitor.rcWork.bottom-monitor.rcWork.top-(frame.bottom-frame.top-client.bottom);
    }
#elif defined(__APPLE__)
    NSView* editor=static_cast<NSView*>(view);
    NSWindow* window=[editor window];
    NSView* content=[window contentView];
    // An AU editor may itself be the content view. Its superview then includes
    // the window frame, which is not available editor space.
    NSView* parent=(standalone || editor==content) ? content : [editor superview];
    if(!parent)return {};
    const auto viewSize=[editor bounds].size;
    bounds.viewWidth=static_cast<int>(viewSize.width);bounds.viewHeight=static_cast<int>(viewSize.height);
    const auto size=[parent bounds].size;
    bounds.width=static_cast<int>(size.width);bounds.height=static_cast<int>(size.height);
    NSScreen* screen=[window screen];
    if(screen) {
        const auto available=[screen visibleFrame];
        const auto content=[window contentRectForFrameRect:available];
        bounds.maximumWidth=static_cast<int>(content.size.width);
        bounds.maximumHeight=static_cast<int>(content.size.height);
    }
#elif defined(__linux__)
    struct Connection {
        Display* display=XOpenDisplay(nullptr);
        ~Connection(){if(display)XCloseDisplay(display);}
    };
    thread_local Connection connection;
    auto* display=connection.display;
    if(!display)return {};
    const auto editor=static_cast<Window>(reinterpret_cast<std::uintptr_t>(view));
    Window root=0,parent=0,*children=nullptr;unsigned count=0;
    if(!XQueryTree(display,editor,&root,&parent,&children,&count))return {};
    if(children)XFree(children);
    XWindowAttributes client{},child{};
    if(!parent || !XGetWindowAttributes(display,parent,&client) || !XGetWindowAttributes(display,editor,&child))return {};
    const int menu=standalone ? std::max(0,child.y) : 0;
    bounds.width=client.width;bounds.height=client.height-menu;
    bounds.maximumWidth=DisplayWidth(display,DefaultScreen(display));
    bounds.maximumHeight=DisplayHeight(display,DefaultScreen(display))-menu;
    // EWMH work area excludes desktop panels; frame extents exclude decorations.
    const auto property=[&](Window window,const char* name,unsigned long* result) {
        Atom type=0;int format=0;unsigned long length=0,remaining=0;unsigned char* data=nullptr;
        const auto atom=XInternAtom(display,name,True);
        if(!atom)return false;
        const int status=XGetWindowProperty(display,window,atom,0,4,False,XA_CARDINAL,
            &type,&format,&length,&remaining,&data);
        const bool valid=status==Success && type==XA_CARDINAL && format==32 && length>=4 && data;
        if(valid)std::copy_n(reinterpret_cast<unsigned long*>(data),4,result);
        if(data)XFree(data);
        return valid;
    };
    unsigned long work[4]{},frame[4]{};
    if(property(root,"_NET_WORKAREA",work)) {
        bounds.maximumWidth=static_cast<int>(work[2]);
        bounds.maximumHeight=static_cast<int>(work[3])-menu;
    }
    if(property(parent,"_NET_FRAME_EXTENTS",frame)) {
        bounds.maximumWidth-=static_cast<int>(frame[0]+frame[1]);
        bounds.maximumHeight-=static_cast<int>(frame[2]+frame[3]);
    }
#else
    return {};
#endif
    if(bounds.width<=0 || bounds.height<=0)return {};
    return bounds;
}
}
