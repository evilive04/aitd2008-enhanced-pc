#include "native_lod.h"
#include <cstring>
namespace native_lod {
namespace {
volatile LONG active{},qualityBits=0x3f800000,drawBits=0x3f800000;
void* accepted=reinterpret_cast<void*>(0x8be410);void* advance=reinterpret_cast<void*>(0x8be406);
Status status{};wchar_t path[32768]{};bool configured{},checked{},stamped{},memoryChecked{},memoryAvailable{};
ULONGLONG lastCheck{},lastMemory{};WIN32_FILE_ATTRIBUTE_DATA stamp{};bool stampExists{};
constexpr ULONGLONG MinimumVirtual=512ull*1024*1024;
// Original COMISS/JBE are reproduced after preserving EDX and XMM1/2. The
// selector's flags and accepted/advance destinations are exactly native.
__declspec(naked) void SelectHook(){__asm {
 cmp dword ptr [active],0
 je compare_native
 push edx
 sub esp,32
 movups [esp],xmm1
 movups [esp+16],xmm2
 movd edx,xmm1
 test edx,80000000h
 jnz invalid_threshold
 cmp edx,7f800000h
 jae invalid_threshold
 mov edx,[edi+40h]
 movss xmm2,[edx+ecx*8-4]
 movd edx,xmm2
 test edx,80000000h
 jnz invalid_threshold
 test edx,edx
 jz invalid_threshold
 cmp edx,7f800000h
 jae invalid_threshold
 mulss xmm2,dword ptr [drawBits]
 movd edx,xmm2
 cmp edx,7f800000h
 jae invalid_threshold
 lea edx,[eax+1]
 cmp edx,ecx
 je final_level
 mulss xmm1,dword ptr [qualityBits]
 minss xmm1,xmm2
 jmp scaled
 final_level:
 movaps xmm1,xmm2
 scaled:
 comiss xmm0,xmm1
 movups xmm1,[esp]
 movups xmm2,[esp+16]
 lea esp,[esp+32]
 pop edx
 jbe accepted_native
 jmp dword ptr [advance]
 invalid_threshold:
 movups xmm1,[esp]
 movups xmm2,[esp+16]
 lea esp,[esp+32]
 pop edx
 compare_native:
 comiss xmm0,xmm1
 jbe accepted_native
 jmp dword ptr [advance]
 accepted_native:
 jmp dword ptr [accepted]
}}
bool Matches(const void* at,const unsigned char* expected,std::size_t bytes) noexcept {
 __try{return std::memcmp(at,expected,bytes)==0;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool PatchSelector(unsigned char* target) noexcept {
 unsigned char patch[5]={0xe9};const auto displacement=static_cast<DWORD>(reinterpret_cast<std::uintptr_t>(&SelectHook)-reinterpret_cast<std::uintptr_t>(target+5));std::memcpy(patch+1,&displacement,4);
 DWORD previous{};if(!VirtualProtect(target,5,PAGE_EXECUTE_READWRITE,&previous)){status.reason=Reason::Unsupported;return false;}
 std::memcpy(target,patch,5);FlushInstructionCache(GetCurrentProcess(),target,5);DWORD ignored{};const BOOL restored=VirtualProtect(target,5,previous,&ignored);(void)restored;
 status.installed=true;return true;
}
void Publish(float q,float d) noexcept {
 LONG qb{},db{};q*=q;d*=d;std::memcpy(&qb,&q,4);std::memcpy(&db,&d,4);
 if(qualityBits==qb&&drawBits==db)return;
 InterlockedExchange(&active,0);
 InterlockedExchange(&qualityBits,qb);InterlockedExchange(&drawBits,db);
 if(q!=1||d!=1)InterlockedExchange(&active,1);
}
bool SameStamp(bool exists,const WIN32_FILE_ATTRIBUTE_DATA& a) noexcept {
 return exists==stampExists&&(!exists||(a.ftLastWriteTime.dwLowDateTime==stamp.ftLastWriteTime.dwLowDateTime&&a.ftLastWriteTime.dwHighDateTime==stamp.ftLastWriteTime.dwHighDateTime&&a.nFileSizeLow==stamp.nFileSizeLow&&a.nFileSizeHigh==stamp.nFileSizeHigh));
}
bool Read(const wchar_t* key,float& value) noexcept {
 wchar_t text[96]{};const auto size=GetPrivateProfileStringW(L"Graphics",key,L"1.0",text,96,path);return size<95&&ParseScale(text,value);
}
}
bool InstallAuthenticated(bool supported) noexcept {
 if(status.installed)return true;
 const unsigned char selector[]={0x0f,0x2f,0xc1,0x76,0x0a},distance[]={0xf3,0x0f,0x59,0x44,0x24,0x24},tag[]={0x81,0x7f,0x04,0x47,0x4c,0x4f,0x44};
 auto* target=reinterpret_cast<unsigned char*>(0x8be401);
 if(!supported||GetModuleHandleW(nullptr)!=reinterpret_cast<HMODULE>(0x400000)||!Matches(target,selector,sizeof selector)||!Matches(reinterpret_cast<void*>(0x8be3d7),distance,sizeof distance)||!Matches(reinterpret_cast<void*>(0x8be307),tag,sizeof tag)){status.reason=Reason::Unsupported;return false;}
 return PatchSelector(target);
}
void Configure(const wchar_t* iniPath) noexcept {
 Publish(1,1);status.requested={};status.appliedQuality=status.appliedDrawDistance=1;configured=false;checked=stamped=memoryChecked=false;stampExists=false;
 if(iniPath&&wcslen(iniPath)<32768){wcscpy_s(path,iniPath);configured=true;}else path[0]=0;
}
void Tick(ULONGLONG now) noexcept {
 if(checked&&now>=lastCheck&&now-lastCheck<1000)return;checked=true;lastCheck=now;
 WIN32_FILE_ATTRIBUTE_DATA next{};const bool exists=configured&&GetFileAttributesExW(path,GetFileExInfoStandard,&next)&&!(next.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY);
 if(!stamped||!SameStamp(exists,next)){Settings s{};if(exists){s.qualityValid=Read(L"LodQualityScale",s.quality);s.drawValid=Read(L"ObjectDrawDistanceScale",s.drawDistance);}status.requested=s;stamp=next;stampExists=exists;stamped=true;}
 float q=status.requested.quality,d=status.requested.drawDistance;
 if(!status.installed){q=d=1;status.reason=Reason::Unsupported;}
 else if(!status.requested.qualityValid||!status.requested.drawValid){q=d=1;status.reason=Reason::InvalidConfiguration;}
 else if(q==1&&d==1)status.reason=Reason::Vanilla;
 else {
  if(!memoryChecked||now<lastMemory||now-lastMemory>=5000){MEMORYSTATUSEX memory{};memory.dwLength=sizeof memory;memoryAvailable=GlobalMemoryStatusEx(&memory)!=FALSE;status.availableVirtual=memoryAvailable?memory.ullAvailVirtual:0;lastMemory=now;memoryChecked=true;}
  if(!memoryAvailable){q=d=1;status.reason=Reason::MemoryStatusUnavailable;}
  else if(status.availableVirtual<MinimumVirtual){q=d=1;status.reason=Reason::LowAddressSpace;}
  else status.reason=Reason::Active;
 }
 status.appliedQuality=q;status.appliedDrawDistance=d;Publish(q,d);
}
Status GetStatus() noexcept{return status;}
const char* ReasonName(Reason reason) noexcept {switch(reason){case Reason::Vanilla:return "vanilla";case Reason::Active:return "active";case Reason::Unsupported:return "unsupported";case Reason::InvalidConfiguration:return "invalid_configuration";case Reason::LowAddressSpace:return "low_address_space";case Reason::MemoryStatusUnavailable:return "memory_status_unavailable";}return "unknown";}
}
