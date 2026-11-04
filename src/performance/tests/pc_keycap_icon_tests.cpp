#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <initializer_list>
namespace pc_prompts {static bool enabled=true;using Encoder=char* (__cdecl*)(char*,std::size_t,char*);static Encoder keycapEncoder=nullptr;}
static bool option=true;
static bool ConfigFlag(const wchar_t*,const wchar_t*,bool,const wchar_t*){return option;}
static void Log(const char*,...){}
#include "../pc_keycap_icons.inl"
static unsigned checks=0;
static volatile LONG registryFaults=0;
static LONG CALLBACK MissingGateProbe(EXCEPTION_POINTERS* exception) {
    if(exception->ExceptionRecord->ExceptionCode==EXCEPTION_ACCESS_VIOLATION)InterlockedIncrement(&registryFaults);
    return EXCEPTION_CONTINUE_SEARCH;
}
static void Check(bool ok,const char* why){++checks;if(!ok){fprintf(stderr,"FAIL %s\n",why);ExitProcess(1);}}
static BYTE material[256]{},texture[64]{},tree[48]{},manager[256]{},gate[128]{};static DWORD resourceNode[9]{};
static __declspec(naked) void ReleaseStub(){
    __asm {
        lock dec dword ptr [edi+0xc]
        ret
    }
}
static bool Invoke(const char* name,pc_keycaps::Bitmap* output){
    DWORD result=0,stackBefore=0,after=0;void* entry=reinterpret_cast<void*>(pc_keycaps::resolverSite);
    __asm {
        push esi
        mov esi,output
        mov stackBefore,esp
        push name
        call entry
        mov result,eax
        mov after,esp
        pop esi
    }
    Check(stackBefore==after,"compiled resolver adapter stdcall stack ABI");return result!=0;
}
int main(){
    using namespace pc_keycaps;
    for(DWORD page:{0xfa0000ul,0xfd0000ul})Check(VirtualAlloc(reinterpret_cast<void*>(page),65536,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)!=nullptr,"native globals fixture allocation");
    *reinterpret_cast<DWORD*>(0xfd6324)=reinterpret_cast<DWORD>(manager);
    *reinterpret_cast<DWORD*>(manager+0x30)=reinterpret_cast<DWORD>(resourceNode);
    resourceNode[0]=0x67eb4a82;resourceNode[1]=0xc6a94a0c;resourceNode[2]=0x19210541;resourceNode[3]=0xcdd4cf3c;
    resourceNode[4]=reinterpret_cast<DWORD>(material);resourceNode[6]=resourceNode[7]=0x121f9cc;
    auto* nativeLock=reinterpret_cast<CRITICAL_SECTION*>(gate+0x28);InitializeCriticalSection(nativeLock);
    *reinterpret_cast<DWORD*>(manager+0xe0)=reinterpret_cast<DWORD>(gate);
    *reinterpret_cast<DWORD*>(material+4)=0x2054414d;
    *reinterpret_cast<DWORD*>(material+0x10)=0x67eb4a82;*reinterpret_cast<DWORD*>(material+0x14)=0xc6a94a0c;
    memcpy(material+0x60,"MAT_0033",8); // The real runtime alias is not the resource Name.
    *reinterpret_cast<DWORD*>(material+0x78)=reinterpret_cast<DWORD>(tree);*reinterpret_cast<DWORD*>(tree+0x14)=0x121bff8;
    *reinterpret_cast<DWORD*>(tree+0xc)=reinterpret_cast<DWORD>(texture);*reinterpret_cast<DWORD*>(texture+0x20)=1024;*reinterpret_cast<DWORD*>(texture+0x24)=1024;
    *reinterpret_cast<DWORD*>(texture+0x10)=textureNameLow;*reinterpret_cast<DWORD*>(texture+0x14)=textureNameHigh;
    *reinterpret_cast<DWORD*>(texture+4)=0x54584554;
    *reinterpret_cast<LONG*>(material+0xc)=1;*reinterpret_cast<LONG*>(texture+0xc)=2;*reinterpret_cast<DWORD*>(texture+0x30)=15;
    *reinterpret_cast<float*>(0xfaec38)=1.25f;*reinterpret_cast<float*>(0xfaec3c)=.1f;
    releaseResourceSite=reinterpret_cast<DWORD>(&ReleaseStub);
    *reinterpret_cast<DWORD*>(manager+0xe0)=0;*reinterpret_cast<DWORD*>(manager+0x30)=1;
    void* exceptionProbe=AddVectoredExceptionHandler(1,&MissingGateProbe);Check(exceptionProbe!=nullptr,"missing-gate access probe installed");
    Check(!Material()&&!registryFaults&&!retainedMaterial&&!retainedTexture&&
        *reinterpret_cast<LONG*>(material+0xc)==1&&*reinterpret_cast<LONG*>(texture+0xc)==2,
        "missing manager gate rejects before registry read or resource retain");
    RemoveVectoredExceptionHandler(exceptionProbe);
    *reinterpret_cast<DWORD*>(manager+0xe0)=reinterpret_cast<DWORD>(gate);*reinterpret_cast<DWORD*>(manager+0x30)=reinterpret_cast<DWORD>(resourceNode);
    LONG dead=0;Check(!RetainPositive(&dead)&&dead==0,"zero references never resurrected");dead=-1;Check(!RetainPositive(&dead)&&dead==-1,"negative references rejected");dead=0x7ffffffe;Check(!RetainPositive(&dead),"retain overflow rejected");
    *reinterpret_cast<LONG*>(texture+0xc)=0;Check(!Material()&&*reinterpret_cast<LONG*>(material+0xc)==1&&!retainedMaterial,"failed second retain rolls native material reference back");*reinterpret_cast<LONG*>(texture+0xc)=2;
    Check(Material()==reinterpret_cast<DWORD>(material),"verified resident atlas");
    Check(*reinterpret_cast<LONG*>(material+0xc)==2&&*reinterpret_cast<LONG*>(texture+0xc)==3,"one native strong reference per owned resource");
    *reinterpret_cast<DWORD*>(texture+0x20)=64;Check(!Material(),"old native64 atlas rejected");*reinterpret_cast<DWORD*>(texture+0x20)=1024;
    Check(nativeLock->RecursionCount==0,"native manager lock released after successful lookup");
    *reinterpret_cast<DWORD*>(material+0x10)^=1;Check(!Material(),"wrong native material identity rejected");*reinterpret_cast<DWORD*>(material+0x10)^=1;
    *reinterpret_cast<DWORD*>(texture+0x10)^=1;Check(!Material(),"wrong native atlas layout identity rejected");*reinterpret_cast<DWORD*>(texture+0x10)^=1;
    retainedMaterial=retainedTexture=0;resourceNode[4]=1;Check(!Material()&&nativeLock->RecursionCount==0,"invalid resource releases native lock on SEH fallback");resourceNode[4]=reinterpret_cast<DWORD>(material);
    DWORD parentNode[9]{};parentNode[0]=resourceNode[0]-1;parentNode[1]=resourceNode[1];parentNode[6]=reinterpret_cast<DWORD>(resourceNode);parentNode[7]=0x121f9cc;
    *reinterpret_cast<DWORD*>(manager+0x30)=reinterpret_cast<DWORD>(parentNode);Check(RegistryMaterial(reinterpret_cast<DWORD>(manager))==reinterpret_cast<DWORD>(material),"native lower Name selects child18");
    parentNode[0]=resourceNode[0]+1;parentNode[6]=0x121f9cc;parentNode[7]=reinterpret_cast<DWORD>(resourceNode);Check(RegistryMaterial(reinterpret_cast<DWORD>(manager))==reinterpret_cast<DWORD>(material),"native greater Name selects child1C");
    parentNode[0]=resourceNode[0];parentNode[4]=0;Check(RegistryMaterial(reinterpret_cast<DWORD>(manager))==reinterpret_cast<DWORD>(material),"equal Name searches other namespace candidates");
    for(unsigned i=0;i<keyCount;++i){Check(Find(keys[i].label)==static_cast<int>(i),"all original key labels mapped");Check(keys[i].x+keys[i].width<=1024&&keys[i].y+64<=1024,"atlas bounds");}
    Check(Find("f")==Find("F")&&Find("Space bar")==Find("SPACE")&&Find("Left Shift")==Find("LSHIFT"),"native spelling aliases");
    Check(Find(";")==Find("SEMICOLON")&&Find("TAB")==49,"semicolon appended without changing existing key indexes");
    for(const char* invalid:{"","Leertaste","UNKNOWN","epc_key_","epc_key_99","epc_key_01evil","pad_A"})Check(Index(invalid)<0,"non-scoped names excluded");
    void* memory=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);Check(memory!=nullptr,"native resolver fixture allocated");
    const BYTE fixture[]={0x53,0x8b,0x5c,0x24,8,0x57,0x5f,0x5b,0x33,0xc0,0xc2,4,0};memcpy(memory,fixture,sizeof(fixture));resolverSite=reinterpret_cast<DWORD>(memory);
    ConfigurePcKeycapIcons(L"unused",false);InstallPcKeycapIcons();Check(!enabled&&static_cast<BYTE*>(memory)[0]==0x53,"unsupported build preserved");
    ConfigurePcKeycapIcons(L"unused",true);InstallPcKeycapIcons();Check(enabled&&pc_prompts::keycapEncoder&&static_cast<BYTE*>(memory)[0]==0xe9,"guarded native hook installed");
    struct Guard{DWORD before;Bitmap bitmap;DWORD after;}guard{0x11223344,{},0x55667788};
    for(unsigned i=0;i<keyCount;++i){
        char name[32];sprintf_s(name,"epc_key_%02u",i);Check(Invoke(name,&guard.bitmap),"compiled icon adapter resolves native descriptor");
        Check(guard.bitmap.aspect==static_cast<float>(keys[i].width)/64&&guard.bitmap.advance==1.25f*guard.bitmap.aspect&&guard.bitmap.baseline==.1f,"native key aspect/advance/baseline coherent");
        Check(guard.bitmap.u0==static_cast<float>(keys[i].x)/1024&&guard.bitmap.u1==static_cast<float>(keys[i].x+keys[i].width)/1024,"exact native UV region");
        Check(guard.before==0x11223344&&guard.after==0x55667788,"native32-byte output extent");
    }
    Check(!Invoke("native_pad_A",&guard.bitmap),"non-scoped native trampoline fallback");
    const LONG materialRefs=*reinterpret_cast<LONG*>(material+0xc),textureRefs=*reinterpret_cast<LONG*>(texture+0xc);
    Check(Material()==reinterpret_cast<DWORD>(material)&&*reinterpret_cast<LONG*>(material+0xc)==materialRefs&&*reinterpret_cast<LONG*>(texture+0xc)==textureRefs,"prompt reuse does not accumulate references");
    *reinterpret_cast<DWORD*>(manager+0x30)=0x121f9cc;Check(Material()==reinterpret_cast<DWORD>(material),"owned resources survive BANK map removal");
    char text[96]="#color:F2E6C9FF#[Z]#nocolor#";char* end=Encode(text,sizeof(text),text+strlen(text));Check(!strcmp(text,"#bitmap:epc_key_25#")&&end==text+strlen(text),"actual rebound Z label encoded");
    strcpy_s(text,"#color:F2E6C9FF#[;]#nocolor#");end=Encode(text,sizeof(text),text+strlen(text));
    char semicolonBitmap[32];sprintf_s(semicolonBitmap,"#bitmap:epc_key_%02u#",static_cast<unsigned>(Find(";")));
    Check(!strcmp(text,semicolonBitmap)&&end==text+strlen(text),"actual favorites semicolon binding encoded");
    strcpy_s(text,"#color:F2E6C9FF#[Leertaste]#nocolor#");end=Encode(text,sizeof(text),text+strlen(text));Check(strstr(text,"Leertaste")&&end==text+strlen(text),"unknown localized binding retained");
    *reinterpret_cast<DWORD*>(texture+0x20)=64;strcpy_s(text,"#color:F2E6C9FF#[F]#nocolor#");end=Encode(text,sizeof(text),text+strlen(text));Check(strstr(text,"[F]")&&end==text+strlen(text),"missing atlas retains readable label");
    Check(nativeLock->RecursionCount==0,"native lock balanced after full icon batch");DeleteCriticalSection(nativeLock);
    printf("PC native keycaps: %u checks PASS\n",checks);return 0;
}
