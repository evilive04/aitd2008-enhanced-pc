#pragma once
#include <windows.h>
namespace ui_pointer {
// Keep client bounds and pointer coordinates in one DPI context. The native
// engine's render/input threads can otherwise virtualize only one of them.
inline bool ReadClient(HWND window,RECT& client,POINT& pointer)noexcept{
    client={};pointer={};if(!window||!IsWindow(window))return false;
    const auto previous=SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if(!previous)return false;
    const bool okay=GetClientRect(window,&client)&&client.right>0&&client.bottom>0&&
        GetPhysicalCursorPos(&pointer)&&ScreenToClient(window,&pointer)&&
        pointer.x>=0&&pointer.y>=0&&pointer.x<client.right&&pointer.y<client.bottom;
    SetThreadDpiAwarenessContext(previous);return okay;
}
}
