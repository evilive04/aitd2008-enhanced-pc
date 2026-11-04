// Include once in the input component; compile dialog_context/text_capture.cpp.
// Configure only after exact-r29 image authentication AND RemakeInterface opt-in.
#include "dialog_context.h"
#include <cstring>
namespace dialog_native_hook {
static volatile LONG configured=0,initialized=0,installed=0,presentation=0,presentationTick=0;
static volatile LONG presentationGeneration=0,closingFence=0,closingTick=0;
static UINT configuredCodePage=0;
static void* nativeOpen=reinterpret_cast<void*>(0x54af80);
#ifdef AITD_DIALOG_HOOK_TEST
#define AITD_DIALOG_HOOK_VISIBLE __declspec(dllexport)
#else
#define AITD_DIALOG_HOOK_VISIBLE
#endif
AITD_DIALOG_HOOK_VISIBLE __declspec(noinline) void __cdecl AfterNativeOpen(DWORD mode,DWORD success){
    if(success&255)dialog_context::ObserveOpen(mode);
}
AITD_DIALOG_HOOK_VISIBLE __declspec(naked) void OpenAdapter(){
    __asm{
        push ebp
        mov ebp,esp
        push [ebp+16]
        push [ebp+12]
        push [ebp+8]
        call dword ptr [nativeOpen]
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        movzx ecx,al
        push ecx
        push [ebp+8]
        call AfterNativeOpen
        add esp,8
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        ret 12
    }
}
static void Configure(bool exactR29,bool remakeInterface,UINT codePage=0){
    configuredCodePage=codePage;InterlockedExchange(&configured,exactR29&&remakeInterface?1:0);
    if(!exactR29||!remakeInterface){InterlockedExchange(&presentation,0);InterlockedExchange(&closingFence,0);}
}
static bool Install(){
    if(!InterlockedCompareExchange(&configured,0,0)||!InterlockedCompareExchange(&initialized,0,0))return false;
    if(InterlockedCompareExchange(&installed,0,0))return true;
    BYTE* entry=reinterpret_cast<BYTE*>(0x54af80);
    const BYTE expected[]={0x53,0x55,0x8b,0x6c,0x24,0x0c};
    if(std::memcmp(entry,expected,sizeof(expected)))return false;
    auto* clone=static_cast<BYTE*>(VirtualAlloc(nullptr,11,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if(!clone)return false;
    std::memcpy(clone,expected,6);clone[6]=0xe9;
    DWORD delta=static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(entry+6)-reinterpret_cast<ULONG_PTR>(clone+11));
    std::memcpy(clone+7,&delta,4);DWORD oldClone{};
    if(!VirtualProtect(clone,11,PAGE_EXECUTE_READ,&oldClone)){VirtualFree(clone,0,MEM_RELEASE);return false;}
    FlushInstructionCache(GetCurrentProcess(),clone,11);
    DWORD oldEntry{};
    if(!VirtualProtect(entry,6,PAGE_EXECUTE_READWRITE,&oldEntry)){VirtualFree(clone,0,MEM_RELEASE);return false;}
    nativeOpen=clone;BYTE patch[6]{0xe9,0,0,0,0,0x90};
    delta=static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(&OpenAdapter)-reinterpret_cast<ULONG_PTR>(entry+5));
    std::memcpy(patch+1,&delta,4);std::memcpy(entry,patch,6);
    FlushInstructionCache(GetCurrentProcess(),entry,6);DWORD ignored{};VirtualProtect(entry,6,oldEntry,&ignored);
    InterlockedExchange(&installed,1);return true;
}
static bool Fresh(DialogStateV1& s){return dialog_context::GetState(&s,sizeof(s))&&(s.flags&DialogReady);}
static BOOL SetPresentationReady(BOOL ready){
    DialogStateV1 s{};
    if(ready){
        if(!InterlockedCompareExchange(&configured,0,0)||!Fresh(s))return FALSE;
        InterlockedExchange(&closingFence,0);
        InterlockedExchange(&presentationGeneration,static_cast<LONG>(s.generation));
        InterlockedExchange(&presentationTick,static_cast<LONG>(GetTickCount()));InterlockedExchange(&presentation,1);return TRUE;
    }
    if(InterlockedExchange(&presentation,0)){
        InterlockedExchange(&closingTick,static_cast<LONG>(GetTickCount()));InterlockedExchange(&closingFence,1);
    }return TRUE;
}
static bool ConsumesInput(){
    if(!InterlockedCompareExchange(&configured,0,0))return false;
    DWORD now=GetTickCount();DialogStateV1 s{};
    if(InterlockedCompareExchange(&presentation,0,0)&&Fresh(s)&&
       s.generation==static_cast<DWORD>(InterlockedCompareExchange(&presentationGeneration,0,0))&&
       now-static_cast<DWORD>(InterlockedCompareExchange(&presentationTick,0,0))<=250)return true;
    if(InterlockedCompareExchange(&closingFence,0,0)){
        if(now-static_cast<DWORD>(InterlockedCompareExchange(&closingTick,0,0))<80 ||
           (GetAsyncKeyState(VK_RETURN)&0x8000)||(GetAsyncKeyState(VK_ESCAPE)&0x8000)||(GetAsyncKeyState(VK_LBUTTON)&0x8000))return true;
        InterlockedExchange(&closingFence,0);
    }return false;
}
[[maybe_unused]] static bool OwnsPaint(std::uintptr_t root) noexcept {
    if(!InterlockedCompareExchange(&configured,0,0)||!InterlockedCompareExchange(&presentation,0,0)||
       GetTickCount()-static_cast<DWORD>(InterlockedCompareExchange(&presentationTick,0,0))>250)return false;
    return dialog_context::OwnsNativeRoot(root,static_cast<DWORD>(InterlockedCompareExchange(&presentationGeneration,0,0)));
}
static void InitializeOnUiThreadTick(){
    if(!InterlockedCompareExchange(&configured,0,0))return;
    if(!InterlockedCompareExchange(&initialized,0,0)){
        dialog_context::Initialize(true,configuredCodePage);InterlockedExchange(&initialized,1);
    }
    if(!Install())return;
    dialog_context::Tick();DialogStateV1 s{};
    if(!Fresh(s)||GetTickCount()-static_cast<DWORD>(InterlockedCompareExchange(&presentationTick,0,0))>250)SetPresentationReady(FALSE);
}
#undef AITD_DIALOG_HOOK_VISIBLE
}
// Add undecorated aliases in the input DLL .def, as with existing menu exports:
// AITDGetDialogState=_AITDGetDialogState@8
// AITDQueueDialogAction=_AITDQueueDialogAction@8
// AITDSetDialogPresentationReady=_AITDSetDialogPresentationReady@4
extern "C" BOOL WINAPI AITDGetDialogState(void* out,DWORD bytes){
    if(!InterlockedCompareExchange(&dialog_native_hook::configured,0,0))return FALSE;
    return dialog_context::GetState(out,bytes);
}
extern "C" BOOL WINAPI AITDQueueDialogAction(DWORD slot,DWORD generation){
    if(!InterlockedCompareExchange(&dialog_native_hook::presentation,0,0)||!dialog_native_hook::ConsumesInput())return FALSE;
    return dialog_context::QueueAction(slot,generation);
}
extern "C" BOOL WINAPI AITDSetDialogPresentationReady(BOOL ready){return dialog_native_hook::SetPresentationReady(ready);}
