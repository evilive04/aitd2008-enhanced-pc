#include "options_presentation.h"
namespace options_overlay {
static bool enabled=false,drawing=false;
static wchar_t assetDirectory[MAX_PATH]{};
static options_presentation::Presentation* presentation=nullptr;
static OptionsStateGetter getter=nullptr;static OptionsActionSender sender=nullptr;static OptionsPresentationSetter readySetter=nullptr;
static DWORD nextLookup=0,previous=~0u;
static DWORD readyGeneration=0;
static BOOL WINAPI SendAction(DWORD action,DWORD generation){const BOOL accepted=sender?sender(action,generation):FALSE;Log("Options UI action=%lu generation=%lu accepted=%d",action,generation,accepted);return accepted;}
static void Configure(const wchar_t* config,bool supported){
    enabled=supported&&ConfigFlag(L"UI",L"RemakeInterface",false,config);if(!enabled)return;
    wchar_t path[MAX_PATH]{};if(!GetModuleFileNameW(selfModule,path,MAX_PATH)){enabled=false;return;}auto* slash=wcsrchr(path,L'\\');if(!slash){enabled=false;return;}*slash=0;
    if(swprintf_s(assetDirectory,L"%s\\enhanced-ui\\theme",path)<0)enabled=false;
}
static void Invalidate(IDirect3DDevice9* device){if(drawing||!presentation||!presentation->OwnsDevice(device))return;presentation->ReleaseDeviceResources();if(readySetter)readySetter(FALSE,readyGeneration);}
static void Draw(IDirect3DDevice9* device,const RECT* source,const RECT* destination){
    if(!enabled||drawing)return;drawing=true;struct Scope{~Scope(){drawing=false;}}scope;
    bool ready=false;OptionsStateV1 state{};
    try{const DWORD now=GetTickCount();
        if((!getter||!sender||!readySetter)&&(!nextLookup||static_cast<LONG>(now-nextLookup)>=0)){
            nextLookup=now+1000;const auto input=GetModuleHandleW(L"dinput8.dll");if(input&&FfSameDirectory(input)){
                getter=reinterpret_cast<OptionsStateGetter>(GetProcAddress(input,"AITDGetOptionsState"));sender=reinterpret_cast<OptionsActionSender>(GetProcAddress(input,"AITDQueueOptionsAction"));readySetter=reinterpret_cast<OptionsPresentationSetter>(GetProcAddress(input,"AITDSetOptionsPresentationReady"));
            }
        }
        if(!source&&!destination&&getter&&sender&&readySetter&&getter(&state,sizeof(state))&&(state.flags&OptionsRootVisible)){
            if(!presentation)presentation=new options_presentation::Presentation;ready=presentation->Draw(device,state,SendAction,assetDirectory,menu_overlay::DrawBackdrop);
        }else if(presentation)presentation->ReleaseDeviceResources();
        const DWORD status=(getter?1u:0u)|(presentation?2u:0u)|(ready?4u:0u)|(state.flags<<8);
        if(status!=previous){previous=status;Log("Options art status=%08lx generation=%lu dispatches=%lu rejected=%lu",status,state.generation,state.dispatches,state.rejected);}
    }catch(...){ready=false;Log("Options art unavailable: original page/input retained");}
    if(ready)readyGeneration=state.generation;
    if(readySetter)readySetter(ready?TRUE:FALSE,ready?state.generation:readyGeneration);
}
}
