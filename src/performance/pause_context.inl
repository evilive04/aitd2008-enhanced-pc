// Include after the native menu bridge. All Resolve/Timeline work stays on
// the original UI thread. The renderer receives copied text and opaque epochs.
#include "pause_state.h"
#include "options_native_values.h"
#include "ui_paint_owner.h"
#include "ui_native_status.h"
#include "native_recap_cancel.h"
namespace pause_context {
struct Key {DWORD movie{},handle{},root{},menu{},mask{},pc{};};
static SRWLOCK lock=SRWLOCK_INIT;
static PauseStateV1 state{};static Key previous{};
static bool enabled=false,ready=false,closing=false;
static volatile DWORD captureStage{}; // Bounded diagnostic for exact native integration; no pointers.
static DWORD generation{},readyGeneration{},readyTick{},closingTick{},queued{},queuedGeneration{};
static DWORD Word(const void* p,unsigned at=0){return *reinterpret_cast<const DWORD*>(static_cast<const BYTE*>(p)+at);}
[[maybe_unused]] static bool Read(void*,std::uint32_t at,void* out,std::size_t bytes){__try{memcpy(out,reinterpret_cast<const void*>(at),bytes);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static bool Value(DWORD root,DWORD node,const char* name,dialog_text::Value& out){
#ifdef AITD_PAUSE_CONTEXT_TEST
    return TestValue(root,node,name,out);
#else
    return options_native_values::Scoped({Read,nullptr},root,node,name,out);
#endif
}
static bool Number(DWORD root,DWORD node,const char* name,DWORD& out){
    dialog_text::Value v{};if(!Value(root,node,name,v))return false;
    std::uint32_t n{};if(!dialog_text::Unsigned(v,16,n))return false;out=n;return true;
}
static bool Text(DWORD root,const char* name,wchar_t* out,int capacity){
    dialog_text::Value v{};if(!Value(root,root+200,name,v)||v.kind!=dialog_text::Kind::String||!v.text[0])return false;
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,v.text,-1,out,capacity);
    if(!n)n=MultiByteToWideChar(CP_ACP,0,v.text,-1,out,capacity);return n!=0;
}
static DWORD Node(unsigned char* handle,const char* path){DWORD b[2]{};if(!menu_context::Resolve(handle,path,b)||!b[0]||b[1]!=Word(handle)||b[0]==b[1]+200||!(Word(reinterpret_cast<void*>(b[0]),0x6c)&1))return 0;return b[0];}
static bool Capture(PauseStateV1& out,Key& key,unsigned char*& movie){
    out={};key={};movie=nullptr;captureStage=1;
    __try{
        // IsMainInGame alone defaults to1 in the authored film. The exact
        // live pause route table and owner below are also mandatory. FAE538
        // is a startup scenario string, not an authoritative current scene.
        if(!enabled||Word(reinterpret_cast<void*>(0xfe1520))!=0xc77b68)return false;
        captureStage=2;movie=*reinterpret_cast<unsigned char**>(0xfe1518);
        if(!movie||Word(movie,12)!=0xfe1520||!movie[0x65])return false;
        captureStage=3;auto* handle=*reinterpret_cast<unsigned char**>(movie+0x24);if(!handle||!Word(handle))return false;
        captureStage=4;const DWORD root=Word(handle),rootNode=root+200;
        const DWORD flags=Word(reinterpret_cast<void*>(0xff4290));
        const bool pc=flags==9; // Actual in-game PC icon/replay surface: bits1+8.
        const DWORD system=Word(reinterpret_cast<void*>(0xff5508))&0x7fffffff;
        if((flags!=2&&!pc)||(system!=35&&!(system==30&&pc&&native_recap_cancel::Matches(reinterpret_cast<DWORD>(movie),reinterpret_cast<DWORD>(handle),root)))||
           *reinterpret_cast<BYTE*>(0xff437c)||ui_native_status::BlockingLoad(Word(reinterpret_cast<void*>(0xff4390))))return false;
        captureStage=5;DWORD ingame{},released{},platform{},count{},fake{};
        if(!Number(root,rootNode,"IsMainInGame",ingame)||ingame!=1||!Number(root,rootNode,"fake",fake)||fake||
           !Number(root,rootNode,"release_button",released)||released>1||(!pc&&released)||
           !Number(root,rootNode,"WhatPlatform",platform)||platform!=1)return false;
        captureStage=6;const DWORD menu=Node(handle,pc?"/menu":"/mainmenu");
        if(!menu||Node(handle,"/press_start")||Node(handle,"/options")||Node(handle,"/pop_up"))return false;
        const DWORD rootFrame=Word(reinterpret_cast<void*>(rootNode),0x60),menuFrame=Word(reinterpret_cast<void*>(menu),0x60);
        const bool busy=released||Word(reinterpret_cast<void*>(menu),0x64)!=2||*reinterpret_cast<BYTE*>(menu+0x68)||
                        (pc&&(rootFrame!=173||menuFrame!=28));
        if(!pc&&busy)return false;
        captureStage=7;if(pc&&(rootFrame<137||rootFrame>173||menuFrame<8||menuFrame>28||
                Word(reinterpret_cast<void*>(Word(reinterpret_cast<void*>(menu),0x18)),8)!=330))return false;
        captureStage=8;if(!Number(root,menu,pc?"nb_ic":"nb_ssms",count)||count!=(pc?4u:6u))return false;
        captureStage=9;const char* full[]={"resume","skip","options","save","load","quit"};
        const char* icon[]={"options","save","load","quit"};
        if(pc){
            // Resume and episode navigation are original root actions, not
            // fictitious icon slots. Keep their localized native captions.
            const char* names[]={"m_Continue","m_Chapters"};
            for(unsigned i=0;i<2;++i){dialog_text::Value label{};
                if(!Value(root,rootNode,names[i],label)||label.kind!=dialog_text::Kind::String||!label.text[0])return false;
                int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,label.text,-1,out.labels[i],128);
                if(!n)n=MultiByteToWideChar(CP_ACP,0,label.text,-1,out.labels[i],128);if(!n)return false;
            }
            out.enabledMask=3;
        }
        captureStage=10;for(unsigned i=0;i<count;++i){
            char name[32]{},path[64]{};sprintf_s(name,"whereto_%u",i+1);dialog_text::Value route{},label{};
            if(!Value(root,menu,name,route)||route.kind!=dialog_text::Kind::String||strcmp(route.text,pc?icon[i]:full[i]))return false;
            sprintf_s(path,pc?"/menu/ic_%u":"/mainmenu/b_%u",i+1);const DWORD button=Node(handle,path);DWORD locked{};
            if(!button||!Number(root,button,"lock",locked)||locked>1||!Value(root,button,"txt_button",label)||label.kind!=dialog_text::Kind::String||!label.text[0])return false;
            const unsigned slot=pc?i+2:i;
            int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,label.text,-1,out.labels[slot],128);
            if(!n)n=MultiByteToWideChar(CP_ACP,0,label.text,-1,out.labels[slot],128);
            if(!n)return false;if(!locked)out.enabledMask|=1u<<slot;
        }
        captureStage=11;if(!(out.enabledMask&1)||*reinterpret_cast<unsigned char**>(0xfe1518)!=movie||Word(movie,0x24)!=reinterpret_cast<DWORD>(handle)||Word(handle)!=root)return false;
        key={reinterpret_cast<DWORD>(movie),reinterpret_cast<DWORD>(handle),root,menu,out.enabledMask,pc?1u:0u};out.flags=PauseVisible|(busy?PauseBusy:0);
        if(!Text(root,"m_game_paused",out.title,128))wcscpy_s(out.title,L"PAUSED");
        if(Text(root,"episode_title",out.episode,128)&&Text(root,"sequence_title",out.sequence,128)&&Text(root,"seq_summary",out.synopsis,768))out.flags|=PauseDetails;
        captureStage=100;return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){out={};key={};return false;}
}
static bool Foreground(){return options_context::Foreground();}
static BOOL GetState(void* out,DWORD bytes){if(!out||bytes!=sizeof state)return FALSE;BOOL okay=TRUE;AcquireSRWLockShared(&lock);__try{memcpy(out,&state,bytes);}__except(EXCEPTION_EXECUTE_HANDLER){okay=FALSE;}ReleaseSRWLockShared(&lock);return okay;}
static void SetPresentationReady(BOOL value,DWORD expected){AcquireSRWLockExclusive(&lock);const DWORD now=GetTickCount();if(value){if(enabled&&(state.flags&PauseVisible)&&state.generation==expected&&now-state.tick<=250){ready=true;readyGeneration=expected;readyTick=now;}}else if(ready&&readyGeneration==expected){ready=false;closing=true;closingTick=now;}ReleaseSRWLockExclusive(&lock);}
static BOOL Queue(DWORD slot,DWORD expected){
    if(slot<1||slot>6||!Foreground())return FALSE;AcquireSRWLockExclusive(&lock);const DWORD now=GetTickCount();
    const bool accepted=enabled&&ready&&(state.flags&PauseVisible)&&!(state.flags&PauseBusy)&&(state.enabledMask&(1u<<(slot-1)))&&state.generation==expected&&readyGeneration==expected&&now-state.tick<=250&&now-readyTick<=250&&!queued;
    if(accepted){queued=slot;queuedGeneration=expected;}ReleaseSRWLockExclusive(&lock);return accepted?TRUE:FALSE;
}
static bool OwnsPaint(std::uintptr_t root)noexcept{AcquireSRWLockShared(&lock);const bool owns=ui_paint_owner::Visual(root,previous.root,enabled,(state.flags&PauseVisible)!=0,ready,state.generation,readyGeneration,state.tick,readyTick,GetTickCount());ReleaseSRWLockShared(&lock);return owns;}
static bool ConsumesInput(){
    if(!Foreground())return false;AcquireSRWLockExclusive(&lock);const DWORD now=GetTickCount();bool owns=enabled&&ready&&(state.flags&PauseVisible)&&state.generation==readyGeneration&&now-state.tick<=250&&now-readyTick<=250;
    if(!owns&&closing){if(now-closingTick<80||(GetAsyncKeyState(VK_RETURN)&0x8000)||(GetAsyncKeyState(VK_ESCAPE)&0x8000)||(GetAsyncKeyState(VK_LBUTTON)&0x8000))owns=true;else closing=false;}
    ReleaseSRWLockExclusive(&lock);return owns;
}
static __declspec(noinline) bool Resume(unsigned char* movie){
    auto* handle=reinterpret_cast<unsigned char*>(Word(movie,0x24));
    // Original PC Escape marks back=1. Without it icon_close only tears down
    // the video bit and leaves the main pause bit active (observed v24).
    options_context::NativeInteger(handle,"/:back",1);
    options_context::NativeInteger(handle,"/:release_button",1);
    return menu_context::Timeline(movie,"/","icon_close");
}
static bool Dispatch(unsigned char* movie,const Key& key,DWORD slot){
    auto* handle=reinterpret_cast<unsigned char*>(key.handle);
    if(key.pc){
        if(slot==1)return Resume(movie);
        if(slot==2)return menu_context::Timeline(movie,"/","to_skip");
#ifdef AITD_PAUSE_CONTEXT_TEST
        TestIcon(handle,slot-2);
#else
        options_context::NativeInteger(handle,"/menu/:num_ic",slot-2);
#endif
        // Original PC Enter button686 invokes the icon's validation scripter.
        return menu_context::Timeline(movie,"/menu/scripter","valid");
    }
#ifdef AITD_PAUSE_CONTEXT_TEST
    TestSelection(handle,slot);
#else
    menu_context::NativeSelection(handle,slot);
#endif
    return menu_context::Timeline(movie,"/mainmenu","end");
}
static void Tick(){
    native_recap_cancel::Reconcile();
    PauseStateV1 next{};Key key{};unsigned char* movie{};Capture(next,key,movie);
    AcquireSRWLockExclusive(&lock);if(memcmp(&key,&previous,sizeof key)){previous=key;++generation;if(!generation)++generation;ready=false;}
    const DWORD slot=queued,expected=queuedGeneration;queued=0;const bool allowed=slot&&(next.flags&PauseVisible)&&!(next.flags&PauseBusy)&&(next.enabledMask&(1u<<(slot-1)))&&expected==generation&&ready&&readyGeneration==generation&&GetTickCount()-readyTick<=250;
    const DWORD sentBefore=state.dispatches,rejectedBefore=state.rejected;if(slot){ready=false;closing=true;closingTick=GetTickCount();}ReleaseSRWLockExclusive(&lock);
    bool sent=false;if(allowed&&Foreground()){
        __try{
            sent=Dispatch(movie,key,slot);
        }__except(EXCEPTION_EXECUTE_HANDLER){sent=false;}
    }
    if(slot){Capture(next,key,movie);AcquireSRWLockExclusive(&lock);if(memcmp(&key,&previous,sizeof key)){previous=key;++generation;if(!generation)++generation;}ReleaseSRWLockExclusive(&lock);}
    next.size=sizeof next;next.version=1;next.tick=GetTickCount();next.generation=generation;next.dispatches=sentBefore+(sent?1:0);next.rejected=rejectedBefore+(slot&&!sent?1:0);
    AcquireSRWLockExclusive(&lock);state=next;if(!(next.flags&PauseVisible))ready=false;ReleaseSRWLockExclusive(&lock);
}
static void Configure(bool value){AcquireSRWLockExclusive(&lock);enabled=value;if(!value){ready=false;closing=false;queued=0;previous={};native_recap_cancel::Clear();++generation;}ReleaseSRWLockExclusive(&lock);}
}
