#include "chapter_preview_identity.h"
#include "ui_paint_owner.h"
#include <d3d9.h>
namespace chapters_previews {
static bool enabled=false;static SRWLOCK lock=SRWLOCK_INIT;static DWORD generation{},lastSequence{};
struct Entry{chapter_preview_identity::Identity image{};};static Entry images[5]{};
static bool Read(void*,std::uint32_t at,void* out,std::size_t bytes){__try{memcpy(out,reinterpret_cast<void*>(at),bytes);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static void Clear(){for(auto& entry:images){if(entry.image.texture)pc_keycaps::ReleaseResource(entry.image.texture);if(entry.image.material)pc_keycaps::ReleaseResource(entry.image.material);entry={};}}
// Only the original UI-thread Tick releases engine references. Native reset
// continues to own its D3D handles; no engine cleanup is called from DllMain.
static void RefreshMissing();
static void Tick(){DWORD epoch{},flags{},sequence{};AcquireSRWLockShared(&chapters_context::lock);epoch=chapters_context::state.generation;flags=chapters_context::state.flags;sequence=chapters_context::state.offset+chapters_context::state.selected+1;ReleaseSRWLockShared(&chapters_context::lock);
 AcquireSRWLockExclusive(&lock);if(!enabled||!(flags&ChaptersVisible)||generation!=epoch){Clear();generation=epoch;lastSequence=sequence;}if(lastSequence!=sequence){auto& image=images[1].image;if(image.texture)pc_keycaps::ReleaseResource(image.texture);if(image.material)pc_keycaps::ReleaseResource(image.material);image={};lastSequence=sequence;}ReleaseSRWLockExclusive(&lock);
 if(enabled&&(flags&(ChaptersVisible|ChaptersBusy))==ChaptersVisible&&ui_paint_owner::rendererAvailable)RefreshMissing();
}
static void __cdecl Observe(DWORD self,DWORD material)noexcept{
 if(!enabled||self!=0xfe15f8)return;ChaptersStateV1 state{};AcquireSRWLockShared(&chapters_context::lock);state=chapters_context::state;ReleaseSRWLockShared(&chapters_context::lock);
 if((state.flags&(ChaptersVisible|ChaptersBusy))!=ChaptersVisible||GetTickCount()-state.tick>250)return;
 __try{
  const auto slot=*reinterpret_cast<DWORD*>(self+0x188);if(slot>=5)return;
  chapter_preview_identity::Identity image{};if(!chapter_preview_identity::Capture({Read,nullptr},{state.chapter,state.chapters,state.offset+state.selected+1,slot},material,image))return;
  AcquireSRWLockExclusive(&lock);
  __try{
   if(generation!=state.generation)__leave;auto& entry=images[slot];if(entry.image.material==image.material&&entry.image.texture==image.texture&&entry.image.textureName==image.textureName)__leave;
   if(!pc_keycaps::RetainPositive(reinterpret_cast<LONG*>(image.material+0xc)))__leave;
   if(!pc_keycaps::RetainPositive(reinterpret_cast<LONG*>(image.texture+0xc))){pc_keycaps::ReleaseResource(image.material);__leave;}
   const auto old=entry.image;entry.image=image;if(old.texture)pc_keycaps::ReleaseResource(old.texture);if(old.material)pc_keycaps::ReleaseResource(old.material);
  }__finally{ReleaseSRWLockExclusive(&lock);}
 }__except(EXCEPTION_EXECUTE_HANDLER){}
}
// The original "vN"/"lure" material callbacks normally run from paint. Once
// original pixels are suppressed, request the same materials on the UI thread
// without drawing any original widget. Restore both native scratch fields.
using NativeNotify=void(__thiscall*)(void*,void*,const char*,DWORD);
static NativeNotify notify=reinterpret_cast<NativeNotify>(0x53df90);
static void RefreshMissing(){
 __try{
  if(*reinterpret_cast<DWORD*>(0xfe15f8)!=0xc778f0||(*reinterpret_cast<DWORD*>(0xff4290)!=33&&*reinterpret_cast<DWORD*>(0xff4290)!=34))return;
  auto* movie=*reinterpret_cast<BYTE**>(0xfe15f0);if(!movie||*reinterpret_cast<DWORD*>(movie+12)!=0xfe15f8||!movie[0x65])return;
  const auto savedMaterial=*reinterpret_cast<DWORD*>(0xfe15fc),savedSlot=*reinterpret_cast<DWORD*>(0xfe1780);
  __try{
   for(DWORD slot=1;slot<2;++slot){AcquireSRWLockShared(&lock);const bool missing=!images[slot].image.texture;ReleaseSRWLockShared(&lock);if(!missing)continue;
    char name[3]={'v',static_cast<char>('0'+slot),0};notify(reinterpret_cast<void*>(0xfe15f8),movie,name,0);notify(reinterpret_cast<void*>(0xfe15f8),movie,"lure",0);
    Observe(0xfe15f8,*reinterpret_cast<DWORD*>(0xfe15fc));
   }
  }__finally{*reinterpret_cast<DWORD*>(0xfe15fc)=savedMaterial;*reinterpret_cast<DWORD*>(0xfe1780)=savedSlot;}
 }__except(EXCEPTION_EXECUTE_HANDLER){}
}
static HRESULT Acquire(DWORD slot,DWORD epoch,IDirect3DTexture9** output)noexcept{
 if(!output||slot>=5)return E_INVALIDARG;*output=nullptr;if(!enabled)return S_FALSE;
 DWORD current{},flags{},tick{};AcquireSRWLockShared(&chapters_context::lock);current=chapters_context::state.generation;flags=chapters_context::state.flags;tick=chapters_context::state.tick;ReleaseSRWLockShared(&chapters_context::lock);
 if(epoch!=current||!(flags&ChaptersVisible)||GetTickCount()-tick>250)return S_FALSE;
 HRESULT result=S_FALSE;AcquireSRWLockShared(&lock);
 __try{__try{
  if(epoch!=generation||!images[slot].image.texture)__leave;const auto& image=images[slot].image;const auto texture=image.texture;
  if(*reinterpret_cast<DWORD*>(texture)!=0xcc74f8||*reinterpret_cast<DWORD*>(texture+4)!=0x54584554||*reinterpret_cast<ULONGLONG*>(texture+0x10)!=image.textureName||*reinterpret_cast<DWORD*>(texture+0x34)!=1)__leave;
  auto* deviceTexture=*reinterpret_cast<IUnknown**>(texture+0x48);if(!deviceTexture)__leave;
  result=deviceTexture->QueryInterface(__uuidof(IDirect3DTexture9),reinterpret_cast<void**>(output));
 }__except(EXCEPTION_EXECUTE_HANDLER){result=E_FAIL;}}
 __finally{ReleaseSRWLockShared(&lock);}return result;
}
}
