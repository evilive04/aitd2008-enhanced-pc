// Renderer integration for the exact supported game. The opt-in remains off
// until both the local video and the locally generated creator list are present.
#include "menu_presentation.h"
#include <string>
namespace menu_overlay {
static bool enabled=false,attempted=false,drawing=false,backdropDrawn=false,mediaSuspended=false,mediaQuiesced=false;
static bool confirmationPending=false;
static DWORD confirmationGeneration{},confirmationNativeAction{};
static wchar_t videoPath[MAX_PATH]{},creditsPath[MAX_PATH]{};
static menu_presentation::Presentation* presentation=nullptr;
static IDirect3DDevice9* owner=nullptr;
static MenuStateGetter getter=nullptr;
static MenuActionSender sender=nullptr;
using ReadySetter=BOOL(WINAPI*)(BOOL);
static ReadySetter setReady=nullptr;
static DWORD nextLookup=0,outsideSince=0;
static DWORD previousState=~0u;
static bool PrepareForLoad(){
    if(!enabled||!presentation||mediaSuspended||mediaQuiesced)return false;
    // Finish decoder teardown BEFORE publishing the action to the UI thread.
    // Keep its uploaded frame: a native confirmation may still be cancelled.
    // This only touches our own decoder. An expired native snapshot must not
    // allow the subsequent queue operation to race a still-active decoder.
    presentation->SuspendVideoForLoad();mediaQuiesced=true;confirmationPending=false;confirmationGeneration=confirmationNativeAction=0;
    Log("Remake menu decoder released before native load/quit action");
    return true;
}
static void CancelPreparedLoad(){
    confirmationPending=false;confirmationGeneration=confirmationNativeAction=0;
    if(!presentation||mediaSuspended||!mediaQuiesced)return;
    mediaQuiesced=false;
    if(presentation->ResumeVideo(videoPath))Log("Remake menu decoder resumed after cancelled/rejected action");
    else Log("Remake menu decoder restart unavailable after cancelled/rejected action");
}
static bool AwaitingConfirmation(){return confirmationPending&&mediaQuiesced&&!mediaSuspended;}
static void ObserveConfirmation(DWORD generation,DWORD action,DWORD result,bool closing){
    if(!AwaitingConfirmation())return;
    // Bind the fresh, still unanswered dialog before its buttons can be used.
    // A stale closing No or another dialog generation cannot resume this load.
    if(!confirmationGeneration&&!result&&!closing){confirmationGeneration=generation;confirmationNativeAction=action;}
    if(!confirmationGeneration||generation!=confirmationGeneration||action!=confirmationNativeAction)return;
    if(result==2)CancelPreparedLoad();
    else if(result==1){confirmationPending=false;confirmationGeneration=confirmationNativeAction=0;}
}
static BOOL WINAPI SendAction(DWORD action){
    const bool loading=action==MenuContinue||action==MenuNewGame||action==MenuQuit;
    const bool prepared=loading&&PrepareForLoad();
    const BOOL accepted=sender?sender(action):FALSE;
    if(accepted&&mediaQuiesced&&!confirmationPending&&(action==MenuNewGame||action==MenuQuit)){
        confirmationPending=true;confirmationGeneration=confirmationNativeAction=0;
    }
    if(!accepted&&prepared)CancelPreparedLoad();
    return accepted;
}
static bool ReadCredits(std::wstring& output){
    HANDLE file=CreateFileW(creditsPath,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    LARGE_INTEGER length{};bool okay=GetFileSizeEx(file,&length)&&length.QuadPart>0&&length.QuadPart<=1024*1024;
    std::string bytes;
    try{
        if(okay){bytes.resize(static_cast<size_t>(length.QuadPart));DWORD count=0;okay=ReadFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&count,nullptr)&&count==bytes.size();}
    }catch(...){okay=false;}
    CloseHandle(file);if(!okay)return false;
    const char* data=bytes.data();int size=static_cast<int>(bytes.size());
    if(size>=3&&!memcmp(data,"\xef\xbb\xbf",3)){data+=3;size-=3;}
    const int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,data,size,nullptr,0);
    if(count<=0||memchr(data,0,size))return false;
    output.resize(static_cast<size_t>(count));
    return MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,data,size,output.data(),count)==count;
}
static void Configure(const wchar_t* config,bool supported){
    enabled=supported&&ConfigFlag(L"UI",L"RemakeMenu",false,config);
    if(!enabled)return;
    wchar_t root[MAX_PATH]{};if(!GetModuleFileNameW(selfModule,root,MAX_PATH)){enabled=false;return;}
    auto* slash=wcsrchr(root,L'\\');if(!slash){enabled=false;return;}*slash=0;
    if(swprintf_s(videoPath,L"%s\\enhanced-ui\\menu-loop.mp4",root)<0||
       swprintf_s(creditsPath,L"%s\\enhanced-ui\\credits.txt",root)<0)enabled=false;
}
static void Invalidate(IDirect3DDevice9* device){
    if(drawing||!presentation||owner!=device)return;
    drawing=true;presentation->ReleaseDeviceResources();owner=nullptr;drawing=false;
    if(setReady)setReady(FALSE);
}
static void Draw(IDirect3DDevice9* device,const RECT* source,const RECT* destination){
    if(!enabled||drawing)return;
    backdropDrawn=false;
    drawing=true;
    struct DrawingScope{~DrawingScope(){drawing=false;}} scope;
    bool ready=false;
    try{
        const DWORD now=GetTickCount();
        if((!getter||!sender||!setReady)&&(!nextLookup||static_cast<LONG>(now-nextLookup)>=0)){
            nextLookup=now+1000;auto module=GetModuleHandleW(L"dinput8.dll");
            if(module&&FfSameDirectory(module)){
                getter=reinterpret_cast<MenuStateGetter>(GetProcAddress(module,"AITDGetMenuState"));
                sender=reinterpret_cast<MenuActionSender>(GetProcAddress(module,"AITDQueueMenuAction"));
                setReady=reinterpret_cast<ReadySetter>(GetProcAddress(module,"AITDSetMenuPresentationReady"));
            }
        }
        MenuStateV1 state{};
        const bool sampled=!source&&!destination&&getter&&sender&&setReady&&getter(&state,sizeof(state))&&
            state.size==sizeof(state)&&state.version==1&&now-state.tick<=250;
        const bool valid=sampled&&(state.flags&MenuAuthenticated);
        const bool root=valid&&(state.flags&(MenuRootVisible|MenuTitleVisible));
        const bool transition=valid&&(state.flags&MenuTransitionBackdrop);
        if((root||transition)&&!attempted){
            attempted=true;std::wstring credits;
            if(ReadCredits(credits)){
                auto* candidate=new menu_presentation::Presentation;
                if(SUCCEEDED(candidate->SetCredits(credits.c_str()))&&candidate->Start(videoPath)){
                    presentation=candidate;
                    // The presenter lives until termination, but its decoder and
                    // frame buffers are released outside the main-menu session.
                    // Pinning prevents unload races; never join in DllMain.
                    HMODULE pinned=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                        reinterpret_cast<LPCWSTR>(&Draw),&pinned);
                }else delete candidate;
            }else Log("Remake menu: complete UTF8 credits file unavailable; original menu retained");
        }
        if(!sampled||valid)outsideSince=0;
        else if(!outsideSince)outsideSince=now?now:1;
        if(presentation&&sampled&&!valid&&!mediaSuspended&&outsideSince&&now-outsideSince>=100){
            // A paused 4K decoder still retains its reference-picture pool.
            // Give that address space back before the native chapter loader;
            // Keep media alive through authenticated Options/Episodes pages;
            // ignore a brief failed capture rather than restarting the decoder.
            presentation->Shutdown();mediaSuspended=true;mediaQuiesced=false;confirmationPending=false;owner=nullptr;
            Log("Remake menu media released outside authenticated main menu");
        }
        if(presentation&&mediaSuspended&&(root||transition)){
            mediaSuspended=false;
            if(!presentation->Start(videoPath)){
                delete presentation;presentation=nullptr;
                Log("Remake menu media restart unavailable; original menu retained");
            }else Log("Remake menu media resumed for authenticated main menu");
        }
        if(presentation){
            if(root){owner=device;const bool painted=presentation->Draw(device,state,SendAction);ready=painted&&(state.flags&MenuRootVisible);}
            else if(transition){owner=device;backdropDrawn=presentation->Draw(device,state,nullptr,true);}
            else presentation->SetVisible((state.flags&MenuAuthenticated)&&now-state.tick<=250);
        }
        const DWORD diagnostic=(state.flags&255u)|(getter?0x100u:0u)|(presentation?0x200u:0u)|(ready?0x400u:0u);
        if(diagnostic!=previousState){
            previousState=diagnostic;Log("Menu presentation status=%08lx native_flags=%08lx native_state=%lu dispatches=%lu rejected=%lu media_hr=%08lx",diagnostic,state.nativeFlags,state.nativeState,state.dispatches,state.rejected,presentation?presentation->Error():S_FALSE);
            if(presentation&&FAILED(presentation->Error())){
                const auto d=presentation->Diagnostics();
                Log("Menu error presentation stage=%lu kind=%lu code=%08lx detail=%s; media stage=%lu kind=%lu code=%08lx decoded=%llu published=%llu copied=%llu bytes=%llu detail=%s",
                    d.stage,d.exceptionKind,d.exceptionCode,d.detail,d.media.stage,d.media.exceptionKind,d.media.exceptionCode,
                    d.media.decoded,d.media.published,d.media.copied,d.media.requestedBytes,d.media.detail);
            }
        }
    }catch(...){ready=false;Log("Remake menu unavailable; original input retained");}
    if(setReady)setReady(ready?TRUE:FALSE);
}
static bool DrawBackdrop(IDirect3DDevice9* device){
    MenuStateV1 state{};
    // MainMenu authentication excludes gameplay/pause reuse of the same film.
    // During gameplay no blit/clear is issued: genuine scene pixels remain.
    if(!enabled||!getter||!getter(&state,sizeof state)||!(state.flags&MenuAuthenticated))return true;
    if(state.size!=sizeof state||state.version!=1||GetTickCount()-state.tick>250||!presentation)return false;
    if(backdropDrawn&&owner==device)return true;
    owner=device;backdropDrawn=presentation->Draw(device,state,nullptr,true);return backdropDrawn;
}
}
