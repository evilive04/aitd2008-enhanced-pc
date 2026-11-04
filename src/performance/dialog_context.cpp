#include "dialog_context.h"
#include "dialog_text_capture.h"
#include "phone_ui_origin.h"
#include <cstring>
namespace dialog_context {
#ifdef AITD_DIALOG_CONTEXT_TEST
bool TestRead(std::uint32_t,void*,std::size_t);
bool TestResolve(DWORD);
bool TestDispatch(DWORD,DWORD);
void TestTimeline(DWORD,const char*);
#endif
namespace {
SRWLOCK lock=SRWLOCK_INIT;
DialogStateV1 state{};
DWORD uiThread=0,generation=0,mode=~0u,observedOwner=0,observedHandle=0;
DWORD queuedSlot=0,queuedGeneration=0;
bool enabled=false,observedPhoneOrigin=false;UINT encoding=0;AcceptedChoiceObserver choiceObserver=nullptr;
bool SafeRead(void*,std::uint32_t at,void* out,std::size_t n){
#ifdef AITD_DIALOG_CONTEXT_TEST
    return TestRead(at,out,n);
#else
    __try {std::memcpy(out,reinterpret_cast<const void*>(at),n);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
#endif
}
bool Word(DWORD at,DWORD& out){return SafeRead(nullptr,at,&out,4);}
bool Decode(const char* source,WCHAR* out,int cap){
    std::size_t n=std::strlen(source);bool ascii=true;
    for(std::size_t i=0;i<n;++i)if(static_cast<unsigned char>(source[i])>=128){ascii=false;break;}
    // A strict valid UTF8 string is self-identifying even without a supplied
    // legacy code page. Never guess ACP for malformed/unidentified old bytes.
    UINT cp=ascii||!encoding?CP_UTF8:encoding;
    DWORD flags=cp==CP_UTF8?MB_ERR_INVALID_CHARS:0;
    int needed=MultiByteToWideChar(cp,flags,source,-1,nullptr,0);
    if(needed<=0||needed>cap)return false;
    return MultiByteToWideChar(cp,flags,source,-1,out,cap)==needed;
}
void Publish(const DialogStateV1& next){AcquireSRWLockExclusive(&lock);state=next;ReleaseSRWLockExclusive(&lock);}
bool ResolveAvert(DWORD handle){
#ifdef AITD_DIALOG_CONTEXT_TEST
    return TestResolve(handle);
#else
    DWORD binding[2]{},result=0;DWORD* output=binding;const char* target="/avert";
    __asm{
        mov ecx,handle
        mov edx,target
        push output
        mov eax,91f860h
        call eax
        mov result,eax
    }
    DWORD root{};return (result&255)&&Word(handle,root)&&binding[1]==root&&binding[0]&&binding[0]!=root+200;
#endif
}
bool SelectNative(DWORD handle,DWORD result){
#ifdef AITD_DIALOG_CONTEXT_TEST
    return TestDispatch(handle,result);
#else
    DWORD movie{},low{},high{},currentHandle{};DWORD offset=result==1?0x18:0x20;
    if(!Word(0xff4378,currentHandle)||currentHandle!=handle||!Word(0xfe14e0,movie)||
       !Word(0xfe14e8+offset,low)||!Word(0xfe14ec+offset,high)||!(low|high))return false;
    using Handler=unsigned char(__thiscall*)(void*,void*,DWORD,DWORD,DWORD);
    return reinterpret_cast<Handler>(0x53f780)(reinterpret_cast<void*>(0xfe14e8),reinterpret_cast<void*>(movie),low,high,0)!=0;
#endif
}
void CloseNative(DWORD handle,bool choice){
    const char* label=choice?"close_choice":"close";
#ifdef AITD_DIALOG_CONTEXT_TEST
    TestTimeline(handle,label);
#else
    const char* target="/avert";
    __asm{
        mov eax,handle
        mov edx,target
        mov edi,label
        push 1
        mov ecx,91f7a0h
        call ecx
    }
#endif
}
}
void SetChoiceObserver(AcceptedChoiceObserver observer)noexcept{choiceObserver=observer;}
void Initialize(bool authenticated,UINT codePage){
    enabled=authenticated;encoding=codePage;uiThread=GetCurrentThreadId();mode=~0u;
    observedOwner=observedHandle=0;observedPhoneOrigin=false;generation=0;queuedSlot=queuedGeneration=0;DialogStateV1 empty{};Publish(empty);
}
void ObserveOpen(DWORD newMode){
    if(!enabled||GetCurrentThreadId()!=uiThread)return;
    // Every successful original open starts a new origin scope, including an
    // unsupported/incompletely observed open. Never inherit a previous phone.
    observedPhoneOrigin=false;mode=~0u;observedOwner=observedHandle=0;
    if(newMode>3)return;
    DWORD owner{},handle{};unsigned char active{};
    if(!SafeRead(nullptr,0xff437c,&active,1)||active!=1||!Word(0xff438c,owner)||!owner||
       !Word(0xff4378,handle)||!handle)return;
    mode=newMode;observedOwner=owner;observedHandle=handle;
    observedPhoneOrigin=phone_ui_origin::Capture({SafeRead,nullptr});
    if(++generation==0)++generation;
}
void Tick(){
    if(!enabled||GetCurrentThreadId()!=uiThread)return;
    AcquireSRWLockExclusive(&lock);DWORD slot=queuedSlot,requestGeneration=queuedGeneration;queuedSlot=queuedGeneration=0;ReleaseSRWLockExclusive(&lock);
    DialogStateV1 next{};next.size=sizeof(next);next.version=1;next.tick=GetTickCount();
    dialog_text::NativeSnapshot native{};
    if(mode<=3&&dialog_text::Capture({SafeRead,nullptr},native)&&native.owner==observedOwner&&native.handle==observedHandle){
        next.generation=generation;next.mode=mode;next.action=native.userArgument;
        next.pendingResult=native.pendingResult;next.language=native.language;next.codePage=encoding;
        bool consistent=native.validEnabled.integer==static_cast<DWORD>(mode!=2)&&native.cancelEnabled.integer==static_cast<DWORD>(mode!=1);
        if(slot&&requestGeneration==generation&&native.pendingResult==0&&consistent&&
           ((slot==DialogFirst&&(mode==3||mode!=2))||(slot==DialogSecond&&(mode==3||mode!=1)))&&ResolveAvert(native.handle)){
            const DWORD result=mode==3?(slot==DialogFirst?2u:1u):(slot==DialogFirst?1u:2u);
            if(SelectNative(native.handle,result)){
                if(choiceObserver)choiceObserver(generation,mode,native.userArgument,result,native.owner);
                // The original movie's geou later invokes the original callback.
                // Never clear modal state or shortcut completion here.
                CloseNative(native.handle,mode==3);next.pendingResult=result;
            }
        }
        const bool choice=mode==3;
        const char* a=choice?native.no.text:native.ok.text;
        const char* b=choice?native.yes.text:native.cancel.text;
        next.firstResult=choice?2:1;next.secondResult=choice?1:2;
        if(consistent&&Decode(native.body.text,next.body,4096)&&Decode(a,next.firstLabel,256)&&Decode(b,next.secondLabel,256)){
            next.flags=DialogReady|(choice?DialogChoice:0);
            if(observedPhoneOrigin)next.flags|=DialogPhoneOrigin;
            if(next.pendingResult)next.flags|=DialogClosing;
            if(choice||mode!=2)next.flags|=DialogFirstEnabled;
            if(choice||mode!=1)next.flags|=DialogSecondEnabled;
        }
    }else{
        // Text resources can appear a tick after the open. A missing label is
        // not a close; retain the authenticated generation and retry next tick.
        DWORD owner{},handle{};unsigned char active{};
        if(!SafeRead(nullptr,0xff437c,&active,1)||active!=1||!Word(0xff438c,owner)||owner!=observedOwner||
           !Word(0xff4378,handle)||handle!=observedHandle){mode=~0u;observedOwner=observedHandle=0;observedPhoneOrigin=false;}
    }
    if(!(next.flags&DialogReady)){
        // Do not accidentally expose partially decoded text to an eager caller.
        DWORD tick=next.tick;next={};next.size=sizeof(next);next.version=1;next.tick=tick;
    }
    Publish(next);
}
BOOL GetState(void* out,DWORD bytes){
    if(!out||bytes!=sizeof(DialogStateV1))return FALSE;
    AcquireSRWLockShared(&lock);DialogStateV1 copy=state;ReleaseSRWLockShared(&lock);
    if(copy.size!=sizeof(copy)||copy.version!=1||!(copy.flags&DialogReady)||GetTickCount()-copy.tick>250)return FALSE;
    __try{std::memcpy(out,&copy,sizeof(copy));return TRUE;}__except(EXCEPTION_EXECUTE_HANDLER){return FALSE;}
}
BOOL QueueAction(DWORD slot,DWORD snapshotGeneration){
    if(slot!=DialogFirst&&slot!=DialogSecond)return FALSE;
    AcquireSRWLockExclusive(&lock);
    DWORD permitted=slot==DialogFirst?DialogFirstEnabled:DialogSecondEnabled;
    bool allowed=state.size==sizeof(state)&&state.version==1&&(state.flags&DialogReady)&&
        !(state.flags&DialogClosing)&&(state.flags&permitted)&&state.generation==snapshotGeneration&&
        GetTickCount()-state.tick<=250&&!queuedSlot;
    if(allowed){queuedSlot=slot;queuedGeneration=snapshotGeneration;}
    ReleaseSRWLockExclusive(&lock);return allowed?TRUE:FALSE;
}
bool OwnsNativeRoot(std::uintptr_t root,DWORD expectedGeneration) noexcept {
    if(!enabled||!root||GetCurrentThreadId()!=uiThread||!observedHandle)return false;
    DWORD current{};if(!Word(observedHandle,current)||current!=root)return false;
    AcquireSRWLockShared(&lock);
    const bool owned=(state.flags&DialogReady)&&state.generation==expectedGeneration&&
                     GetTickCount()-state.tick<=250;
    ReleaseSRWLockShared(&lock);return owned;
}
}
