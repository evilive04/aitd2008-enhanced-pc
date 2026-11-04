// x86 D3D9 diagnostics plus an opt-in, exact-build display-settings fix.
// No on-disk game-binary modifications or protection removal.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <d3d9.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <new>

static HMODULE selfModule;
static HMODULE systemD3D;
static INIT_ONCE initOnce = INIT_ONCE_STATIC_INIT;
static SRWLOCK logLock = SRWLOCK_INIT;
static SRWLOCK objectLock = SRWLOCK_INIT;
static bool diagnostics = false;
static bool displaySettingsFix = false;
static SRWLOCK patchLock = SRWLOCK_INIT;
static bool displayPatchApplied = false;
static wchar_t logPath[MAX_PATH];
extern "C" { void* exports[23] = {}; }

static bool ConfigFlag(const wchar_t* section,const wchar_t* key,bool fallback,const wchar_t* path) {
    wchar_t value[32];
    GetPrivateProfileStringW(section,key,fallback ? L"true" : L"false",value,32,path);
    if (!_wcsicmp(value,L"true") || !wcscmp(value,L"1")) return true;
    if (!_wcsicmp(value,L"false") || !wcscmp(value,L"0")) return false;
    return fallback;
}

static void Log(const char* format, ...) {
    if (!diagnostics || !logPath[0]) return;
    const DWORD savedError = GetLastError();
    char message[2048];
    va_list args;
    va_start(args, format);
    vsnprintf_s(message, sizeof(message), _TRUNCATE, format, args);
    va_end(args);
    char line[2300];
    SYSTEMTIME time;
    GetLocalTime(&time);
    const int count = _snprintf_s(line, sizeof(line), _TRUNCATE,
        "%04u-%02u-%02u %02u:%02u:%02u.%03u pid=%lu tid=%lu %s\r\n",
        time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,time.wMilliseconds,
        GetCurrentProcessId(),GetCurrentThreadId(),message);
    AcquireSRWLockExclusive(&logLock);
    HANDLE file = CreateFileW(logPath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteFile(file,line,static_cast<DWORD>(count > 0 ? count : 0),&written,nullptr);
        CloseHandle(file);
    }
    ReleaseSRWLockExclusive(&logLock);
    SetLastError(savedError);
}

#include "frame_pacing.inl"

static bool SupportedExecutable() {
    wchar_t path[MAX_PATH];
    if (!GetModuleFileNameW(nullptr,path,MAX_PATH)) return false;
    HANDLE file = CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    unsigned char result[32] = {};
    bool success = BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0) >= 0;
    if (success) success = BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0) >= 0;
    unsigned char buffer[65536];
    DWORD size;
    while (success) {
        if (!ReadFile(file,buffer,sizeof(buffer),&size,nullptr)) { success=false; break; }
        if (!size) break;
        success = BCryptHashData(hash,buffer,size,0) >= 0;
    }
    if (success) success = BCryptFinishHash(hash,result,sizeof(result),0) >= 0;
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm,0);
    CloseHandle(file);
    const unsigned char expected[32] = {
        0xd8,0xa7,0x37,0x0a,0x1f,0x4f,0x24,0x18,0xcf,0xeb,0x2f,0x22,0xc0,0xd2,0xe9,0xda,
        0xfa,0x59,0xf6,0x06,0xff,0x83,0xfb,0xb6,0xfc,0x83,0x11,0xba,0x84,0xdf,0x48,0x18};
    return success && memcmp(result,expected,32)==0;
}

static BOOL CALLBACK Initialize(PINIT_ONCE,void*,void**) {
    wchar_t path[MAX_PATH];
    UINT length=GetSystemDirectoryW(path,MAX_PATH);
    if (!length || length+12 >= MAX_PATH) return FALSE;
    wcscat_s(path,L"\\d3d9.dll");
    systemD3D=LoadLibraryW(path);
    if (!systemD3D) return FALSE;
    // Preserve every ordinal from the locally audited Windows D3D9 export table.
    for (unsigned i=0;i<23;++i) exports[i]=reinterpret_cast<void*>(GetProcAddress(systemD3D,MAKEINTRESOURCEA(i+16)));
    if (!exports[13] || !exports[15] || !exports[21]) return FALSE;
    wchar_t config[MAX_PATH];
    GetModuleFileNameW(selfModule,config,MAX_PATH);
    wchar_t* slash=wcsrchr(config,L'\\');
    if (!slash) return TRUE;
    wcscpy_s(slash,MAX_PATH-static_cast<size_t>(slash-config),L"\\aitd-enhanced.ini");
    const bool enabled=ConfigFlag(L"General",L"Enabled",true,config);
    diagnostics=enabled && ConfigFlag(L"Diagnostics",L"Enabled",false,config);
    displaySettingsFix=enabled && ConfigFlag(L"Compatibility",L"FixDisplaySettingsCrash",false,config);
    DWORD envLength=GetEnvironmentVariableW(L"LOCALAPPDATA",path,MAX_PATH);
    if (envLength && envLength+65 < MAX_PATH) {
        wcscat_s(path,L"\\AITD2008Enhanced");
        CreateDirectoryW(path,nullptr);
        wcscat_s(path,L"\\logs");
        CreateDirectoryW(path,nullptr);
        swprintf_s(logPath,L"%s\\d3d9-%lu.log",path,GetCurrentProcessId());
    }
    const bool supported=SupportedExecutable();
    Log("proxy=0.0.4-x86 supported_sha256=%d system_d3d9=loaded",supported);
    if (!supported) {
        Log("Unknown executable: forwarding only, observation and fixes disabled");
        diagnostics=false;
        displaySettingsFix=false;
    }
    if (supported && enabled) {
        InitializeFramePacing(config);
        const UINT requested=GetPrivateProfileIntW(L"Compatibility",L"LimitLogicalCores",0,config);
        if (requested>0 && requested<=32) {
            DWORD_PTR processMask=0,systemMask=0,selectedMask=0;
            if (GetProcessAffinityMask(GetCurrentProcess(),&processMask,&systemMask)) {
                UINT selected=0;
                for (unsigned bit=0;bit<32 && selected<requested;++bit) {
                    const DWORD_PTR mask=static_cast<DWORD_PTR>(1)<<bit;
                    if (processMask & mask) { selectedMask|=mask; ++selected; }
                }
                if (selectedMask && SetProcessAffinityMask(GetCurrentProcess(),selectedMask))
                    Log("Process affinity old=0x%08lx selected=0x%08lx logical_cores=%u",processMask,selectedMask,selected);
                else
                    Log("Process affinity unchanged: API failed, error=%lu",GetLastError());
            }
        }
    }
    return TRUE;
}

extern "C" void __cdecl EnsureExports() {
    if (!InitOnceExecuteOnce(&initOnce,Initialize,nullptr,nullptr))
        RaiseFailFastException(nullptr,nullptr,0);
}

struct ObjectRecord {
    void** table;
    unsigned index;
    void* originalMethod;
    ObjectRecord* next;
};
static ObjectRecord* objects=nullptr;

static void* Original(void* object, unsigned index) {
    void* result=nullptr;
    AcquireSRWLockShared(&objectLock);
    for (auto* record=objects;record;record=record->next)
        if (record->table==*reinterpret_cast<void***>(object) && index==record->index) { result=record->originalMethod; break; }
    ReleaseSRWLockShared(&objectLock);
    return result;
}

static void LogParameters(const char* operation,const D3DPRESENT_PARAMETERS* p) {
    if (!p) { Log("%s parameters=null",operation); return; }
    Log("%s width=%u height=%u format=%u count=%u windowed=%d refresh=%u interval=%u swap=%u hwnd=%p",
        operation,p->BackBufferWidth,p->BackBufferHeight,p->BackBufferFormat,p->BackBufferCount,
        p->Windowed,p->FullScreen_RefreshRateInHz,p->PresentationInterval,p->SwapEffect,p->hDeviceWindow);
}

static HRESULT STDMETHODCALLTYPE ObservedReset(IDirect3DDevice9* object,D3DPRESENT_PARAMETERS* parameters) {
    auto reset=reinterpret_cast<HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,D3DPRESENT_PARAMETERS*)>(Original(object,16));
    ResetFrameDevice(object,true);
    LogParameters("Reset requested",parameters);
    const HRESULT result=reset(object,parameters);
    ResetFrameDevice(object,FAILED(result),SUCCEEDED(result) && parameters ? parameters->hDeviceWindow : nullptr);
    Log("Reset result=0x%08lx",static_cast<unsigned long>(result));
    LogParameters("Reset returned",parameters);
    return result;
}

static HRESULT STDMETHODCALLTYPE ObservedPresent(IDirect3DDevice9* object,const RECT* source,
    const RECT* destination,HWND window,const RGNDATA* dirty) {
    auto present=reinterpret_cast<HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,const RECT*,const RECT*,HWND,const RGNDATA*)>(Original(object,17));
    PaceFrame(object,window);
    const HRESULT result=present(object,source,destination,window,dirty);
    if (FAILED(result)) ResetFrameDevice(object,result==D3DERR_DEVICELOST || result==D3DERR_DEVICEREMOVED);
    return result;
}

static ULONG STDMETHODCALLTYPE ObservedRelease(IDirect3DDevice9* object) {
    auto release=reinterpret_cast<ULONG (STDMETHODCALLTYPE*)(IDirect3DDevice9*)>(Original(object,2));
    const ULONGLONG generation=FrameDeviceGeneration(object);
    const ULONG references=release(object);
    if (!references) ForgetFrameDevice(object,generation); // Do not dereference destroyed/reused objects.
    return references;
}

static bool Observe(void* object,unsigned index,void* method) {
    if (!object || !(diagnostics || displaySettingsFix || frameLimit)) return false;
    auto** table=*reinterpret_cast<void***>(object);
    AcquireSRWLockExclusive(&objectLock);
    for (auto* record=objects;record;record=record->next)
        if (record->table==table && record->index==index) {
            const bool installed=table[index]==method;
            ReleaseSRWLockExclusive(&objectLock); return installed;
        }
    auto* record=new(std::nothrow) ObjectRecord;
    if (!record) { ReleaseSRWLockExclusive(&objectLock); return false; }
    DWORD protection;
    if (!VirtualProtect(&table[index],sizeof(void*),PAGE_READWRITE,&protection)) {
        delete record; ReleaseSRWLockExclusive(&objectLock); return false;
    }
    *record={table,index,table[index],objects};
    objects=record;
    // Preserve the actual COM objects, identity, reference counting and vptrs.
    // Each patched slot retains its own original target (copy-on-write).
    InterlockedExchangePointer(reinterpret_cast<void* volatile*>(&table[index]),method);
    DWORD ignored;
    VirtualProtect(&table[index],sizeof(void*),protection,&ignored);
    ReleaseSRWLockExclusive(&objectLock);
    return true;
}

static void ApplyDisplaySettingsFix() {
    if (!displaySettingsFix) return;
    AcquireSRWLockExclusive(&patchLock);
    if (displayPatchApplied) { ReleaseSRWLockExclusive(&patchLock); return; }
    auto* base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    // Exact 08.19.14 build only. Candidate RVAs verified in live original code.
    auto* multiply=base+0x845b;
    auto* address=base+0x96f4;
    const unsigned char expectedMultiply[]={0x69,0xed,0x3c,0x01,0x00,0x00};
    const unsigned char expectedAddress[]={0x8d,0x14,0x81};
    if (memcmp(multiply,expectedMultiply,sizeof(expectedMultiply)) ||
        memcmp(address,expectedAddress,sizeof(expectedAddress))) {
        Log("DisplaySettingsFix skipped: runtime signature mismatch (initialization may not be complete)");
        ReleaseSRWLockExclusive(&patchLock);
        return;
    }
    DWORD firstProtection,secondProtection,ignored;
    if (!VirtualProtect(multiply,sizeof(expectedMultiply),PAGE_EXECUTE_READWRITE,&firstProtection)) {
        Log("DisplaySettingsFix skipped: first page not writable");
        ReleaseSRWLockExclusive(&patchLock);
        return;
    }
    if (!VirtualProtect(address,sizeof(expectedAddress),PAGE_EXECUTE_READWRITE,&secondProtection)) {
        VirtualProtect(multiply,sizeof(expectedMultiply),firstProtection,&ignored);
        Log("DisplaySettingsFix skipped: second page not writable");
        ReleaseSRWLockExclusive(&patchLock);
        return;
    }
    multiply[2]=0x00;
    address[2]=0x41;
    FlushInstructionCache(GetCurrentProcess(),multiply,sizeof(expectedMultiply));
    FlushInstructionCache(GetCurrentProcess(),address,sizeof(expectedAddress));
    VirtualProtect(multiply,sizeof(expectedMultiply),firstProtection,&ignored);
    VirtualProtect(address,sizeof(expectedAddress),secondProtection,&ignored);
    displayPatchApplied=true;
    Log("DisplaySettingsFix applied: exact hash and both runtime signatures matched");
    ReleaseSRWLockExclusive(&patchLock);
}

static HRESULT STDMETHODCALLTYPE ObservedCreateDevice(IDirect3D9* object,UINT adapter,D3DDEVTYPE type,
    HWND window,DWORD behavior,D3DPRESENT_PARAMETERS* parameters,IDirect3DDevice9** device) {
    auto create=reinterpret_cast<HRESULT (STDMETHODCALLTYPE*)(IDirect3D9*,UINT,D3DDEVTYPE,HWND,DWORD,D3DPRESENT_PARAMETERS*,IDirect3DDevice9**)>(Original(object,16));
    ApplyDisplaySettingsFix();
    Log("CreateDevice adapter=%u type=%u behavior=0x%08lx",adapter,type,behavior);
    LogParameters("CreateDevice requested",parameters);
    const HRESULT result=create(object,adapter,type,window,behavior,parameters,device);
    Log("CreateDevice result=0x%08lx",static_cast<unsigned long>(result));
    LogParameters("CreateDevice returned",parameters);
    if (SUCCEEDED(result) && device && *device) {
        if (diagnostics || frameLimit) {
            const bool resetHook=Observe(*device,16,reinterpret_cast<void*>(&ObservedReset));
            Log("Device observation=%d",resetHook);
            if (frameLimit && type==D3DDEVTYPE_HAL && !(behavior & D3DCREATE_ADAPTERGROUP_DEVICE) && resetHook &&
                Observe(*device,2,reinterpret_cast<void*>(&ObservedRelease)) &&
                Observe(*device,17,reinterpret_cast<void*>(&ObservedPresent))) {
                RegisterFrameDevice(*device,parameters && parameters->hDeviceWindow ? parameters->hDeviceWindow : window);
            }
        }
        IDirect3DSurface9* backbuffer=nullptr;
        if (SUCCEEDED((*device)->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&backbuffer))) {
            D3DSURFACE_DESC description={};
            if (SUCCEEDED(backbuffer->GetDesc(&description)))
                Log("Backbuffer actual=%ux%u format=%u",description.Width,description.Height,description.Format);
            backbuffer->Release();
        }
    }
    return result;
}

extern "C" IDirect3D9* WINAPI ProxyCreate9(UINT sdk) {
    EnsureExports();
    auto create=reinterpret_cast<IDirect3D9* (WINAPI*)(UINT)>(exports[21]);
    IDirect3D9* object=create(sdk);
    Log("Direct3DCreate9 sdk=%u result=%p",sdk,object);
    if (object && (diagnostics || displaySettingsFix || frameLimit)) {
        if (diagnostics) for (UINT adapter=0;adapter<object->GetAdapterCount();++adapter) {
            D3DADAPTER_IDENTIFIER9 identifier={};
            if (SUCCEEDED(object->GetAdapterIdentifier(adapter,0,&identifier)))
                Log("Adapter=%u description=%s vendor=0x%x device=0x%x",adapter,identifier.Description,identifier.VendorId,identifier.DeviceId);
            const D3DFORMAT formats[]={D3DFMT_X8R8G8B8,D3DFMT_R5G6B5,D3DFMT_A2R10G10B10};
            for (auto format:formats) {
                const UINT count=object->GetAdapterModeCount(adapter,format);
                Log("Modes adapter=%u format=%u count=%u",adapter,format,count);
            }
        }
        Log("D3D observation=%d",Observe(object,16,reinterpret_cast<void*>(&ObservedCreateDevice)));
    }
    return object;
}

BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID) {
    if (reason==DLL_PROCESS_ATTACH) { selfModule=module; DisableThreadLibraryCalls(module); }
    return TRUE;
}
