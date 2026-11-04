#include <d3d9.h>
namespace loading_tip_images{
static SRWLOCK lock=SRWLOCK_INIT;static DWORD generation{},picture{};static unsigned captureStage{};static native_bitmap_identity::Identity image{};
struct Snapshot{DWORD epoch{},root{},picture{};};
static bool Current(Snapshot& out){out={};AcquireSRWLockShared(&loading_context::lock);const auto& s=loading_context::state;
 const bool valid=loading_context::enabled&&s.flags==(LoadingVisible|LoadingInformation)&&s.nativeMode==9&&s.picture>=1&&s.picture<=5&&GetTickCount()-s.tick<=250;
 if(valid)out={s.generation,loading_context::previous.root,s.picture};ReleaseSRWLockShared(&loading_context::lock);return valid&&out.root;
}
static void Release(){if(image.texture)pc_keycaps::ReleaseResource(image.texture);if(image.material)pc_keycaps::ReleaseResource(image.material);image={};captureStage=0;}
// Native MAT/TEXT lifetime changes only on the original UI thread. D3D Reset
// continues to own its handles; no engine cleanup is called from DllMain.
static void Tick(){Snapshot s{};const bool active=Current(s);AcquireSRWLockExclusive(&lock);
 __try{__try{
  if(!active||generation!=s.epoch||picture!=s.picture){Release();generation=s.epoch;picture=s.picture;}if(!active)__leave;
  native_bitmap_identity::Identity next{};
  if(!native_bitmap_identity::Capture({phone_context::Read,nullptr},s.root,loading_tip_content::pictures[s.picture-1].image,next,&captureStage)){Release();__leave;}
  if(image.material==next.material&&image.texture==next.texture&&image.name==next.name)__leave;
  if(!pc_keycaps::RetainPositive(reinterpret_cast<LONG*>(next.material+0xc))){Release();__leave;}
  if(!pc_keycaps::RetainPositive(reinterpret_cast<LONG*>(next.texture+0xc))){pc_keycaps::ReleaseResource(next.material);Release();__leave;}
  const auto old=image;image=next;if(old.texture)pc_keycaps::ReleaseResource(old.texture);if(old.material)pc_keycaps::ReleaseResource(old.material);
 }__except(EXCEPTION_EXECUTE_HANDLER){Release();}}
 __finally{ReleaseSRWLockExclusive(&lock);}
}
static HRESULT Acquire(DWORD epoch,IDirect3DTexture9** output)noexcept{
 if(!output)return E_INVALIDARG;*output=nullptr;Snapshot s{};if(!Current(s)||epoch!=s.epoch)return S_FALSE;HRESULT result=S_FALSE;AcquireSRWLockShared(&lock);
 __try{__try{
  if(generation!=epoch||picture!=s.picture||!image.texture)__leave;const auto texture=image.texture;
  if(*reinterpret_cast<DWORD*>(texture)!=0xcc74f8||*reinterpret_cast<DWORD*>(texture+4)!=0x54584554||*reinterpret_cast<ULONGLONG*>(texture+0x10)!=image.name||*reinterpret_cast<DWORD*>(texture+0x34)!=1)__leave;
  auto* native=*reinterpret_cast<IUnknown**>(texture+0x48);if(native)result=native->QueryInterface(__uuidof(IDirect3DTexture9),reinterpret_cast<void**>(output));
 }__except(EXCEPTION_EXECUTE_HANDLER){result=E_FAIL;}}
 __finally{ReleaseSRWLockShared(&lock);}return result;
}
}
