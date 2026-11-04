#pragma once
#include <windows.h>
#include <sddl.h>
#include <cwchar>
#include <type_traits>
#pragma comment(lib,"advapi32.lib")
namespace crash_protocol {
constexpr DWORD Magic=0x43544941,Version=1;
struct Shared {
 DWORD magic,version,bytes,pid;
 volatile LONG state; //0 armed,1 writing,2 complete,3 acknowledged
 DWORD thread;
 EXCEPTION_RECORD exception;
 CONTEXT context;
};
static_assert(std::is_trivial<Shared>::value&&std::is_standard_layout<Shared>::value,"Crash IPC must be POD");
inline bool Valid(const Shared& s,DWORD pid){return s.magic==Magic&&s.version==Version&&s.bytes==sizeof(Shared)&&s.pid==pid;}
struct Names {wchar_t mapping[256]{},request[256]{},ack[256]{};};
// Per-user names avoid cross-account collisions; kernel objects retain the
// creating user's default DACL. No handles are inherited by the game.
inline bool ObjectNames(DWORD pid,Names& out){
 HANDLE token{};if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return false;
 alignas(TOKEN_USER) BYTE storage[512]{};DWORD bytes{};
 const BOOL ok=GetTokenInformation(token,TokenUser,storage,sizeof storage,&bytes);CloseHandle(token);if(!ok)return false;
 LPWSTR sid{};if(!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(storage)->User.Sid,&sid))return false;
 const int a=swprintf_s(out.mapping,L"Local\\AITD2008Crash_%s_%lu_state",sid,pid);
 const int b=swprintf_s(out.request,L"Local\\AITD2008Crash_%s_%lu_request",sid,pid);
 const int c=swprintf_s(out.ack,L"Local\\AITD2008Crash_%s_%lu_ack",sid,pid);LocalFree(sid);return a>0&&b>0&&c>0;
}
}
