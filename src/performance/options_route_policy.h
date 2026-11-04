#pragma once
#include "options_context.h"
namespace options_route_policy {
inline unsigned Count(bool ingame)noexcept{return ingame?3u:4u;}
inline DWORD RowAction(unsigned row,bool ingame)noexcept{
    const DWORD full[]={OptionsAudio,OptionsLanguages,OptionsDisplay,OptionsConfiguration};
    const DWORD paused[]={OptionsAudio,OptionsDisplay,OptionsConfiguration};
    return row<Count(ingame)?(ingame?paused[row]:full[row]):0;
}
inline DWORD NativeSlot(DWORD action,bool ingame)noexcept{
    for(unsigned row=0;row<Count(ingame);++row)if(RowAction(row,ingame)==action)return row+1;
    return 0;
}
}
