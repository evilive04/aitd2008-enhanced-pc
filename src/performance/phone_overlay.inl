#include "phone_presentation.h"
namespace phone_overlay {
static bool drawing=false;static phone_presentation::Presentation* presentation=nullptr;
static PhoneGetter getter=nullptr;static PhoneSender sender=nullptr;static PhoneReady setter=nullptr;
static DWORD lookupTick{},readyGeneration{},previous=~0u;
static BOOL WINAPI Send(DWORD action,DWORD generation){const auto accepted=sender?sender(action,generation):FALSE;Log("Phone UI action=%lu generation=%lu accepted=%d",action,generation,accepted);return accepted;}
static void Invalidate(IDirect3DDevice9* device){if(drawing||!presentation||!presentation->OwnsDevice(device))return;presentation->ReleaseDeviceResources();if(setter)setter(FALSE,readyGeneration);}
static void Draw(IDirect3DDevice9* device,const RECT* source,const RECT* destination){
 if(!dialog_overlay::enabled||drawing)return;drawing=true;struct Scope{~Scope(){drawing=false;}}scope;bool painted=false;PhoneStateV1 state{};
 try{
  const auto now=GetTickCount();if((!getter||!sender||!setter)&&(!lookupTick||static_cast<LONG>(now-lookupTick)>=0)){
   lookupTick=now+1000;const auto input=GetModuleHandleW(L"dinput8.dll");if(input&&FfSameDirectory(input)){
    getter=reinterpret_cast<PhoneGetter>(GetProcAddress(input,"AITDGetPhoneState"));sender=reinterpret_cast<PhoneSender>(GetProcAddress(input,"AITDQueuePhoneAction"));setter=reinterpret_cast<PhoneReady>(GetProcAddress(input,"AITDSetPhonePresentationReady"));
   }
  }
  if(!source&&!destination&&getter&&sender&&setter&&getter(&state,sizeof state)&&(state.flags&PhoneHome)&&!(state.flags&PhoneDirectMap)){
   if(!presentation)presentation=new phone_presentation::Presentation;painted=presentation->Draw(device,state,Send);
  }else if(presentation)presentation->ReleaseDeviceResources();
  const DWORD status=(getter?1u:0u)|(presentation?2u:0u)|(painted?4u:0u)|(state.flags<<8);
  if(status!=previous){previous=status;Log("Phone art status=%08lx generation=%lu page=%lu count=%lu sent=%lu rejected=%lu",status,state.generation,state.nativePage,state.count,state.dispatches,state.rejected);}
 }catch(...){painted=false;Log("Phone presenter unavailable; native path retained");}
 if(painted)readyGeneration=state.generation;if(setter)setter(painted?TRUE:FALSE,painted?state.generation:readyGeneration);
}
}
