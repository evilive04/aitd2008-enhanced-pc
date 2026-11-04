// Scoped native rich-text bitmaps. Requires paired atlas + VMF pack.
#include "pc_keycap_layout.h"
namespace pc_keycaps {
static bool enabled=false;
static volatile LONG installation=0;
static volatile LONG encodes=0,resolves=0,atlasMisses=0;
static void* nativeResolver=nullptr;
static DWORD resolverSite=0x54bcc0;
static SRWLOCK ownershipLock=SRWLOCK_INIT;
static DWORD retainedMaterial=0,retainedTexture=0;
static DWORD releaseResourceSite=0x4990b0;
static bool RetainPositive(LONG* references) {
    LONG observed=InterlockedCompareExchange(references,0,0);
    for(unsigned n=0;n<32&&observed>0&&observed<0x7ffffffe;++n) {
        const LONG previous=InterlockedCompareExchange(references,observed+1,observed);
        if(previous==observed)return true;
        observed=previous;
    }
    return false;
}
static __declspec(noinline) void ReleaseResource(DWORD resource) {
    __asm {
        push edi
        mov edi,resource
        call dword ptr [releaseResourceSite]
        pop edi
    }
}
static DWORD CandidateTexture(DWORD material) {
    // Native graphics resource identity is +10/+14. +60/+64 is the material's
    // short alias (e.g. MAT_0033), used only by the separate inline icon BANK.
    if(!material||*reinterpret_cast<DWORD*>(material+4)!=0x2054414d||
       *reinterpret_cast<DWORD*>(material+0x10)!=0x67eb4a82||
       *reinterpret_cast<DWORD*>(material+0x14)!=0xc6a94a0c)return 0;
    DWORD first=*reinterpret_cast<DWORD*>(material+0x78);
    for(unsigned k=0;k<64;++k) {
        const DWORD next=*reinterpret_cast<DWORD*>(first+0x14);
        if(next==0x121bff8)break;
        first=next;if(k==63)return 0;
    }
    if(first==0x121bff8)return 0;
    const DWORD texture=*reinterpret_cast<DWORD*>(first+0xc);
    const bool verified=texture&&*reinterpret_cast<DWORD*>(texture+4)==0x54584554&&
        *reinterpret_cast<DWORD*>(texture+0x10)==textureNameLow&&
        *reinterpret_cast<DWORD*>(texture+0x14)==textureNameHigh&&
        *reinterpret_cast<DWORD*>(texture+0x20)==atlasSize&&
        *reinterpret_cast<DWORD*>(texture+0x24)==atlasSize&&
        *reinterpret_cast<DWORD*>(texture+0x30)==15;
    return verified?texture:0;
}
static DWORD RegistryMaterial(DWORD manager) {
    // Native87CAB0 orders the resource map by Name, then namespace; children
    // are +18/+1C and the resident resource is +10. Search equal Names in all
    // namespaces, selecting only the exact versioned atlas-backed material.
    DWORD pending[64]{};unsigned used=1,visited=0;
    pending[0]=*reinterpret_cast<DWORD*>(manager+0x30);
    while(used&&visited++<256) {
        const DWORD node=pending[--used];if(!node||node==0x121f9cc)continue;
        const DWORD low=*reinterpret_cast<DWORD*>(node),high=*reinterpret_cast<DWORD*>(node+4);
        if(high<0xc6a94a0c||(high==0xc6a94a0c&&low<0x67eb4a82)) {
            if(used==64)return 0;pending[used++]=*reinterpret_cast<DWORD*>(node+0x18);
        } else if(high>0xc6a94a0c||(high==0xc6a94a0c&&low>0x67eb4a82)) {
            if(used==64)return 0;pending[used++]=*reinterpret_cast<DWORD*>(node+0x1c);
        } else {
            const DWORD resource=*reinterpret_cast<DWORD*>(node+0x10);
            if(CandidateTexture(resource))return resource;
            if(used>62)return 0;
            pending[used++]=*reinterpret_cast<DWORD*>(node+0x18);
            pending[used++]=*reinterpret_cast<DWORD*>(node+0x1c);
        }
    }
    return 0;
}
static DWORD Material() {
    DWORD result=0;
    AcquireSRWLockExclusive(&ownershipLock);
    __try {
        __try {
            if(retainedMaterial) {
                // These two resources have our native strong references; BANK
                // unload cannot destroy them. Never acquire more per prompt.
                if(CandidateTexture(retainedMaterial)==retainedTexture)result=retainedMaterial;
            } else {
                const DWORD manager=*reinterpret_cast<DWORD*>(0xfd6324);
                if(manager) {
                    const DWORD gate=*reinterpret_cast<DWORD*>(manager+0xe0);
                    if(!gate)__leave; // Manager initialization/shutdown: retain readable text.
                    auto* lock=reinterpret_cast<CRITICAL_SECTION*>(gate+0x28);
                    EnterCriticalSection(lock);
                    __try {
                        const DWORD material=RegistryMaterial(manager);
                        const DWORD texture=material?CandidateTexture(material):0;
                        if(texture) {
                            auto* materialRefs=reinterpret_cast<LONG*>(material+0xc);
                            auto* textureRefs=reinterpret_cast<LONG*>(texture+0xc);
                            // Native+0C protocol, with positive-only CAS so an
                            // unload racing before manager lock cannot revive0.
                            if(RetainPositive(materialRefs)) {
                                if(RetainPositive(textureRefs)) {
                                    retainedMaterial=material;retainedTexture=texture;result=material;
                                    Log("PC keycap resources retained for process lifetime material=%08lx texture=%08lx",material,texture);
                                } else ReleaseResource(material);
                            }
                        }
                    } __finally {LeaveCriticalSection(lock);}
                }
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {}
    } __finally {ReleaseSRWLockExclusive(&ownershipLock);}
    return result;
}
static char* __cdecl Encode(char* output,std::size_t capacity,char* end) {
    const auto bytes=static_cast<std::size_t>(end-output);
    static constexpr char prefix[]="#color:F2E6C9FF#[",suffix[]="]#nocolor#";
    const unsigned head=sizeof(prefix)-1,tail=sizeof(suffix)-1;
    if(bytes<head+tail+1||bytes>capacity||memcmp(output,prefix,head)||
       memcmp(end-tail,suffix,tail)||bytes-head-tail>=40)return end;
    char label[40]{};memcpy(label,output+head,bytes-head-tail);
    const int key=Find(label);if(key<0||!enabled)return end;
    if(!Material()) {
        if(InterlockedIncrement(&atlasMisses)<=4)Log("PC keycap atlas unavailable or identity mismatch; native label retained");
        return end;
    }
    char tag[40]{};const int length=sprintf_s(tag,"#bitmap:epc_key_%02u#",static_cast<unsigned>(key));
    if(length<=0||static_cast<std::size_t>(length)>capacity)return end;
    memcpy(output,tag,static_cast<std::size_t>(length));
    if(static_cast<std::size_t>(length)<capacity)output[length]=0;
    if(InterlockedIncrement(&encodes)<=8)Log("PC keycap encoded label=%s key=%d",label,key);
    return output+length;
}
struct Bitmap {DWORD material;float aspect,advance,baseline,u0,v0,u1,v1;};
static_assert(sizeof(Bitmap)==32,"native inline bitmap entry");
static int Index(const char* name) {
    if(!name||strncmp(name,"epc_key_",8))return -1;
    const char a=name[8];if(a<'0'||a>'9')return -1;
    const char b=name[9];if(b<'0'||b>'9'||name[10])return -1;
    const int index=(a-'0')*10+b-'0';return index<static_cast<int>(keyCount)?index:-1;
}
static bool __cdecl Resolve(const char* name,Bitmap* output) {
    const int index=Index(name);if(index<0||!enabled||!output)return false;
    const DWORD material=Material();if(!material)return false;
    const auto& key=keys[index];const float ratio=static_cast<float>(key.width)/64.f;
    Bitmap value{};value.material=material;value.aspect=ratio;
    value.advance=*reinterpret_cast<float*>(0xfaec38)*ratio;
    value.baseline=*reinterpret_cast<float*>(0xfaec3c);
    value.u0=static_cast<float>(key.x)/atlasSize;value.v0=static_cast<float>(key.y)/atlasSize;
    value.u1=static_cast<float>(key.x+key.width)/atlasSize;value.v1=static_cast<float>(key.y+64)/atlasSize;
    *output=value;
    if(InterlockedIncrement(&resolves)<=8)Log("PC keycap native resolved key=%d material=%08lx aspect=%.2f",index,material,ratio);
    return true;
}
static __declspec(naked) void ResolverAdapter() {
    __asm {
        pushfd
        pushad
        mov ebp,esp
        sub esp,528
        and esp,0xfffffff0
        fxsave [esp]
        push esi
        push dword ptr [ebp+40]
        call Resolve
        add esp,8
        movzx eax,al
        mov [ebp+28],eax
        fxrstor [esp]
        mov esp,ebp
        popad
        popfd
        test eax,eax
        jz native_path
        ret 4
    native_path:
        jmp dword ptr [nativeResolver]
    }
}
}
static void ConfigurePcKeycapIcons(const wchar_t* config,bool supported) {
    pc_keycaps::enabled=supported&&pc_prompts::enabled&&ConfigFlag(L"Interface",L"PcKeycapIcons",false,config);
}
static void InstallPcKeycapIcons() {
    using namespace pc_keycaps;
    if(!pc_keycaps::enabled||InterlockedCompareExchange(&installation,1,0))return;
    auto* entry=reinterpret_cast<BYTE*>(resolverSite);const BYTE signature[]={0x53,0x8b,0x5c,0x24,0x08,0x57};
    if(memcmp(entry,signature,6)){pc_keycaps::enabled=false;return;}
    auto* trampoline=static_cast<BYTE*>(VirtualAlloc(nullptr,11,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!trampoline){pc_keycaps::enabled=false;return;}
    memcpy(trampoline,entry,6);trampoline[6]=0xe9;
    const DWORD back=static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(entry+6)-reinterpret_cast<ULONG_PTR>(trampoline+11));
    memcpy(trampoline+7,&back,4);DWORD protection=0,codeProtection=0;
    if(!VirtualProtect(trampoline,11,PAGE_EXECUTE_READ,&protection)||!VirtualProtect(entry,6,PAGE_EXECUTE_READWRITE,&codeProtection)) {
        VirtualFree(trampoline,0,MEM_RELEASE);pc_keycaps::enabled=false;return;
    }
    nativeResolver=trampoline;const DWORD destination=static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(&ResolverAdapter)-reinterpret_cast<ULONG_PTR>(entry+5));
    memcpy(entry+1,&destination,4);entry[5]=0x90;entry[0]=0xe9;
    FlushInstructionCache(GetCurrentProcess(),trampoline,11);FlushInstructionCache(GetCurrentProcess(),entry,6);
    DWORD ignored;VirtualProtect(entry,6,codeProtection,&ignored);
    pc_prompts::keycapEncoder=&Encode;
    Log("Scoped PC keycaps installed; exact native atlas identity/dimensions required; unknown bindings keep text");
}
