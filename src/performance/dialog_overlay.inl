#include "dialog_presentation.h"
namespace dialog_overlay {
static bool enabled=false,drawing=false;
static dialog_presentation::Presentation* presentation=nullptr;
static DialogStateGetter getter=nullptr;
static DialogActionSender sender=nullptr;
using ReadySetter=BOOL(WINAPI*)(BOOL);
static ReadySetter setReady=nullptr;
static DWORD nextLookup=0,previous=~0u;
static BOOL WINAPI SendAction(DWORD slot,DWORD generation){const BOOL accepted=sender?sender(slot,generation):FALSE;Log("Dialog UI action slot=%lu generation=%lu accepted=%d",slot,generation,accepted);return accepted;}
static void Configure(const wchar_t* config,bool supported){enabled=supported&&ConfigFlag(L"UI",L"RemakeInterface",false,config);}
static void Invalidate(IDirect3DDevice9* device){if(drawing||!presentation||!presentation->OwnsDevice(device))return;presentation->ReleaseDeviceResources();if(setReady)setReady(FALSE);}
static void Draw(IDirect3DDevice9* device,const RECT* source,const RECT* destination){
    if(!enabled||drawing)return;drawing=true;
    struct Scope{~Scope(){drawing=false;}} scope;bool ready=false;
    try{
        const DWORD now=GetTickCount();
        if((!getter||!sender||!setReady)&&(!nextLookup||static_cast<LONG>(now-nextLookup)>=0)){
            nextLookup=now+1000;auto input=GetModuleHandleW(L"dinput8.dll");
            if(input&&FfSameDirectory(input)){
                getter=reinterpret_cast<DialogStateGetter>(GetProcAddress(input,"AITDGetDialogState"));
                sender=reinterpret_cast<DialogActionSender>(GetProcAddress(input,"AITDQueueDialogAction"));
                setReady=reinterpret_cast<ReadySetter>(GetProcAddress(input,"AITDSetDialogPresentationReady"));
            }
        }
        DialogStateV1 state{};
        if(!source&&!destination&&getter&&sender&&setReady&&getter(&state,sizeof(state))&&state.size==sizeof(state)&&state.version==1&&(state.flags&DialogReady)&&now-state.tick<=250){
            // The supported Choice contract is first=No/result2. Resume only
            // after the UI thread executed No for our pending menu confirmation,
            // never just because a second input was accepted into a queue.
            if((state.flags&DialogChoice)&&state.mode==3&&state.firstResult==2&&state.secondResult==1)
                menu_overlay::ObserveConfirmation(state.generation,state.action,state.pendingResult,(state.flags&DialogClosing)!=0);
            if(!presentation)presentation=new dialog_presentation::Presentation;
            ready=presentation->Draw(device,state,SendAction,menu_overlay::DrawBackdrop);
        }else if(presentation)presentation->ReleaseDeviceResources();
        const DWORD diagnostic=(getter?1u:0u)|(presentation?2u:0u)|(ready?4u:0u)|(state.flags<<8);
        if(diagnostic!=previous){previous=diagnostic;Log("Dialog presentation status=%08lx generation=%lu mode=%lu native_action=%lu",diagnostic,state.generation,state.mode,state.action);}
    }catch(...){ready=false;Log("Dialog presentation unavailable: original dialog/input retained");}
    if(setReady)setReady(ready?TRUE:FALSE);
}
}
