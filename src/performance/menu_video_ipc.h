#pragma once
#include <windows.h>
#include <cstdint>
#include <cstddef>
#include "menu_video.h"
namespace menu_video_ipc {
constexpr DWORD Magic=0x564d4441,Version=1,HeaderBytes=65536;
constexpr DWORD Capacity=4096u*4096u*3u/2u;
constexpr DWORD MappingBytes=HeaderBytes+Capacity;
// Only fixed-width fields cross the x86/x64 boundary. The unnamed mapping,
// mutex and events are inherited by the explicit child handle allowlist.
#pragma pack(push,8)
struct Header {
    DWORD magic,version,capacity,parentPid;
    volatile LONG active,error;
    std::uint64_t serial,loop;
    DWORD width,height,matrix,range,chroma,primaries,transfer,bytes;
    std::int64_t timestamp,duration;
    menu_video::Diagnostic diagnostic;
};
#pragma pack(pop)
static_assert(offsetof(Header,serial)==24&&offsetof(Header,diagnostic)==88,"IPC layout");
static_assert(sizeof(Header)<HeaderBytes,"IPC header extent");
static_assert(sizeof(Header)==320,"Fixed-width IPC extent");
inline bool Valid(const Header& h){return h.magic==Magic&&h.version==Version&&h.capacity==Capacity;}
inline bool FrameValid(const Header& h){
    return h.serial&&h.width&&h.height&&h.width<=4096&&h.height<=4096&&!(h.width&3)&&!(h.height&1)&&
        h.bytes==std::uint64_t(h.width)*h.height*3/2&&h.bytes<=Capacity;
}
}
