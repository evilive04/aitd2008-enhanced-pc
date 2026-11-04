// Original PC atlas regions; top-left64x64 remains the tutorial mouse icon.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace pc_keycaps {
struct Key {const char* label;std::uint16_t x,y,width;};
static constexpr Key keys[]={
    {"A",64,0,64},
    {"B",128,0,64},
    {"C",192,0,64},
    {"D",256,0,64},
    {"E",320,0,64},
    {"F",384,0,64},
    {"G",448,0,64},
    {"H",512,0,64},
    {"I",576,0,64},
    {"J",640,0,64},
    {"K",704,0,64},
    {"L",768,0,64},
    {"M",832,0,64},
    {"N",896,0,64},
    {"O",960,0,64},
    {"P",0,64,64},
    {"Q",64,64,64},
    {"R",128,64,64},
    {"S",192,64,64},
    {"T",256,64,64},
    {"U",320,64,64},
    {"V",384,64,64},
    {"W",448,64,64},
    {"X",512,64,64},
    {"Y",576,64,64},
    {"Z",640,64,64},
    {"0",704,64,64},
    {"1",768,64,64},
    {"2",832,64,64},
    {"3",896,64,64},
    {"4",960,64,64},
    {"5",0,128,64},
    {"6",64,128,64},
    {"7",128,128,64},
    {"8",192,128,64},
    {"9",256,128,64},
    {"F1",320,128,64},
    {"F2",384,128,64},
    {"F3",448,128,64},
    {"F4",512,128,64},
    {"F5",576,128,64},
    {"F6",640,128,64},
    {"F7",704,128,64},
    {"F8",768,128,64},
    {"F9",832,128,64},
    {"F10",896,128,64},
    {"F11",960,128,64},
    {"F12",0,192,64},
    {"SPACE",0,256,256},
    {"TAB",256,256,128},
    {"ENTER",384,256,192},
    {"ESC",576,256,128},
    {"SHIFT",704,256,192},
    {"CTRL",0,320,128},
    {"ALT",128,320,128},
    {"BACKSPACE",256,320,256},
    {"UP",512,320,64},
    {"DOWN",576,320,64},
    {"LEFT",640,320,64},
    {"RIGHT",704,320,64},
    {"LSHIFT",0,384,192},
    {"RSHIFT",192,384,192},
    {"LCTRL",384,384,192},
    {"RCTRL",576,384,192},
    {"LALT",0,448,128},
    {"RALT",128,448,128},
    {"CAPSLOCK",256,448,192},
    {"INSERT",448,448,128},
    {"DELETE",576,448,128},
    {"HOME",704,448,128},
    {"END",832,448,128},
    {"PAGEUP",0,512,192},
    {"PAGEDOWN",192,512,192},
    {"SEMICOLON",384,512,64},
    {"MOUSE LEFT",0,640,64},
    {"MOUSE RIGHT",64,640,64},
};
static constexpr unsigned atlasSize=1024;
static constexpr std::uint32_t textureNameLow=0x8790fce0;
static constexpr std::uint32_t textureNameHigh=0xccd33a99;
static constexpr unsigned keyCount=sizeof(keys)/sizeof(keys[0]);
static bool Equal(const char* a,const char* b){while(*a&&*b){const char x=*a>='a'&&*a<='z'?static_cast<char>(*a-32):*a;const char y=*b>='a'&&*b<='z'?static_cast<char>(*b-32):*b;if(x!=y)return false;++a;++b;}return *a==*b;}
[[maybe_unused]] static int Find(const char* label){
    for(unsigned i=0;i<keyCount;++i)if(Equal(label,keys[i].label))return static_cast<int>(i);
    struct Alias{const char* from;const char* to;};
    static constexpr Alias aliases[]={{"Spacebar","SPACE"},{"Space bar","SPACE"},{"Return","ENTER"},{"Escape","ESC"},{"Control","CTRL"},{"Left Shift","LSHIFT"},{"Right Shift","RSHIFT"},{"Left Control","LCTRL"},{"Right Control","RCTRL"},{"Left Ctrl","LCTRL"},{"Right Ctrl","RCTRL"},{"Left Alt","LALT"},{"Right Alt","RALT"},{"Caps Lock","CAPSLOCK"},{"Page Up","PAGEUP"},{"Page Down","PAGEDOWN"},{"Up Arrow","UP"},{"Down Arrow","DOWN"},{"Left Arrow","LEFT"},{"Right Arrow","RIGHT"}};
    if(Equal(label,";"))return Find("SEMICOLON");
    for(const auto& a:aliases)if(Equal(label,a.from))return Find(a.to);
    return -1;
}
}
