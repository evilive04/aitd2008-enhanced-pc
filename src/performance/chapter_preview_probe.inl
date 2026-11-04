// Shared original-getter adapter. Opt-in diagnostics copy bounded metadata;
// the optional chapter presenter consumer separately manages native references.
namespace chapter_preview_probe {
static bool enabled=false;static void* original=nullptr;
using Consumer=void(__cdecl*)(DWORD,DWORD);static Consumer consumer=nullptr;
static SRWLOCK lock=SRWLOCK_INIT;
struct Record{DWORD chapter{},slot{},material{},texture{},tick{};BYTE materialHeader[256]{},textureHeader[128]{};};
static Record records[64]{};static DWORD count{};
static void __cdecl Observe(DWORD self,DWORD material)noexcept{
 const auto error=GetLastError();
 if(consumer)consumer(self,material);
 __try{
  if(!enabled||self!=0xfe15f8||!material||*reinterpret_cast<DWORD*>(material+4)!=0x2054414d||
     *reinterpret_cast<DWORD*>(self)!=0xc778f0||(*reinterpret_cast<DWORD*>(0xff4290)!=34&&*reinterpret_cast<DWORD*>(0xff4290)!=33))__leave;
  auto* movie=*reinterpret_cast<BYTE**>(0xfe15f0);if(!movie||*reinterpret_cast<DWORD*>(movie+12)!=self||!movie[0x65])__leave;
  DWORD chapter{},tick{},flags{};AcquireSRWLockShared(&chapters_context::lock);chapter=chapters_context::state.chapter;tick=chapters_context::state.tick;flags=chapters_context::state.flags;ReleaseSRWLockShared(&chapters_context::lock);const auto slot=*reinterpret_cast<DWORD*>(self+0x188);
  if((flags&(ChaptersVisible|ChaptersBusy))!=ChaptersVisible||GetTickCount()-tick>250||slot>6)__leave;
  AcquireSRWLockExclusive(&lock);
  __try{
   if(count==64)__leave;
   for(DWORD n=0;n<count;++n)if(records[n].chapter==chapter&&records[n].slot==slot&&records[n].material==material)__leave;
   Record record{};record.chapter=chapter;record.slot=slot;record.material=material;record.tick=GetTickCount();memcpy(record.materialHeader,reinterpret_cast<void*>(material),256);
   auto node=*reinterpret_cast<DWORD*>(material+0x78);DWORD seen[64]{};unsigned used=0;
   while(node&&node!=0x121bff8&&used<64){
    bool duplicate=false;for(unsigned n=0;n<used;++n)duplicate|=seen[n]==node;if(duplicate)break;seen[used++]=node;
    const auto next=*reinterpret_cast<DWORD*>(node+0x14);if(next==0x121bff8){
     const auto texture=*reinterpret_cast<DWORD*>(node+0xc);
     if(texture&&*reinterpret_cast<DWORD*>(texture+4)==0x54584554){record.texture=texture;memcpy(record.textureHeader,reinterpret_cast<void*>(texture),128);}break;
    }node=next;
   }
   records[count++]=record;
  }__finally{ReleaseSRWLockExclusive(&lock);}
 }__except(EXCEPTION_EXECUTE_HANDLER){}
 SetLastError(error);
}
// Original53DF70 is THISCALL/RET8 and leaves the native result in EAX.
// Observe runs after that exact getter and preserves the resulting machine state.
static __declspec(naked)void Adapter(){__asm{
 push dword ptr [esp+8]
 push dword ptr [esp+8]
 call dword ptr [original]
 pushfd
 pushad
 mov esi,esp
 sub esp,528
 and esp,0fffffff0h
 fxsave [esp]
 push dword ptr [esi+28]
 push dword ptr [esi+24]
 call Observe
 add esp,8
 fxrstor [esp]
 mov esp,esi
 popad
 popfd
 ret 8
}}
static bool Install(){
 if(!enabled&&!consumer)return false;if(original)return true;auto* entry=reinterpret_cast<BYTE*>(0x53df70);const BYTE expected[]={0x8b,0x41,0x04,0x85,0xc0};if(memcmp(entry,expected,5))return false;
 auto* code=static_cast<BYTE*>(VirtualAlloc(nullptr,10,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));if(!code)return false;memcpy(code,entry,5);code[5]=0xe9;
 const DWORD back=static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(entry+5)-reinterpret_cast<ULONG_PTR>(code+10));memcpy(code+6,&back,4);DWORD protection{},entryProtection{};
 if(!VirtualProtect(code,10,PAGE_EXECUTE_READ,&protection)||!VirtualProtect(entry,5,PAGE_EXECUTE_READWRITE,&entryProtection)){VirtualFree(code,0,MEM_RELEASE);return false;}
 original=code;const DWORD jump=static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(&Adapter)-reinterpret_cast<ULONG_PTR>(entry+5));memcpy(entry+1,&jump,4);entry[0]=0xe9;FlushInstructionCache(GetCurrentProcess(),code,10);FlushInstructionCache(GetCurrentProcess(),entry,5);DWORD ignored{};VirtualProtect(entry,5,entryProtection,&ignored);return true;
}
}
