#pragma once
#include "ui_art.h"
#include <windows.h>
namespace ui_pointer {
inline bool centralCursor=false,centralPass=false;
// Phone opening hides the Windows cursor. Keep its show count untouched and
// supply a vector pointer only while the system pointer is actually hidden.
// Callers use the same DPI-correct canonical point as their hit testing.
inline bool NeedsSoftwareCursor(bool positionValid,bool querySucceeded,DWORD flags,bool handleValid=true)noexcept{
    return positionValid&&(!querySucceeded||(!(flags&CURSOR_SUPPRESSED)&&(!(flags&CURSOR_SHOWING)||!handleValid)));
}
inline void AppendHiddenCursor(ui_art::Scene& scene,bool positionValid,float x,float y){
    if(centralCursor&&!centralPass)return;
    CURSORINFO info{};info.cbSize=sizeof info;
    const bool queried=GetCursorInfo(&info)!=FALSE;
    if(positionValid&&queried&&(info.flags&CURSOR_SHOWING)&&!(info.flags&CURSOR_SUPPRESSED)){
        static HCURSOR custom{};static bool tried=false;
        if(!tried){tried=true;wchar_t path[MAX_PATH]{};if(GetModuleFileNameW(nullptr,path,MAX_PATH)){auto* slash=wcsrchr(path,L'\\');if(slash){slash[1]=0;if(!wcscat_s(path,L"assets\\ui\\cursor_metal.cur"))custom=static_cast<HCURSOR>(LoadImageW(nullptr,path,IMAGE_CURSOR,0,0,LR_LOADFROMFILE));}}}
        if(custom)SetCursor(custom);return;
    }
    if(!NeedsSoftwareCursor(positionValid,queried,info.flags,info.hCursor!=nullptr))return;
    scene.Image(7,404,672,x,y,29,48.24f);

}
}
