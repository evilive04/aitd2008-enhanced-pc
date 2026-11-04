#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <initializer_list>
static unsigned checks=0;
static bool interfaceEnabled=true,combatEnabled=true;
static bool ConfigFlag(const wchar_t*,const wchar_t* key,bool,const wchar_t*) {
    return wcscmp(key,L"KeyboardPrompts")==0?interfaceEnabled:combatEnabled;
}
static void Log(const char*,...) {}
#include "../pc_keyboard_prompts.inl"
static void Check(bool value,const char* text){++checks;if(!value){std::fprintf(stderr,"FAIL %s\n",text);ExitProcess(1);}}
static char* Invoke(const char* command,char* output,std::size_t count) {
    char* result=nullptr;
    const DWORD entry=pc_prompts::formatterSite;
    __asm {
        mov eax,command
        push count
        push output
        call entry
        add esp,8
        mov result,eax
    }
    return result;
}
static const char* __cdecl Provider(const char* command) {
    return pc_prompts::Equal(command,"OBJECT_HANDLE")?"E":nullptr;
}
int main() {
    using namespace pc_prompts;
    Check(FixedLabel("ACTION_ENTER",true)&&!strcmp(FixedLabel("ACTION_ENTER",true),"F"),"interaction F");
    Check(FixedLabel("ACTION_LEAVE",true)&&!strcmp(FixedLabel("ACTION_LEAVE",true),"F"),"leave F");
    Check(!FixedLabel("OBJECT_LAUNCH",true)&&!FixedLabel("ACTION_ENTER",false),"context-dependent actions/native mode retained");
    constexpr char expect[]="#color:F2E6C9FF#[F]#nocolor#";
    for(std::size_t n=0;n<64;++n) {
        char storage[96];memset(storage,'!',sizeof(storage));char* out=storage+8;
        auto* end=AppendLabel(out,n,"F");
        Check(!memcmp(storage,"!!!!!!!!",8),"prefix canary");
        Check(storage[8+(n?n:0)]=='!',"capacity canary");
        if(n>=sizeof(expect))Check(!strcmp(out,expect)&&end==out+sizeof(expect)-1,"complete native label");
        else Check(end==out&&(!n||out[0]==0),"small buffer fail-closed");
    }
    char text[96];strcpy_s(text,"#color:A6440DFF#[R]#nocolor#");RecolorNativeLabel(text,strlen(text));
    Check(!strcmp(text,"#color:F2E6C9FF#[R]#nocolor#"),"native current binding retained");
    for(const char* invalid:{"#bitmap:X#","%s","\n","","abcdefghijklmnopqrstuvwxyz"}) {
        memset(text,'!',sizeof(text));auto* end=AppendLabel(text,sizeof(text),invalid);
        Check(end==text&&text[0]==0,"unsafe label rejected");
    }
    auto* memory=static_cast<BYTE*>(VirtualAlloc(nullptr,0x1000,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
    Check(memory!=nullptr,"isolated native site fixture allocated");
    auto* entry=memory+0x650;formatterSite=reinterpret_cast<DWORD>(entry);
    const BYTE fixture[]={0x81,0xec,0x04,0x02,0,0,0x81,0xc4,0x04,0x02,0,0,0x8b,0x44,0x24,4,
        0xc7,0,0x54,0x45,0x53,0x54,0xc6,0x40,4,0,0x83,0xc0,4,0xc3};
    memcpy(entry,fixture,sizeof(fixture));entry[0x23]=0x74;entry[0x24]=0x0c;entry[0xd4]=0x74;entry[0xd5]=0x11;
    ConfigurePcKeyboardPrompts(L"unused",false);InstallPcKeyboardPrompts();
    Check(entry[0]==0x81,"unsupported image untouched");
    ConfigurePcKeyboardPrompts(L"unused",true);InstallPcKeyboardPrompts();
    Check(pc_prompts::enabled&&nativeFormatter&&entry[0]==0xe9&&entry[0x23]==0xeb&&entry[0xd4]==0xeb,"all exact sites installed");
    Check(entry[0x24]==0x0c&&entry[0xd5]==0x11,"branch destinations unchanged");
    strcpy_s(text,"before");auto* end=Invoke("ACTION_ENTER",text,sizeof(text));
    Check(!strcmp(text,expect)&&end==text+strlen(text),"real x86 adapter modern label and pointer ABI");
    strcpy_s(text,"before");end=Invoke("OBJECT_LAUNCH",text,sizeof(text));
    Check(!strcmp(text,"TEST")&&end==text+4,"real x86 trampoline/native binding fallback");
    contextLabelProvider=Provider;
    end=Invoke("OBJECT_HANDLE",text,sizeof(text));
    Check(strstr(text,"[E]")&&end==text+strlen(text),"context provider label");
    end=Invoke("ACTION_ENTER",text,sizeof(text));
    Check(!strcmp(text,"TEST")&&end==text+4,"provider can decline F outside modern context");
    void* saved=nativeFormatter;InstallPcKeyboardPrompts();Check(saved==nativeFormatter,"installation idempotent");
    VirtualFree(nativeFormatter,0,MEM_RELEASE);VirtualFree(memory,0,MEM_RELEASE);
    std::printf("PASS %u PC label, bounds, installer and compiled x86 ABI checks\n",checks);
    return 0;
}
