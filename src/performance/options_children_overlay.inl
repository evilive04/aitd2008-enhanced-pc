#include "options_children_presentation.h"
namespace options_children_overlay {
static bool drawing=false;
static options_children_presentation::Presentation* presentation=nullptr;
static OptionsChildrenGetter getter=nullptr;static OptionsChildrenSender sender=nullptr;static OptionsPresentationSetter readySetter=nullptr;
static DWORD nextLookup=0,previous=~0u,readyGeneration=0;
static BOOL WINAPI SendAction(DWORD action,LONG row,double normalized,DWORD generation){const BOOL accepted=sender?sender(action,row,normalized,generation):FALSE;Log("Options child action=%lu row=%ld value=%.4f generation=%lu accepted=%d",action,row,normalized,generation,accepted);return accepted;}
static void Invalidate(IDirect3DDevice9* device){if(drawing||!presentation||!presentation->OwnsDevice(device))return;presentation->ReleaseDeviceResources();if(readySetter)readySetter(FALSE,readyGeneration);}
static void Draw(IDirect3DDevice9* device,const RECT* source,const RECT* destination){
    if(!options_overlay::enabled||drawing)return;drawing=true;struct Scope{~Scope(){drawing=false;}}scope;
    bool ready=false;OptionsChildrenStateV1 state{};
    try{const DWORD now=GetTickCount();
        if((!getter||!sender||!readySetter)&&(!nextLookup||static_cast<LONG>(now-nextLookup)>=0)){
            nextLookup=now+1000;const auto input=GetModuleHandleW(L"dinput8.dll");if(input&&FfSameDirectory(input)){
                getter=reinterpret_cast<OptionsChildrenGetter>(GetProcAddress(input,"AITDGetOptionsChildrenState"));sender=reinterpret_cast<OptionsChildrenSender>(GetProcAddress(input,"AITDQueueOptionsChildrenAction"));readySetter=reinterpret_cast<OptionsPresentationSetter>(GetProcAddress(input,"AITDSetOptionsChildrenPresentationReady"));
            }
        }
        if(!source&&!destination&&getter&&sender&&readySetter&&getter(&state,sizeof(state))&&(state.flags&(OptionsChildVisible|OptionsChildActionsProven))==(OptionsChildVisible|OptionsChildActionsProven)){
            if(!presentation)presentation=new options_children_presentation::Presentation;ready=presentation->Draw(device,state,SendAction,options_overlay::assetDirectory,menu_overlay::DrawBackdrop);
        }else if(presentation)presentation->ReleaseDeviceResources();
        const DWORD status=(getter?1u:0u)|(presentation?2u:0u)|(ready?4u:0u)|(state.flags<<8);
        if(status!=previous){previous=status;Log("Options child art status=%08lx generation=%lu mode=%lu dispatches=%lu rejected=%lu",status,state.generation,state.nativeMode,state.dispatches,state.rejected);}
    }catch(...){ready=false;Log("Options child art unavailable: original page/input retained");}
    if(ready)readyGeneration=state.generation;
    if(readySetter)readySetter(ready?TRUE:FALSE,ready?state.generation:readyGeneration);
}
}
