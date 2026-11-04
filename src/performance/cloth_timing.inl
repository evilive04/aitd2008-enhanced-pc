// Exact supported-build geometric cloth-rigidity correction. The native solver
// and its dispatch, actual dt, damping, forces, collision and
// Compress/Stretch response coefficients remain unchanged.
#include "cloth_rigidity_rate.h"
namespace cloth_rigidity_rate {
static bool requested=false;
static volatile LONG installAttempt=0;
static void Report(){
 if(!requested)return;
 const LONG count=InterlockedCompareExchange(&highFrequencyApplications,0,0);
 static volatile LONG reported=0;
 if(count>0&&!InterlockedCompareExchange(&reported,1,0))
  Log("Cloth high-FPS rigidity path executed: applications=%ld; geometric bands only, actual simulation delta retained",count);
}
#include "cloth_async_timing.inl"
static void Install(){
 if(!requested||InterlockedCompareExchange(&installAttempt,1,0))return;
 struct Site{DWORD address;const BYTE* expected;void* adapter;DWORD protection;};
 const BYTE rigid0[]={0xf3,0x0f,0x59,0xc1,0x0f,0x28,0xd4};
 const BYTE rigid3[]={0xf3,0x0f,0x59,0xd9,0x0f,0x28,0xd4};
 Site sites[]={
  {0x4e8500,rigid0,reinterpret_cast<void*>(&ScaleRigid0),0},
  {0x4e853e,rigid3,reinterpret_cast<void*>(&ScaleRigid3),0},
  {0x4e855f,rigid3,reinterpret_cast<void*>(&ScaleRigid3),0},
  {0x4e8596,rigid3,reinterpret_cast<void*>(&ScaleRigid3),0}
 };
 // Authenticate the producer too: no writes toXMM1 occur between this exact
 // reciprocal-frequency/60 multiply and the four sites (nativeCOMISS reads it).
 const BYTE producer[]={0x0f,0x28,0xcc,0xf3,0x0f,0x5e,0x4b,0x40,
  0xf3,0x0f,0x11,0x4c,0x24,0x34,0xf3,0x0f,0x59,0x0d,0x74,0x8f,0xd1,0x00};
 if(*reinterpret_cast<const DWORD*>(0xd18f74)!=0x42700000u||
    memcmp(reinterpret_cast<const void*>(0x4e8436),producer,sizeof producer)){
  Log("Cloth rigidity correction skipped: native factor signature mismatch");return;
 }
 for(const auto& site:sites)if(memcmp(reinterpret_cast<const void*>(site.address),site.expected,sizeof rigid0)){
  Log("Cloth rigidity correction skipped: signature mismatch at %08lx",site.address);return;
 }
 unsigned writable=0;
 for(auto& site:sites){
  if(!VirtualProtect(reinterpret_cast<void*>(site.address),sizeof rigid0,PAGE_EXECUTE_READWRITE,&site.protection))break;
  ++writable;
 }
 if(writable!=_countof(sites)){
  for(int n=static_cast<int>(writable)-1;n>=0;--n){DWORD ignored;VirtualProtect(reinterpret_cast<void*>(sites[n].address),sizeof rigid0,sites[n].protection,&ignored);}
  Log("Cloth rigidity correction skipped: native page access failed");return;
 }
 for(const auto& site:sites){
  auto* code=reinterpret_cast<BYTE*>(site.address);code[0]=0xe8;
  const DWORD relative=static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(site.adapter)-site.address-5);
  memcpy(code+1,&relative,sizeof relative);code[5]=0x90;code[6]=0x90;
  FlushInstructionCache(GetCurrentProcess(),code,sizeof rigid0);
 }
 bool restored=true;
 for(int n=static_cast<int>(_countof(sites))-1;n>=0;--n){
  DWORD ignored;if(!VirtualProtect(reinterpret_cast<void*>(sites[n].address),sizeof rigid0,sites[n].protection,&ignored))restored=false;
 }
 Log("Cloth rigidity correction installed: 4 geometric deadzone sites; authored60Hz bands above60Hz, native dt/forces/damping retained; page_protection=%d",restored);
 InstallAsync();
}
}
