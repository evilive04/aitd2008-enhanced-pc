#include "pause_presentation.h"
namespace pause_overlay {
static bool drawing=false;
static pause_presentation::Presentation* presentation=nullptr;
static PauseGetter getter=nullptr;static PauseSender sender=nullptr;static PauseReady setter=nullptr;
static DWORD lookupTick{},readyGeneration{},previous=~0u;
static BOOL WINAPI Send(DWORD slot,DWORD generation){const auto accepted=sender?sender(slot,generation):FALSE;Log("Pause UI slot=%lu generation=%lu accepted=%d",slot,generation,accepted);return accepted;}
static void Invalidate(IDirect3DDevice9* device){if(drawing||!presentation||!presentation->OwnsDevice(device))return;presentation->ReleaseDeviceResources();if(setter)setter(FALSE,readyGeneration);}
static void Draw(IDirect3DDevice9* device,const RECT* source,const RECT* destination){
    if(!dialog_overlay::enabled||drawing)return;drawing=true;struct Scope{~Scope(){drawing=false;}}scope;bool painted=false;PauseStateV1 state{};
    try{
        const DWORD now=GetTickCount();if((!getter||!sender||!setter)&&(!lookupTick||static_cast<LONG>(now-lookupTick)>=0)){
            lookupTick=now+1000;const auto input=GetModuleHandleW(L"dinput8.dll");if(input&&FfSameDirectory(input)){
                getter=reinterpret_cast<PauseGetter>(GetProcAddress(input,"AITDGetPauseState"));sender=reinterpret_cast<PauseSender>(GetProcAddress(input,"AITDQueuePauseAction"));setter=reinterpret_cast<PauseReady>(GetProcAddress(input,"AITDSetPausePresentationReady"));
            }
        }
        if(!source&&!destination&&getter&&sender&&setter&&getter(&state,sizeof state)&&(state.flags&PauseVisible)){
            if(!presentation)presentation=new pause_presentation::Presentation;painted=presentation->Draw(device,state,Send);
        }else if(presentation)presentation->ReleaseDeviceResources();
        const DWORD status=(getter?1u:0u)|(presentation?2u:0u)|(painted?4u:0u)|(state.flags<<8);
        if(status!=previous){previous=status;Log("Pause art status=%08lx generation=%lu enabled=%02lx dispatched=%lu rejected=%lu build=%08lx",status,state.generation,state.enabledMask,state.dispatches,state.rejected,static_cast<unsigned long>(presentation?presentation->BuildStatus():S_OK));}
    }catch(...){painted=false;Log("Pause presenter unavailable; original pause retained");}
    if(painted)readyGeneration=state.generation;if(setter)setter(painted?TRUE:FALSE,painted?state.generation:readyGeneration);
}
}
