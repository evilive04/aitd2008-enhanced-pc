#pragma once
#include <windows.h>
#include <cstdint>
namespace ui_transition_timing {
enum class Request { None, ChildReturn, OptionsReturn, OptionsChoiceReturn };
struct Rule {const char* path;std::uint32_t sprite,frames,first,last,target;};
// One-based native frame indices. These intervals contain no AVM actions;
// their terminal frames execute the original callbacks and state changes.
inline constexpr Rule ChildRules[]={
 {"/options/audio",405,153,102,117,118},
 {"/options/audio",405,153,119,144,145},
 {"/options/languages",414,55,45,54,55},
 {"/options/display",447,131,100,129,130},
 {"/options",572,271,91,122,123},
 {"/options/menu",330,64,9,18,19},
 {"/options/menu",330,64,20,26,27}
};
inline constexpr Rule OptionsRules[]={
 {"/options",572,271,212,221,222},
 {"/",0,646,77,84,85},
 {"/mainmenu",172,182,152,165,166}
};
// After original opt_out/skipmenu/setup callbacks117/131/137 have run,
// paused Options return spends another35 inert frames reopening the old icon
// surface. Keep original terminal173, which releases native input.
inline constexpr Rule PauseReturnRules[]={
 {"/",0,646,138,172,173}
};
inline constexpr Rule PauseBridgeRules[]={
 {"/",0,646,119,130,131},
 {"/",0,646,132,136,137}
};
// Keep every opening-loop action. Only inert gaps between them are shortened;
// the original scripter still initializes each icon and releases menu input.
inline constexpr Rule PauseChildRules[]={
 {"/menu",330,64,9,18,19},
 {"/menu",330,64,20,26,27},
 {"/menu/scripter",328,48,9,10,11}
};
inline constexpr Rule FrontendRules[]={
 {"/mainmenu",172,182,12,23,24},
 {"/mainmenu",172,182,25,29,30},
 {"/mainmenu",172,182,31,32,33},
 {"/mainmenu",172,182,99,116,117},
 {"/mainmenu",172,182,152,165,166},
 {"/",0,646,77,84,85},
 {"/",0,646,102,109,110},
 {"/options",572,271,9,10,11},
 {"/options",572,271,12,27,28},
 {"/options",572,271,29,33,34},
 {"/options",572,271,35,36,37},
 {"/options/menu",330,64,9,18,19},
 {"/options/menu",330,64,20,26,27},
 {"/options/menu/scripter",328,48,9,10,11},
 {"/options/audio",405,153,10,24,25},
 {"/options/audio",405,153,26,29,30},
 {"/options/audio",405,153,31,32,33},
 {"/options/languages",414,55,9,15,16},
 {"/options/display",447,131,3,11,12},
 {"/options/display",447,131,14,16,17},
 {"/options/display",447,131,18,21,22},
 {"/options/display",447,131,23,30,31},
 {"/options/display",447,131,32,38,39}
};
inline constexpr Rule ChapterRules[]={
 {"/menu",121,149,82,92,93},
 {"/menu",121,149,2,31,32},
 {"/menu",121,149,46,51,52},
 {"/menu",121,149,53,55,56},
 {"/menu",121,149,67,72,73},
 {"/menu",121,149,74,79,80},
 {"/menu/title",69,86,10,36,37},
 {"/menu/title",69,86,44,52,53},
 {"/menu/title",69,86,55,62,63},
 {"/menu/title",69,86,67,75,76},
 {"/menu/title",69,86,78,85,86}
};
inline bool frontendEnabled=false;
struct Pending {Request kind{};void* movie{};void* handle{};std::uint32_t root{},started{};bool inGame{};};
inline Pending pending{}; // UI thread only; never accessed by render/audio threads.
inline std::uint32_t advances=0,lastSprite=0,lastFrom=0,lastTarget=0;
inline void Begin(Request kind,void* movie,void* handle,bool inGame=false){
 pending={kind,movie,handle,handle?*static_cast<const std::uint32_t*>(handle):0,GetTickCount(),inGame};
}
inline void Reset(){pending={};}
inline bool Eligible(const Rule& rule,std::uint32_t sprite,std::uint32_t frames,std::uint32_t frame,std::uint32_t playing,bool changing){
 return sprite==rule.sprite&&frames==rule.frames&&frame>=rule.first&&frame<=rule.last&&playing==1&&!changing;
}
}
