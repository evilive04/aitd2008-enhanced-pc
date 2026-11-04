#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstring>
#include "../native_shadow_sites.h"
// Transaction fixtures use exclusively owned pages. Windows can map NLS/shared
// data at the old literal test VAs; do not overwrite or unmap those mappings.
// Exact production-address signatures are separately checked on native bytes
// by test-native-shadow-path.py. Only this include's address references redirect.
namespace native_shadows {static Site fixtureSites[siteCount]{};static std::uint32_t fixtureTable{};}
static unsigned checks=0,protectCalls=0,failProtection=0;
static bool option=true;
static bool ConfigFlag(const wchar_t*,const wchar_t*,bool,const wchar_t*){return option;}
static void Log(const char*,...){}
static BOOL TestProtect(void* address,SIZE_T size,DWORD protection,DWORD* old){
    if(++protectCalls==failProtection){SetLastError(ERROR_ACCESS_DENIED);return FALSE;}
    return VirtualProtect(address,size,protection,old);
}
#define VirtualProtect TestProtect
#define sites fixtureSites
#define tableAddress fixtureTable
#include "../native_shadow_quality.inl"
#undef tableAddress
#undef sites
#undef VirtualProtect
static void Check(bool ok,const char* why){++checks;if(!ok){fprintf(stderr,"FAIL %s\n",why);ExitProcess(1);}}
static unsigned maxWidth=2048,maxHeight=2048;static HRESULT capsResult=S_OK;
static HRESULT STDMETHODCALLTYPE Caps(IDirect3DDevice9*,D3DCAPS9* caps){caps->MaxTextureWidth=maxWidth;caps->MaxTextureHeight=maxHeight;return capsResult;}
static void Populate(){
    for(const auto& s:native_shadows::fixtureSites){auto* p=reinterpret_cast<BYTE*>(s.address);p[0]=s.opcode;memcpy(p+1,&s.original,4);}
    memcpy(reinterpret_cast<void*>(native_shadows::fixtureTable),native_shadows::sizes,16);
}
static bool Matches(bool doubled){
    for(const auto& s:native_shadows::fixtureSites){DWORD value;memcpy(&value,reinterpret_cast<void*>(s.address+1),4);if(value!=s.original*(doubled?2u:1u))return false;}
    for(unsigned i=0;i<4;++i)if(reinterpret_cast<DWORD*>(native_shadows::fixtureTable)[i]!=native_shadows::sizes[i]*(doubled?2u:1u))return false;
    return true;
}
int main(){
    using namespace native_shadows;
    void* pages[3]{};for(auto& page:pages){page=VirtualAlloc(nullptr,65536,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);Check(page!=nullptr,"reserve own fixture page");}
    for(unsigned i=0;i<siteCount;++i){fixtureSites[i]=sites[i];fixtureSites[i].address=reinterpret_cast<DWORD>(pages[sites[i].address<0x8f0000?0:1])+(sites[i].address&0xffffu);}
    fixtureTable=reinterpret_cast<DWORD>(pages[2])+(tableAddress&0xffffu);
    Populate();
    for(unsigned n=1;n<=siteCount+1;++n){protectCalls=0;failProtection=n;Check(!Patch(),"protection failure rejects whole transaction");Check(Matches(false),"no partial mutation on protection failure");}
    failProtection=0;
    for(unsigned n=0;n<siteCount+1;++n){
        Populate();BYTE* p=n<siteCount?reinterpret_cast<BYTE*>(fixtureSites[n].address):reinterpret_cast<BYTE*>(fixtureTable);const BYTE before=*p;*p^=1;
        protectCalls=0;Check(!Patch(),"signature failure rejects transaction");Check(protectCalls==0,"signature checked before protection changes");*p=before;Check(Matches(false),"signature failure preserved all other regions");
    }
    void* vt[119]{};vt[7]=reinterpret_cast<void*>(&Caps);struct Device{void** vt;}dev{vt};auto* device=reinterpret_cast<IDirect3DDevice9*>(&dev);
    ConfigureNativeShadowQuality(L"unused",false);Check(!enabled,"unsupported exact-build gate");
    option=false;ConfigureNativeShadowQuality(L"unused",true);Check(!enabled,"config off gate");option=true;
    for(unsigned n=0;n<3;++n){ConfigureNativeShadowQuality(L"unused",true);installation=0;maxWidth=n==0?1024:2048;maxHeight=n==1?1024:2048;capsResult=n==2?E_FAIL:S_OK;InstallNativeShadowQuality(device);Check(!installed&&Matches(false),"unsupported or failed caps leave native resources intact");}
    maxWidth=maxHeight=2048;capsResult=S_OK;installation=0;ConfigureNativeShadowQuality(L"unused",true);InstallNativeShadowQuality(device);
    Check(installed&&Matches(true),"full quality transaction installed");const unsigned calls=protectCalls;InstallNativeShadowQuality(device);Check(protectCalls==calls,"idempotent installation");
    for(void* p:pages)VirtualFree(p,0,MEM_RELEASE);
    printf("Native shadow quality: %u checks PASS\n",checks);return 0;
}
