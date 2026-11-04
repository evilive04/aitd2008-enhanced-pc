// Exact-build rope host reference-rate correction. Render pacing, simulation
// clocks, camera ownership and the game's nominal physics setting stay native.
#include "rope_reference_rate.h"
namespace rope_reference_rate {
static bool requested=false;
static volatile LONG installAttempt=0;
static void Install(){
 if(!requested||InterlockedCompareExchange(&installAttempt,1,0))return;
 struct Site{DWORD address;DWORD protection;};
 Site sites[]={{0x715179,0},{0x7157e1,0},{0x7160f2,0}};
 const BYTE expected[]={0xdb,0x05,0xb8,0xec,0xfa,0x00};
 for(const auto& site:sites)if(memcmp(reinterpret_cast<const void*>(site.address),expected,sizeof expected)){
  Log("Rope timing correction skipped: reference-rate signature mismatch at %08lx",site.address);return;
 }
 unsigned writable=0;
 for(auto& site:sites){if(!VirtualProtect(reinterpret_cast<void*>(site.address),sizeof expected,PAGE_EXECUTE_READWRITE,&site.protection))break;++writable;}
 if(writable!=_countof(sites)){
  for(int n=static_cast<int>(writable)-1;n>=0;--n){DWORD ignored;VirtualProtect(reinterpret_cast<void*>(sites[n].address),sizeof expected,sites[n].protection,&ignored);}
  Log("Rope timing correction skipped: native page access failed");return;
 }
 for(const auto& site:sites){
  auto* code=reinterpret_cast<BYTE*>(site.address);code[0]=0xe8;
  const DWORD relative=static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(&Load)-site.address-5);
  memcpy(code+1,&relative,sizeof relative);code[5]=0x90;FlushInstructionCache(GetCurrentProcess(),code,sizeof expected);
 }
 bool restored=true;
 for(int n=static_cast<int>(_countof(sites))-1;n>=0;--n){DWORD ignored;if(!VirtualProtect(reinterpret_cast<void*>(sites[n].address),sizeof expected,sites[n].protection,&ignored))restored=false;}
 Log("Rope timing correction installed: 3 native rate loads, actual high-FPS delta; native <=60 FPS/substeps retained; page_protection=%d",restored);
}
}
