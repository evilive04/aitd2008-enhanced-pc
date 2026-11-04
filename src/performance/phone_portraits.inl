#include "phone_portrait_identity.h"
#include "phone_portraits_state.h"
#include <d3d9.h>
namespace phone_portraits {
static bool enabled=false;static SRWLOCK lock=SRWLOCK_INIT;static DWORD generation{};
static phone_portrait_identity::Identity images[3]{};static unsigned captureStage[3]{};
struct Roster{DWORD epoch{},tick{},root{},count{},route[5]{};};
static bool Snapshot(Roster& out){out={};AcquireSRWLockShared(&phone_parameters_context::lock);const auto& s=phone_parameters_context::state;
 const bool valid=enabled&&(s.flags&(OptionsChildVisible|OptionsChildActionsProven))==(OptionsChildVisible|OptionsChildActionsProven)&&(s.nativeMode==2||s.nativeMode==5)&&s.view.count<=5&&GetTickCount()-s.tick<=250;
 if(valid){out.epoch=s.generation;out.tick=s.tick;out.root=phone_parameters_context::previous.root;out.count=s.view.count;for(unsigned n=0;n<out.count;++n)out.route[n]=phone_portrait_identity::Route(s.view.rows[n].value);}
 ReleaseSRWLockShared(&phone_parameters_context::lock);return valid&&out.root;
}
static void Release(phone_portrait_identity::Identity& image){if(image.texture)pc_keycaps::ReleaseResource(image.texture);if(image.material)pc_keycaps::ReleaseResource(image.material);image={};}
static void Clear(){for(auto& image:images)Release(image);for(auto& stage:captureStage)stage=0;}
// Engine references are acquired/released only by the real UI-thread Tick,
// never by a D3D reset callback or DllMain.
static void Tick(){Roster roster{};const bool active=Snapshot(roster);AcquireSRWLockExclusive(&lock);
 __try{__try{
  if(!active||generation!=roster.epoch){Clear();generation=roster.epoch;}if(!active)__leave;
  for(unsigned route=1;route<=3;++route){auto& image=images[route-1];phone_portrait_identity::Identity next{};
   if(!phone_portrait_identity::Capture({phone_context::Read,nullptr},roster.root,route,next,captureStage+route-1)){Release(image);continue;}
   if(image.material==next.material&&image.texture==next.texture&&image.name==next.name)continue;
   if(!pc_keycaps::RetainPositive(reinterpret_cast<LONG*>(next.material+0xc)))continue;
   if(!pc_keycaps::RetainPositive(reinterpret_cast<LONG*>(next.texture+0xc))){pc_keycaps::ReleaseResource(next.material);continue;}
   const auto old=image;image=next;if(old.texture)pc_keycaps::ReleaseResource(old.texture);if(old.material)pc_keycaps::ReleaseResource(old.material);
  }
 }__except(EXCEPTION_EXECUTE_HANDLER){}}
 __finally{ReleaseSRWLockExclusive(&lock);}
}
static HRESULT Acquire(DWORD row,DWORD epoch,IDirect3DTexture9** output)noexcept{
 if(!output)return E_INVALIDARG;*output=nullptr;Roster roster{};if(!Snapshot(roster)||epoch!=roster.epoch||row>=roster.count)return S_FALSE;const auto route=roster.route[row];if(route<1||route>3)return S_FALSE;
 HRESULT result=S_FALSE;AcquireSRWLockShared(&lock);
 __try{__try{
  if(generation!=epoch)__leave;const auto& image=images[route-1];if(!image.texture)__leave;const auto texture=image.texture;
  if(*reinterpret_cast<DWORD*>(texture)!=0xcc74f8||*reinterpret_cast<DWORD*>(texture+4)!=0x54584554||*reinterpret_cast<ULONGLONG*>(texture+0x10)!=image.name||*reinterpret_cast<DWORD*>(texture+0x34)!=1)__leave;
  auto* native=*reinterpret_cast<IUnknown**>(texture+0x48);if(!native)__leave;
  result=native->QueryInterface(__uuidof(IDirect3DTexture9),reinterpret_cast<void**>(output));
 }__except(EXCEPTION_EXECUTE_HANDLER){result=E_FAIL;}}
 __finally{ReleaseSRWLockShared(&lock);}return result;
}
}
