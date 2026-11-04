// Exact r29 native MainMenu bridge. Include only in the input component.
// Tick must run on the game's UI thread; GetState/Queue are cross-thread safe.
#include "menu_state.h"
#include "options_native_values.h"
#include "ui_native_status.h"
namespace menu_context {
static bool enabled=false,credits=false,enteredMenu=false,startupTitleSeen=false;
static volatile LONG queued=0;
static volatile LONG presentationReady=0,presentationTick=0,closing=0,closingTick=0;
static volatile LONG continueKnown=0,continueAvailable=0,installation=0;
static void* nativeVariable=reinterpret_cast<void*>(0x91f4b0);
static SRWLOCK stateLock=SRWLOCK_INIT;
static MenuStateV1 state{sizeof(MenuStateV1),1,0,0,0,0,0,0};
static DWORD ReadWord(const void* p,unsigned offset){return *reinterpret_cast<const DWORD*>(static_cast<const unsigned char*>(p)+offset);}
static void __cdecl CaptureContinue(unsigned char* handle,DWORD available){
    __try {
        if(!enabled||available>1||strcmp(reinterpret_cast<const char*>(0xfae538),"MainMenu"))return;
        auto* movie=*reinterpret_cast<unsigned char**>(0xfe1518);
        if(!movie||ReadWord(movie,0xc)!=0xfe1520||*reinterpret_cast<unsigned char**>(movie+0x24)!=handle)return;
        InterlockedExchange(&continueAvailable,static_cast<LONG>(available));InterlockedExchange(&continueKnown,1);
    }__except(EXCEPTION_EXECUTE_HANDLER){}
}
static __declspec(naked) void ContinueAdapter(){
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        push [ebp+12]
        push eax
        call CaptureContinue
        add esp,8
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        jmp dword ptr [nativeVariable]
    }
}
static bool Resolve(unsigned char* handle,const char* path,DWORD (&binding)[2]){
#ifdef AITD_MENU_CONTEXT_TEST
    return TestResolve(handle,path,binding);
#else
    DWORD result=0;DWORD* bindingOutput=binding;
    __asm {
        mov ecx,handle
        mov edx,path
        push bindingOutput
        mov eax,91f860h
        call eax
        mov result,eax
    }
    return (result&255)!=0;
#endif
}
static bool Visible(unsigned char* handle,const char* path){
    DWORD binding[2]{};
    if(!Resolve(handle,path,binding)||!binding[0]||binding[1]!=ReadWord(handle,0)||binding[0]==binding[1]+200)return false;
    return (ReadWord(reinterpret_cast<void*>(binding[0]),0x6c)&1)!=0;
}
static bool LiveMode(unsigned char* handle,DWORD& ingame,DWORD& fake){
#ifdef AITD_MENU_CONTEXT_TEST
 return TestMode(handle,ingame,fake);
#else
 const auto read=[](void*,std::uint32_t at,void* out,std::size_t bytes){__try{memcpy(out,reinterpret_cast<void*>(at),bytes);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}};
 const DWORD root=ReadWord(handle,0);dialog_text::Value v{};std::uint32_t value{};
 if(!options_native_values::Scoped({read,nullptr},root,root+200,"IsMainInGame",v)||!dialog_text::Unsigned(v,1,value))return false;ingame=value;
 if(!options_native_values::Scoped({read,nullptr},root,root+200,"fake",v)||!dialog_text::Unsigned(v,1,value))return false;fake=value;return true;
#endif
}
static bool Capture(MenuStateV1& s,unsigned char*& movie){
    movie=nullptr;s.flags=0;s.nativeFlags=0;s.nativeState=0;
    __try{
        if(!enabled)return false;
        // FAE538 is the startup scenario string and can remain MainMenu after
        // a different episode loads. Authenticate the live film mode instead.
        if(ReadWord(reinterpret_cast<void*>(0xfe1520),0)!=0xc77b68)return false;
        movie=*reinterpret_cast<unsigned char**>(0xfe1518);
        if(!movie||ReadWord(movie,0xc)!=0xfe1520||!movie[0x65])return false;
        auto* handle=*reinterpret_cast<unsigned char**>(movie+0x24);
        if(!handle||!ReadWord(handle,0))return false;
        DWORD ingame{},fake{};const bool known=LiveMode(handle,ingame,fake),main=known&&!ingame&&!fake;
        if((known&&ingame&&!fake)||(enteredMenu&&!main))return false;
        s.nativeFlags=*reinterpret_cast<DWORD*>(0xff4290);
        s.nativeState=*reinterpret_cast<DWORD*>(0xff5508)&0x7fffffff;
        s.flags=MenuAuthenticated;
        // A movie pointer alone includes title, chapters, settings and dialogs.
        // This presenter owns navigation only; the shared guard separately covers
        // transitions and the other replacement presenters own their routes.
        // UserSysInterface neutral state is 0x23 (5548D0 initializer and native
        // return paths). Main-menu movie/bit2 stay visible during native dialogs:
        // popnew sets state2; popexit sets0x27. Replacing that background would
        // cover the separate native confirmation and consume its Escape input.
        // Generic warnings (54AF80) independently set FF437C; loading/message
        // movies (54B200/54B300) use FF4390, even with UserSysInterface neutral.
        if(main&&s.nativeState==0x23&&s.nativeFlags==2&&!*reinterpret_cast<BYTE*>(0xff437c)&&
           !ui_native_status::BlockingLoad(*reinterpret_cast<DWORD*>(0xff4390))&&Visible(handle,"/mainmenu")&&!Visible(handle,"/press_start")&&
           !Visible(handle,"/pop_up")&&!Visible(handle,"/options"))s.flags|=MenuRootVisible;
        // Title is presentation-only: original PressAny input stays live.
        // Actual first entry (observed V3/V8/V9) is flags18/state0, before
        // neutral navigation35. Replace its old title/video once per session.
        if(s.nativeState==0x23&&s.nativeFlags==2&&!*reinterpret_cast<BYTE*>(0xff437c)&&
           !*reinterpret_cast<DWORD*>(0xff4390)&&Visible(handle,"/press_start")&&!Visible(handle,"/mainmenu")&&
           !Visible(handle,"/pop_up")&&!Visible(handle,"/options"))s.flags|=MenuTitleVisible;
        if(!enteredMenu&&s.nativeFlags==18&&s.nativeState==0&&!*reinterpret_cast<BYTE*>(0xff437c)&&
           !*reinterpret_cast<DWORD*>(0xff4390))s.flags|=MenuTitleVisible;
        // User-session traces: an initial flags2/state35 gap precedes the
        // title clip, and accepting PressAny visits state10 or27 for ~130ms.
        // These are startup-only presentation states, never input ownership.
        if(!enteredMenu&&!(s.flags&MenuRootVisible)&&(s.nativeFlags==2||s.nativeFlags==18)&&
           ((s.nativeFlags==2&&s.nativeState==35)||
            (startupTitleSeen&&(s.nativeState==0||s.nativeState==1||s.nativeState==10||s.nativeState==27||s.nativeState==35)))&&
           !*reinterpret_cast<BYTE*>(0xff437c)&&(*reinterpret_cast<DWORD*>(0xff4390)<=3||*reinterpret_cast<DWORD*>(0xff4390)==5)&&
           !Visible(handle,"/pop_up")&&!Visible(handle,"/options"))s.flags|=MenuTitleVisible;
        // 54B200 uses FF4390=1..3 exclusively for loading progress. Keep the
        // startup logo over that transient film; FF4390=4 is a message (54B300)
        // and remains visible, as do all real warnings through FF437C. Value5
        // is only the closing animation scheduled by54B3D0 after progress/message
        // completion; it also must not reveal the original startup artwork.
        // Known implemented submenu transitions may have no coherent copied
        // child text for a frame. Draw our video underneath the new presenter
        // rather than exposing native chrome. Configuration/chapters/modals
        // remain excluded until their corresponding frontend owns them.
        if(!(s.flags&(MenuRootVisible|MenuTitleVisible))&&(s.nativeState==35||
           (s.nativeState==34&&enteredMenu&&s.nativeFlags==6&&ReadWord(reinterpret_cast<void*>(0xfe1790),0)==0))&&
           !*reinterpret_cast<BYTE*>(0xff437c)&&!ui_native_status::BlockingLoad(*reinterpret_cast<DWORD*>(0xff4390))&&
           ((s.nativeFlags==6&&ReadWord(reinterpret_cast<void*>(0xfe1790),0)<=3)||
            (s.nativeFlags==2&&enteredMenu)))s.flags|=MenuTransitionBackdrop;
        if(credits&&(s.flags&MenuRootVisible))s.flags|=MenuCredits;
        if(InterlockedCompareExchange(&continueKnown,0,0)){
            s.flags|=MenuContinueKnown;
            if(InterlockedCompareExchange(&continueAvailable,0,0))s.flags|=MenuContinueAvailable;
        }
        return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){movie=nullptr;s.flags=0;return false;}
}
// Custom VMF ABI: EAX=handle, EDX=target, EDI=label, stack BOOL play;
// 91F7A0 returns with RET 4. Unlike an FSCommand, this advances display state.
[[maybe_unused]] static __declspec(noinline) void NativeTimeline(unsigned char* handle,const char* target,const char* label){
    __asm {
        mov eax,handle
        mov edx,target
        mov edi,label
        push 1
        mov ecx,91f7a0h
        call ecx
    }
}
[[maybe_unused]] static __declspec(noinline) void NativeSelection(unsigned char* handle,DWORD selection){
    const char* path="/mainmenu/:num_ssm";
    __asm {
        mov eax,handle
        push selection
        push path
        mov ecx,91f4b0h
        call ecx
    }
}
static bool Timeline(unsigned char* movie,const char* target,const char* label){
    auto* handle=*reinterpret_cast<unsigned char**>(movie+0x24);
    DWORD binding[2]{};
    if(!handle||!Resolve(handle,target,binding)||!binding[0]||binding[1]!=ReadWord(handle,0))return false;
#ifdef AITD_MENU_CONTEXT_TEST
    return TestTimeline(handle,target,label);
#else
    NativeTimeline(handle,target,label);return true;
#endif
}
static bool Confirmation(unsigned char* movie,DWORD selection){
    auto* handle=*reinterpret_cast<unsigned char**>(movie+0x24);
    // Run the original button's end action, including AfterValid/num_valided,
    // platform checks, confirmation command and warning animation. Main-menu
    // slot 6 is exit/popexit; gameplay-pause quit/popquit is a different flow.
#ifdef AITD_MENU_CONTEXT_TEST
    if(!TestSelection(handle,selection))return false;
#else
    NativeSelection(handle,selection);
#endif
    return Timeline(movie,"/mainmenu","end");
}
static bool Dispatch(unsigned char* movie,DWORD action){
    // Native confirmation/profile flows remain native: do not jump directly to
    // newgame or terminate the process. These are existing FSCommand identities.
    __try{
    DWORD offset=0;
    switch(action){
    case MenuContinue:offset=0x18;break; // cont_in
    case MenuNewGame:return Confirmation(movie,2);
    case MenuChapters:offset=0x20;break; // chap_in
    case MenuOptions:offset=0x90;break; // opt_in
    case MenuQuit:return Confirmation(movie,6);
    default:return false;
    }
        const DWORD low=*reinterpret_cast<DWORD*>(0xfe1520+offset),high=*reinterpret_cast<DWORD*>(0xfe1524+offset);
        if(!(low|high))return false;
        // Original stop_close Options actions: target root, goto options,
        // play, restore target, then FSCommand opt_in. Native opt_in alone
        // enables settings logic but leaves the old navigation display visible.
        if(action==MenuOptions&&!Timeline(movie,"/","options"))return false;
#ifdef AITD_MENU_CONTEXT_TEST
        return TestDispatch(movie,low,high);
#else
        using Native=unsigned char(__thiscall*)(void*,void*,DWORD,DWORD,DWORD);
        return reinterpret_cast<Native>(0x53fa40)(reinterpret_cast<void*>(0xfe1520),movie,low,high,0)!=0;
#endif
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static void SetPresentationReady(BOOL value){
    if(value){InterlockedExchange(&presentationTick,static_cast<LONG>(GetTickCount()));InterlockedExchange(&presentationReady,1);}
    else if(InterlockedExchange(&presentationReady,0)){
        InterlockedExchange(&closingTick,static_cast<LONG>(GetTickCount()));InterlockedExchange(&closing,1);
    }
}
static void KeepMenuActive(const MenuStateV1& current){
    // Native 53E780 detects only ActionManager activity. Our complete renderer
    // deliberately consumes those inputs, otherwise its invisible buttons also
    // activate. Keep the native 120s attract timer at zero while our actual fresh
    // root/Credits presentation owns the screen. No title/gameplay/global bypass.
    if(!(current.flags&MenuRootVisible)||!InterlockedCompareExchange(&presentationReady,0,0)||
       GetTickCount()-static_cast<DWORD>(InterlockedCompareExchange(&presentationTick,0,0))>250)return;
    __try{*reinterpret_cast<float*>(0xff429c)=0.0f;}__except(EXCEPTION_EXECUTE_HANDLER){}
}
static void Tick(){
    MenuStateV1 next{};unsigned char* movie=nullptr;Capture(next,movie);
    if(next.flags&MenuTitleVisible)startupTitleSeen=true;
    if(next.flags&MenuRootVisible)enteredMenu=true;
    KeepMenuActive(next);
    const DWORD action=static_cast<DWORD>(InterlockedExchange(&queued,0));
    if(!(next.flags&MenuRootVisible)){credits=false;SetPresentationReady(FALSE);}
    bool dispatched=false,rejected=false;
    if(action){
        if(!(next.flags&MenuRootVisible))rejected=true;
        else if(action==MenuShowCredits){credits=true;dispatched=true;}
        else if(action==MenuCloseCredits){credits=false;dispatched=true;}
        else if(credits)rejected=true;
        else if(action==MenuContinue&&!(next.flags&MenuContinueAvailable))rejected=true;
        else{dispatched=Dispatch(movie,action);rejected=!dispatched;SetPresentationReady(FALSE);}
        Capture(next,movie);
    }
    next.size=sizeof(next);next.version=1;next.tick=GetTickCount();
    AcquireSRWLockExclusive(&stateLock);
    next.dispatches=state.dispatches+(dispatched?1:0);next.rejected=state.rejected+(rejected?1:0);state=next;
    ReleaseSRWLockExclusive(&stateLock);
}
static BOOL GetState(void* output,DWORD bytes){
    if(!output||bytes!=sizeof(MenuStateV1))return FALSE;
    BOOL okay=TRUE;AcquireSRWLockShared(&stateLock);
    __try{memcpy(output,&state,sizeof(state));}__except(EXCEPTION_EXECUTE_HANDLER){okay=FALSE;}
    ReleaseSRWLockShared(&stateLock);return okay;
}
static bool ConsumesInput(){
    if(!enabled)return false;const DWORD now=GetTickCount();MenuStateV1 current{};GetState(&current,sizeof(current));
    if(InterlockedCompareExchange(&presentationReady,0,0)&&(current.flags&MenuRootVisible)&&now-current.tick<=250&&
       now-static_cast<DWORD>(InterlockedCompareExchange(&presentationTick,0,0))<=250)return true;
    if(InterlockedCompareExchange(&closing,0,0)){
        if(now-static_cast<DWORD>(InterlockedCompareExchange(&closingTick,0,0))<80||
           (GetAsyncKeyState(VK_RETURN)&0x8000)||(GetAsyncKeyState(VK_ESCAPE)&0x8000)||(GetAsyncKeyState(VK_LBUTTON)&0x8000))return true;
        InterlockedExchange(&closing,0);
    }
    return false;
}
static BOOL Queue(DWORD action){
    if(!enabled||action<MenuContinue||action>MenuCloseCredits)return FALSE;
    MenuStateV1 current{};GetState(&current,sizeof(current));
    if(!(current.flags&MenuRootVisible)||GetTickCount()-current.tick>250)return FALSE;
    if(action==MenuContinue&&!(current.flags&MenuContinueAvailable))return FALSE;
    return InterlockedCompareExchange(&queued,static_cast<LONG>(action),0)==0;
}
static void Configure(bool value){enabled=value;if(!value){credits=false;enteredMenu=false;startupTitleSeen=false;InterlockedExchange(&queued,0);SetPresentationReady(FALSE);InterlockedExchange(&closing,0);}}
[[maybe_unused]] static void Install(){
    if(!enabled||InterlockedCompareExchange(&installation,1,0))return;
    auto* site=reinterpret_cast<BYTE*>(0x53ebf6);
    const BYTE signature[]={0xe8,0xb5,0x08,0x3e,0x00};
    if(memcmp(site,signature,5)){enabled=false;return;}
    DWORD old=0;if(!VirtualProtect(site,5,PAGE_EXECUTE_READWRITE,&old)){enabled=false;return;}
    const DWORD offset=static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(&ContinueAdapter)-reinterpret_cast<ULONG_PTR>(site+5));
    memcpy(site+1,&offset,4);site[0]=0xe8;FlushInstructionCache(GetCurrentProcess(),site,5);
    DWORD ignored=0;VirtualProtect(site,5,old,&ignored);
}
}
