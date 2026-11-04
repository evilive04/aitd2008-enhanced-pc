#include "native_bitmap_identity.h"
#include <d3d9.h>
namespace inventory_ui_images {
static SRWLOCK lock=SRWLOCK_INIT;static DWORD generation{};
static native_bitmap_identity::Identity images[3]{};
static void Release(native_bitmap_identity::Identity& image){if(image.texture)pc_keycaps::ReleaseResource(image.texture);if(image.material)pc_keycaps::ReleaseResource(image.material);image={};}
static void Tick(){
 InventoryUiStateV1 s{};DWORD root{};AcquireSRWLockShared(&inventory_ui_context::lock);s=inventory_ui_context::state;root=inventory_ui_context::previous.root;ReleaseSRWLockShared(&inventory_ui_context::lock);
 AcquireSRWLockExclusive(&lock);__try{__try{
  if(generation!=s.generation||!(s.flags&InventoryUiVisible)){for(auto& image:images)Release(image);generation=s.generation;}
  for(unsigned n=0;n<3;++n){const auto& spec=s.images[n];native_bitmap_identity::Identity next{};
   if(!root||!spec.bitmap||!(s.flags&InventoryUiVisible)||!native_bitmap_identity::Capture({phone_context::Read,nullptr},root,{spec.bitmap,spec.width,spec.height},next)){Release(images[n]);continue;}
   auto& image=images[n];if(image.texture==next.texture&&image.material==next.material&&image.name==next.name)continue;
   if(!pc_keycaps::RetainPositive(reinterpret_cast<LONG*>(next.material+12)))continue;
   if(!pc_keycaps::RetainPositive(reinterpret_cast<LONG*>(next.texture+12))){pc_keycaps::ReleaseResource(next.material);continue;}
   Release(image);image=next;
  }
 }__except(EXCEPTION_EXECUTE_HANDLER){}}__finally{ReleaseSRWLockExclusive(&lock);}
}
static HRESULT Acquire(DWORD slot,DWORD epoch,IDirect3DTexture9** output)noexcept{
 if(!output||slot>=3)return E_INVALIDARG;*output=nullptr;InventoryUiStateV1 s{};
 if(!inventory_ui_context::GetState(&s,sizeof s)||s.generation!=epoch||!(s.flags&InventoryUiVisible))return S_FALSE;
 HRESULT result=S_FALSE;AcquireSRWLockShared(&lock);__try{__try{
  if(generation!=epoch||!images[slot].texture)__leave;const auto& image=images[slot];const auto texture=image.texture;
  if(*reinterpret_cast<DWORD*>(texture)!=0xcc74f8||*reinterpret_cast<DWORD*>(texture+4)!=0x54584554||*reinterpret_cast<ULONGLONG*>(texture+16)!=image.name||*reinterpret_cast<DWORD*>(texture+0x34)!=1)__leave;
  auto* native=*reinterpret_cast<IUnknown**>(texture+0x48);if(native)result=native->QueryInterface(__uuidof(IDirect3DTexture9),reinterpret_cast<void**>(output));
 }__except(EXCEPTION_EXECUTE_HANDLER){result=E_FAIL;}}__finally{ReleaseSRWLockShared(&lock);}return result;
}
}
