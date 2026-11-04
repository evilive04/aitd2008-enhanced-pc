#pragma once
#include <windows.h>
#include <cmath>
#include <cwchar>
#include <cwctype>
#include <cerrno>
#include <cstdint>
namespace native_lod {
struct Settings {float quality=1,drawDistance=1;bool qualityValid=true,drawValid=true;};
inline bool ParseScale(const wchar_t* text,float& value) noexcept {
 value=1;if(!text)return false;while(std::iswspace(*text))++text;if(!*text)return false;
 errno=0;wchar_t* end{};const double parsed=std::wcstod(text,&end);if(end==text||errno==ERANGE||!std::isfinite(parsed))return false;
 while(std::iswspace(*end))++end;if(*end)return false;value=static_cast<float>(parsed<1?1:parsed>1.5?1.5:parsed);return true;
}
// Higher-detail transitions cannot extend past the independently chosen final
// mesh cutoff. This mirrors the small native selector detour, not spawn logic.
inline float Threshold(float original,float finalThreshold,unsigned index,unsigned count,const Settings& s) noexcept {
 if(s.quality==1&&s.drawDistance==1)return original;
 const float q=s.quality*s.quality,d=s.drawDistance*s.drawDistance;
 const float final=finalThreshold*d;
 if(!std::isfinite(original)||std::signbit(original)||!std::isfinite(finalThreshold)||finalThreshold<=0||!std::isfinite(final))return original;
 if(index+1==count)return original*d;
 const float detail=original*q;return detail<final?detail:final;
}
enum class Reason {Vanilla,Active,Unsupported,InvalidConfiguration,LowAddressSpace,MemoryStatusUnavailable};
struct Status {Settings requested{};float appliedQuality=1,appliedDrawDistance=1;bool installed=false;Reason reason=Reason::Vanilla;ULONGLONG availableVirtual=0;};
// Call once from authenticated startup BEFORE native scene/renderer workers
// can execute the selector. OFF remains a dormant hook; no hot DLL unloading.
bool InstallAuthenticated(bool supportedExecutable) noexcept;
void Configure(const wchar_t* iniPath) noexcept;
// Existing frame/control tick only. INI metadata <=1Hz, memory status <=0.2Hz.
// Module never writes INI or logs. Parent may log changed Status at <=1Hz.
void Tick(ULONGLONG now=GetTickCount64()) noexcept;
Status GetStatus() noexcept;
const char* ReasonName(Reason) noexcept;
}
