// Exact-r29 native #command: / #pccommand: formatter integration.
// The original formatter still resolves active keyboard/mouse bindings and
// localized compound aliases. Only its two pad-bitmap choices are redirected
// to the existing PC route. Optional scoped keycaps retain native resolution.
#include "pc_prompt_policy.h"
namespace pc_prompts {
using LabelProvider=const char* (__cdecl*)(const char* command);
static LabelProvider contextLabelProvider=nullptr;
using BufferLabelProvider=const char* (__cdecl*)(const char*,const char*,std::size_t);
static BufferLabelProvider bufferLabelProvider=nullptr;
using KeycapEncoder=char* (__cdecl*)(char*,std::size_t,char*);
static KeycapEncoder keycapEncoder=nullptr;
static bool enabled=false,modernControls=false;
static volatile LONG installation=0,observations=0;
static void* nativeFormatter=nullptr;
static DWORD formatterSite=0x54a650;
static __declspec(noinline) char* CallNative(const char* command,char* output,std::size_t remaining) {
    char* result=nullptr;
    __asm {
        mov eax,command
        push remaining
        push output
        call dword ptr [nativeFormatter]
        add esp,8
        mov result,eax
    }
    return result;
}
// Resolve the same physical binding as54A650. Mouse control offsets are
// DirectInput byte offsets12/13, not button ordinals. Chords stay native.
static __declspec(noinline) int BindingIndex(void* config,const char* command){int result{};__asm{
 mov eax,config
 push command
 mov edx,41da30h
 call edx
 mov result,eax
}return result;}
static __declspec(noinline) BYTE* BindingDevice(void* binding){BYTE* result{};__asm{
 mov ecx,binding
 mov edx,41d130h
 call edx
 mov result,eax
}return result;}
static __declspec(noinline) BYTE* BindingControl(void* binding){BYTE* result{};__asm{
 mov eax,binding
 mov edx,41d170h
 call edx
 mov result,eax
}return result;}
static const char* MouseLabel(const char* command){
 __try{
  const LONG current=*reinterpret_cast<LONG*>(0xfd8cbc);if(current!=0)return nullptr;
  auto* owner=*reinterpret_cast<BYTE**>(0xfd8cb4);auto* config=owner?*reinterpret_cast<BYTE**>(owner+0x14):nullptr;if(!config)return nullptr;
  const auto capacity=*reinterpret_cast<DWORD*>(config),used=*reinterpret_cast<DWORD*>(config+4);if(!capacity||capacity>115||!used||used>capacity)return nullptr;
  const int index=BindingIndex(config,command);if(index<0||static_cast<DWORD>(index)>=used)return nullptr;
  auto* entries=*reinterpret_cast<BYTE**>(config+8);if(!entries)return nullptr;auto* record=entries+index*0x20c;
  const auto bindingCapacity=*reinterpret_cast<DWORD*>(record+8);if(!bindingCapacity||bindingCapacity>6||*reinterpret_cast<DWORD*>(record+12)!=1)return nullptr;
  auto* binding=*reinterpret_cast<BYTE**>(record+16);if(!binding)return nullptr;
  auto* device=BindingDevice(binding);auto* control=BindingControl(binding);
  if(!device||*reinterpret_cast<DWORD*>(device)!=0xcd3d68||!control||*reinterpret_cast<DWORD*>(control)!=0)return nullptr;
  const auto offset=*reinterpret_cast<DWORD*>(control+4);return offset==12?"MOUSE LEFT":offset==13?"MOUSE RIGHT":nullptr;
 }__except(EXCEPTION_EXECUTE_HANDLER){return nullptr;}
}
static char* __cdecl Format(const char* command,char* output,std::size_t remaining) {
    if(!command||!output)return output;
    // A supplied provider owns context eligibility as well as the label.
    // nullptr from it means retain the actual native binding in this context.
    const char* label=bufferLabelProvider?bufferLabelProvider(command,output,remaining):nullptr;
    if(!label)label=contextLabelProvider?contextLabelProvider(command):FixedLabel(command,modernControls);
    if(!label)label=MouseLabel(command);
    char* result=label?AppendLabel(output,remaining,label):CallNative(command,output,remaining);
    const auto begin=reinterpret_cast<ULONG_PTR>(output);
    auto end=reinterpret_cast<ULONG_PTR>(result);
    if(end>=begin&&end-begin<=remaining) {
        RecolorNativeLabel(output,static_cast<std::size_t>(end-begin));
        if(keycapEncoder){result=keycapEncoder(output,remaining,result);end=reinterpret_cast<ULONG_PTR>(result);}
    }
    if(InterlockedIncrement(&observations)<=64)
        Log("PC prompt command=%.80s custom=%d bytes=%lu text=%.*s",command,label!=nullptr,
            end>=begin&&end-begin<=remaining?static_cast<unsigned long>(end-begin):0ul,
            static_cast<int>(remaining<160?remaining:160),output);
    return result;
}
static __declspec(naked) void FormatAdapter() {
    __asm {
        mov ecx,esp
        push dword ptr [ecx+8]
        push dword ptr [ecx+4]
        push eax
        call Format
        add esp,12
        ret
    }
}
}
static void ConfigurePcKeyboardPrompts(const wchar_t* config,bool supported) {
    pc_prompts::enabled=supported&&ConfigFlag(L"Interface",L"KeyboardPrompts",true,config);
    pc_prompts::modernControls=supported&&ConfigFlag(L"Controls",L"ModernCombat",true,config);
}
static void InstallPcKeyboardPrompts() {
    using namespace pc_prompts;
    if(!pc_prompts::enabled||InterlockedCompareExchange(&installation,1,0))return;
    auto* entry=reinterpret_cast<BYTE*>(formatterSite);
    const BYTE prologue[]={0x81,0xec,0x04,0x02,0,0};
    const BYTE firstBranch[]={0x74,0x0c},secondBranch[]={0x74,0x11};
    if(memcmp(entry,prologue,sizeof(prologue))||memcmp(entry+0x23,firstBranch,2)||memcmp(entry+0xd4,secondBranch,2)) {
        pc_prompts::enabled=false;Log("PC prompt signatures mismatch; native formatter retained");return;
    }
    auto* trampoline=static_cast<BYTE*>(VirtualAlloc(nullptr,11,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!trampoline){pc_prompts::enabled=false;return;}
    memcpy(trampoline,entry,6);trampoline[6]=0xe9;
    const DWORD back=static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(entry+6)-reinterpret_cast<ULONG_PTR>(trampoline+11));
    memcpy(trampoline+7,&back,4);
    DWORD oldTrampoline=0,oldCode=0;
    if(!VirtualProtect(trampoline,11,PAGE_EXECUTE_READ,&oldTrampoline)||
       !VirtualProtect(entry,0xd6,PAGE_EXECUTE_READWRITE,&oldCode)) {
        VirtualFree(trampoline,0,MEM_RELEASE);pc_prompts::enabled=false;return;
    }
    nativeFormatter=trampoline;
    entry[0x23]=0xeb;entry[0xd4]=0xeb;
    const DWORD destination=static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(&FormatAdapter)-reinterpret_cast<ULONG_PTR>(entry+5));
    memcpy(entry+1,&destination,4);entry[5]=0x90;entry[0]=0xe9;
    FlushInstructionCache(GetCurrentProcess(),trampoline,11);
    FlushInstructionCache(GetCurrentProcess(),entry,0xd6);
    DWORD ignored=0;VirtualProtect(entry,0xd6,oldCode,&ignored);
    Log("PC prompt formatter installed: active native PC bindings; modern interaction F; cream labels");
}
