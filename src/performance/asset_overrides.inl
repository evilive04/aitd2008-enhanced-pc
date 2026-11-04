// Original-preserving native archive cache. Disabled until explicitly enabled.
// Compiler: tools/compile_asset_mods.py. Contains no game payloads.
#include <cwctype>
namespace asset_overrides {
using OpenA = HANDLE (WINAPI*)(LPCSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE);
static OpenA originalOpen = nullptr;
static bool enabled = false;
static volatile LONG redirectedOpens = 0;
static wchar_t gameRoot[MAX_PATH]{};
static wchar_t cacheRoot[MAX_PATH]{};
struct Archive { bool active{}; wchar_t original[2][MAX_PATH]{}; wchar_t cached[2][MAX_PATH]{}; HANDLE locks[2]{INVALID_HANDLE_VALUE,INVALID_HANDLE_VALUE}; };
static Archive archives[3];

static bool SafeGeneration(const wchar_t* value) {
    if(wcslen(value)!=32)return false;
    for(unsigned n=0;n<32;++n)if(!((value[n]>=L'0'&&value[n]<=L'9')||(value[n]>=L'a'&&value[n]<=L'f')))return false;
    return true;
}
static bool NoReparse(const wchar_t* value) {
    wchar_t path[MAX_PATH]{};
    if(wcslen(value)>=MAX_PATH)return false;
    wcscpy_s(path,value);
    for(wchar_t* p=path+3;;++p){
        if(*p!=L'\\'&&*p!=L'\0')continue;
        const wchar_t saved=*p;*p=0;
        const DWORD attrs=GetFileAttributesW(path);*p=saved;
        if(attrs==INVALID_FILE_ATTRIBUTES||(attrs&FILE_ATTRIBUTE_REPARSE_POINT))return false;
        if(!saved)break;
    }
    return true;
}
static bool HashMatches(HANDLE file,const wchar_t* expected) {
    if(wcslen(expected)!=64)return false;
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    bool ok=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0;
    if(ok)ok=BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0;
    unsigned char buffer[65536],result[32]{};DWORD read=0;
    LARGE_INTEGER zero{};
    if(ok)ok=SetFilePointerEx(file,zero,nullptr,FILE_BEGIN)!=FALSE;
    while(ok){
        if(!ReadFile(file,buffer,sizeof(buffer),&read,nullptr)){ok=false;break;}
        if(!read)break;
        ok=BCryptHashData(hash,buffer,read,0)>=0;
    }
    if(ok)ok=BCryptFinishHash(hash,result,32,0)>=0;
    if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);
    constexpr wchar_t digits[]=L"0123456789abcdef";
    if(ok)for(unsigned n=0;n<32;++n)if(expected[n*2]!=digits[result[n]>>4]||expected[n*2+1]!=digits[result[n]&15])ok=false;
    return ok;
}
static void Clear() {
    for(auto& archive:archives){
        archive.active=false;
        for(auto& file:archive.locks)if(file!=INVALID_HANDLE_VALUE){CloseHandle(file);file=INVALID_HANDLE_VALUE;}
    }
    enabled=false;
}
static bool Configure(const wchar_t* config,bool supported) {
    Clear();
    if(!supported||!ConfigFlag(L"Mods",L"AssetOverrides",false,config))return false;
    if(!GetModuleFileNameW(nullptr,gameRoot,MAX_PATH))return false;
    wchar_t* slash=wcsrchr(gameRoot,L'\\');if(!slash)return false;*slash=0;
    if(swprintf_s(cacheRoot,L"%s\\mods\\.cache",gameRoot)<0||!NoReparse(cacheRoot))return false;
    wchar_t manifest[MAX_PATH]{},generation[64]{},version[16]{},expectedExe[80]{};
    if(swprintf_s(manifest,L"%s\\active.ini",cacheRoot)<0||!NoReparse(manifest))return false;
    GetPrivateProfileStringW(L"Cache",L"Version",L"",version,16,manifest);
    GetPrivateProfileStringW(L"Cache",L"Generation",L"",generation,64,manifest);
    GetPrivateProfileStringW(L"Cache",L"ExecutableSHA256",L"",expectedExe,80,manifest);
    if(wcscmp(version,L"1")||!SafeGeneration(generation)||wcscmp(expectedExe,L"d8a7370a1f4f2418cfeb2f22c0d2e9dafa59f606ff83fbb6fc8311ba84df4818"))return false;
    for(unsigned a=0;a<3;++a){
        wchar_t section[16]{};swprintf_s(section,L"Archive%c",L'A'+a);
        if(GetPrivateProfileIntW(section,L"Enabled",0,manifest)!=1)continue;
        auto& archive=archives[a];
        for(unsigned f=0;f<2;++f){
            const wchar_t* extension=f?L"fat":L"big";
            wchar_t expected[80]{};
            GetPrivateProfileStringW(section,f?L"FatSHA256":L"BigSHA256",L"",expected,80,manifest);
            if(swprintf_s(archive.original[f],L"%s\\alone_compressed_%c.%s",gameRoot,L'a'+a,extension)<0||
               swprintf_s(archive.cached[f],L"%s\\%s\\alone_compressed_%c.%s",cacheRoot,generation,L'a'+a,extension)<0||
               !NoReparse(archive.cached[f])){Clear();return false;}
            archive.locks[f]=CreateFileW(archive.cached[f],GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(archive.locks[f]==INVALID_HANDLE_VALUE||!HashMatches(archive.locks[f],expected)){Clear();return false;}
        }
        archive.active=true;enabled=true;
    }
    return enabled;
}
static const wchar_t* Resolve(const char* name,DWORD access,DWORD disposition) {
    if(!enabled||!name||access!=GENERIC_READ||disposition!=OPEN_EXISTING)return nullptr;
    wchar_t wide[MAX_PATH]{},full[MAX_PATH]{};
    if(!MultiByteToWideChar(CP_ACP,MB_ERR_INVALID_CHARS,name,-1,wide,MAX_PATH))return nullptr;
    const DWORD length=GetFullPathNameW(wide,MAX_PATH,full,nullptr);
    if(!length||length>=MAX_PATH)return nullptr;
    for(auto& archive:archives)if(archive.active)for(unsigned f=0;f<2;++f)
        if(!_wcsicmp(full,archive.original[f]))return archive.cached[f];
    return nullptr;
}
static HANDLE WINAPI Open(LPCSTR name,DWORD access,DWORD share,LPSECURITY_ATTRIBUTES security,DWORD disposition,DWORD flags,HANDLE templateFile) {
    const DWORD incomingError=GetLastError();
    const wchar_t* replacement=Resolve(name,access,disposition);
    if(replacement){
        // Cache files stay locked against mutation/deletion for process lifetime.
        if((flags&FILE_FLAG_DELETE_ON_CLOSE)||(flags&FILE_FLAG_OPEN_REPARSE_POINT)){SetLastError(ERROR_ACCESS_DENIED);return INVALID_HANDLE_VALUE;}
        if(!NoReparse(replacement)){SetLastError(ERROR_ACCESS_DENIED);return INVALID_HANDLE_VALUE;}
        HANDLE result=CreateFileW(replacement,access,share|FILE_SHARE_READ,security,disposition,flags,templateFile);
        DWORD resultError=GetLastError();
        if(result!=INVALID_HANDLE_VALUE){
            HANDLE locked=INVALID_HANDLE_VALUE;
            for(auto& archive:archives)for(unsigned f=0;f<2;++f)if(replacement==archive.cached[f])locked=archive.locks[f];
            BY_HANDLE_FILE_INFORMATION first{},second{};
            if(locked==INVALID_HANDLE_VALUE||!GetFileInformationByHandle(locked,&first)||!GetFileInformationByHandle(result,&second)||
               first.dwVolumeSerialNumber!=second.dwVolumeSerialNumber||first.nFileIndexHigh!=second.nFileIndexHigh||first.nFileIndexLow!=second.nFileIndexLow){
                CloseHandle(result);resultError=ERROR_ACCESS_DENIED;result=INVALID_HANDLE_VALUE;
            }
        }
        const LONG count=InterlockedIncrement(&redirectedOpens);
        if(count<=24)Log("asset_override open=%ld success=%d native_request=%s",count,result!=INVALID_HANDLE_VALUE,name);
        SetLastError(resultError);
        return result;
    }
    SetLastError(incomingError);
    return originalOpen(name,access,share,security,disposition,flags,templateFile);
}
static bool InstallAtSlot(void** slot) {
    if(!enabled)return false;
    const auto expected=reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"CreateFileA"));
    if(*slot!=expected)return false;
    DWORD old=0;
    if(!VirtualProtect(slot,sizeof(void*),PAGE_READWRITE,&old))return false;
    originalOpen=reinterpret_cast<OpenA>(*slot);
    InterlockedExchangePointer(slot,reinterpret_cast<void*>(&Open));
    DWORD ignored=0;VirtualProtect(slot,sizeof(void*),old,&ignored);
    return true;
}
static bool Install() {
    // Original Steam PE IAT candidate: kernel32!CreateFileA at RVA7850A0. Configure requires supported SHA.
    return InstallAtSlot(reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr))+0x7850A0));
}
} // namespace asset_overrides
