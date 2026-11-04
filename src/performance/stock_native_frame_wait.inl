// Own runtime change to the normal game's default frame waiting only. This
// is not an activation/protection hook and does not modify the disk image.
namespace stock_native_frame_wait {
static const float defaultRate=120.f;
static void* continuation=reinterpret_cast<void*>(0x41a1c2);
static volatile LONG state=0;
__declspec(naked) static void Adapter(){
 __asm {
  pushfd
  push eax
  mov eax,0x00faea8c
  cmp dword ptr [eax],0x42700000
  jne nativeRate
  fdiv dword ptr [defaultRate]
  jmp finished
 nativeRate:
  fdiv dword ptr [eax]
 finished:
  pop eax
  popfd
  jmp dword ptr [continuation]
 }
}
static bool Owned(){
 if(InterlockedCompareExchange(&state,0,0)!=2)return false;
 __try{
  const auto* site=reinterpret_cast<const BYTE*>(0x41a1bc);
  return site[0]==0xe9&&site[5]==0x90&&
   static_cast<DWORD>(0x41a1bc+5+*reinterpret_cast<const LONG*>(site+1))==reinterpret_cast<DWORD>(&Adapter);
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool Install(){
 if(InterlockedCompareExchange(&state,0,0)==2)return Owned();
 if(InterlockedCompareExchange(&state,1,0))return false;
 const BYTE expected[]={0xd8,0x35,0x8c,0xea,0xfa,0x00};
 auto* site=reinterpret_cast<BYTE*>(0x41a1bc);
 if(std::memcmp(site,expected,sizeof expected)){
  InterlockedExchange(&state,3);Log("Stock default frame wait skipped: exact native signature mismatch");return false;
 }
 DWORD protection{};
 if(!VirtualProtect(site,sizeof expected,PAGE_EXECUTE_READWRITE,&protection)){
  InterlockedExchange(&state,3);Log("Stock default frame wait skipped: page access error=%lu",GetLastError());return false;
 }
 BYTE patch[6]={0xe9,0,0,0,0,0x90};
 const LONG offset=static_cast<LONG>(reinterpret_cast<DWORD>(&Adapter)-(0x41a1bc+5));
 std::memcpy(patch+1,&offset,sizeof offset);std::memcpy(site,patch,sizeof patch);
 const bool flushed=FlushInstructionCache(GetCurrentProcess(),site,sizeof patch)!=FALSE;
 DWORD ignored{};const bool restored=VirtualProtect(site,sizeof patch,protection,&ignored)!=FALSE;
 InterlockedExchange(&state,2);
 Log("Stock default frame wait installed=%d default60_to120=1 explicit_other_caps_preserved=1 page_protection=%d instruction_cache=%d",Owned(),restored,flushed);
 return restored&&flushed&&Owned();
}
}
