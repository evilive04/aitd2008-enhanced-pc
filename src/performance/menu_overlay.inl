// Renderer integration for the exact supported game. The opt-in remains off
// until both the local video and the complete private creator list are present.
#include "menu_presentation.h"
#include <string>
namespace menu_overlay {
static bool enabled=false,attempted=false,drawing=false,backdropDrawn=false;
static wchar_t videoPath[MAX_PATH]{},creditsPath[MAX_PATH]{};
static menu_presentation::Presentation* presentation=nullptr;
static IDirect3DDevice9* owner=nullptr;
static MenuStateGetter getter=nullptr;
static MenuActionSender sender=nullptr;
using ReadySetter=BOOL(WINAPI*)(BOOL);
static ReadySetter setReady=nullptr;
static DWORD nextLookup=0;
static DWORD previousState=~0u;
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
        const bool valid=!source&&!destination&&getter&&sender&&setReady&&getter(&state,sizeof(state))&&
            state.size==sizeof(state)&&state.version==1&&(state.flags&MenuAuthenticated)&&now-state.tick<=250;
        const bool root=valid&&(state.flags&(MenuRootVisible|MenuTitleVisible));
        const bool transition=valid&&(state.flags&MenuTransitionBackdrop);
        if((root||transition)&&!attempted){
            attempted=true;std::wstring credits;
            if(ReadCredits(credits)){
                auto* candidate=new menu_presentation::Presentation;
                if(SUCCEEDED(candidate->SetCredits(credits.c_str()))&&candidate->Start(videoPath)){
                    presentation=candidate;
                    // The player and its worker live until process termination.
                    // Pinning avoids any FreeLibrary/unload race; never join in DllMain.
                    HMODULE pinned=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                        reinterpret_cast<LPCWSTR>(&Draw),&pinned);
                }else delete candidate;
            }else Log("Remake menu: complete UTF8 credits file unavailable; original menu retained");
        }
        if(presentation){
            if(root){owner=device;const bool painted=presentation->Draw(device,state,sender);ready=painted&&(state.flags&MenuRootVisible);}
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
