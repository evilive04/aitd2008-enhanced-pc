// Game-local DirectInput8 observation. Never records text or global keyboard input.
#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <bcrypt.h>
#include <dinput.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <new>
#include <intrin.h>
#include <cmath>
#include <cstdlib>

static HMODULE selfModule,systemModule;
static INIT_ONCE initOnce=INIT_ONCE_STATIC_INIT;
static SRWLOCK logLock=SRWLOCK_INIT,tableLock=SRWLOCK_INIT,deviceLock=SRWLOCK_INIT;
static bool enabled=false,objectMetadata=false;
static bool inputObservation=false;
static volatile LONG startupStackLogged=0;
static wchar_t logPath[MAX_PATH];
static FARPROC functions[6];

static void Log(const char* format,...) {
    if (!enabled || !logPath[0]) return;
    const DWORD error=GetLastError();
    char message[1800],line[2048];
    va_list args;
    va_start(args,format);
    vsnprintf_s(message,sizeof(message),_TRUNCATE,format,args);
    va_end(args);
    const int length=_snprintf_s(line,sizeof(line),_TRUNCATE,"tick=%llu pid=%lu tid=%lu %s\r\n",
        GetTickCount64(),GetCurrentProcessId(),GetCurrentThreadId(),message);
    AcquireSRWLockExclusive(&logLock);
    HANDLE file=CreateFileW(logPath,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,0,nullptr);
    if (file!=INVALID_HANDLE_VALUE) {
        DWORD written;
        if (length>0) WriteFile(file,line,static_cast<DWORD>(length),&written,nullptr);
        CloseHandle(file);
    }
    ReleaseSRWLockExclusive(&logLock);
    SetLastError(error);
}

static bool SupportedExecutable() {
    wchar_t path[MAX_PATH];
    if (!GetModuleFileNameW(nullptr,path,MAX_PATH)) return false;
    Log("Executable hash path=%ls",path);
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if (file==INVALID_HANDLE_VALUE) {
        Log("Executable hash: open failed error=%lu",GetLastError());
        return false;
    }
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    BCRYPT_HASH_HANDLE hash=nullptr;
    unsigned char value[32]={};
    LARGE_INTEGER fileSize={};
    HANDLE mapping=nullptr;
    const unsigned char* data=nullptr;
    bool mapped=GetFileSizeEx(file,&fileSize) && fileSize.QuadPart>0 && fileSize.QuadPart<64*1024*1024;
    if (mapped) mapping=CreateFileMappingW(file,nullptr,PAGE_READONLY,0,0,nullptr);
    if (mapping) data=static_cast<const unsigned char*>(MapViewOfFile(mapping,FILE_MAP_READ,0,0,0));
    mapped=data!=nullptr;
    NTSTATUS status=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0);
    bool ok=mapped && status>=0;
    if (ok) { status=BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0); ok=status>=0; }
    if (ok) { status=BCryptHashData(hash,const_cast<unsigned char*>(data),static_cast<ULONG>(fileSize.QuadPart),0); ok=status>=0; }
    if (ok) { status=BCryptFinishHash(hash,value,sizeof(value),0); ok=status>=0; }
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm,0);
    if (data) UnmapViewOfFile(data);
    if (mapping) CloseHandle(mapping);
    CloseHandle(file);
    const unsigned char expected[32]={
        0xd8,0xa7,0x37,0x0a,0x1f,0x4f,0x24,0x18,0xcf,0xeb,0x2f,0x22,0xc0,0xd2,0xe9,0xda,
        0xfa,0x59,0xf6,0x06,0xff,0x83,0xfb,0xb6,0xfc,0x83,0x11,0xba,0x84,0xdf,0x48,0x18};
    char hex[65]={};
    for (unsigned i=0;i<32;++i) sprintf_s(hex+i*2,3,"%02x",value[i]);
    Log("Executable mapped hash: ok=%d status=0x%08lx bytes=%llu sha256=%s",ok,static_cast<unsigned long>(status),static_cast<unsigned long long>(fileSize.QuadPart),hex);
    return ok && memcmp(value,expected,32)==0;
}

static bool ConfigFlag(const wchar_t* section,const wchar_t* key,bool fallback,const wchar_t* path) {
    wchar_t text[32];
    GetPrivateProfileStringW(section,key,fallback?L"true":L"false",text,32,path);
    if (!_wcsicmp(text,L"true") || !wcscmp(text,L"1")) return true;
    if (!_wcsicmp(text,L"false") || !wcscmp(text,L"0")) return false;
    return fallback;
}

#include "camera_controls.inl"

static BOOL CALLBACK Initialize(PINIT_ONCE,void*,void**) {
    wchar_t path[MAX_PATH];
    const UINT length=GetSystemDirectoryW(path,MAX_PATH);
    if (!length || length+14>=MAX_PATH) return FALSE;
    wcscat_s(path,L"\\dinput8.dll");
    systemModule=LoadLibraryW(path);
    if (!systemModule) return FALSE;
    const char* names[]={"DirectInput8Create","DllCanUnloadNow","DllGetClassObject","DllRegisterServer","DllUnregisterServer","GetdfDIJoystick"};
    for (unsigned i=0;i<6;++i) {
        functions[i]=GetProcAddress(systemModule,names[i]);
        if (!functions[i]) return FALSE;
    }
    GetModuleFileNameW(selfModule,path,MAX_PATH);
    wchar_t* slash=wcsrchr(path,L'\\');
    if (!slash) return TRUE;
    wcscpy_s(slash,MAX_PATH-static_cast<size_t>(slash-path),L"\\aitd-enhanced.ini");
    inputObservation=ConfigFlag(L"General",L"Enabled",true,path) && ConfigFlag(L"InputDiagnostics",L"Enabled",false,path);
    ConfigureCamera(path);
    enabled=inputObservation || cameraRequested;
    if (!enabled) return TRUE;
    objectMetadata=ConfigFlag(L"InputDiagnostics",L"ObjectMetadata",false,path);
    const DWORD envLength=GetEnvironmentVariableW(L"LOCALAPPDATA",path,MAX_PATH);
    if (envLength && envLength+65<MAX_PATH) {
        wcscat_s(path,L"\\AITD2008Enhanced"); CreateDirectoryW(path,nullptr);
        wcscat_s(path,L"\\logs"); CreateDirectoryW(path,nullptr);
        swprintf_s(logPath,L"%s\\input-%lu.log",path,GetCurrentProcessId());
    }
    const bool supported=SupportedExecutable();
    Log("input_proxy=0.0.10 supported_sha256=%d bounded_state_samples=64 foreground_only=1 keyboard_text=never caller_rva=enabled startup_stack=once_max24 object_metadata=%d camera_requested=%d",supported,objectMetadata,cameraRequested);
    if (!supported) { Log("Unknown executable: forwarding only"); enabled=false; cameraRequested=false; inputObservation=false; return TRUE; }
    HMODULE pinned=nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&Initialize),&pinned);
    return TRUE;
}

static void Ensure() {
    if (!InitOnceExecuteOnce(&initOnce,Initialize,nullptr,nullptr)) RaiseFailFastException(nullptr,nullptr,0);
}

struct TableRecord { void** table; unsigned slot; void* original; TableRecord* next; };
static TableRecord* tables=nullptr;
static void* Original(void* object,unsigned slot) {
    void* result=nullptr;
    auto** table=*reinterpret_cast<void***>(object);
    AcquireSRWLockShared(&tableLock);
    for (auto* record=tables;record;record=record->next)
        if (record->table==table && record->slot==slot) { result=record->original; break; }
    ReleaseSRWLockShared(&tableLock);
    return result;
}

static bool Observe(void* object,unsigned slot,void* method) {
    if (!enabled || !object) return false;
    auto** table=*reinterpret_cast<void***>(object);
    AcquireSRWLockExclusive(&tableLock);
    for (auto* record=tables;record;record=record->next)
        if (record->table==table && record->slot==slot) { ReleaseSRWLockExclusive(&tableLock); return true; }
    auto* record=new(std::nothrow) TableRecord;
    if (!record) { ReleaseSRWLockExclusive(&tableLock); return false; }
    DWORD protection,ignored;
    if (!VirtualProtect(&table[slot],sizeof(void*),PAGE_READWRITE,&protection)) {
        delete record; ReleaseSRWLockExclusive(&tableLock); return false;
    }
    *record={table,slot,table[slot],tables}; tables=record;
    InterlockedExchangePointer(reinterpret_cast<void* volatile*>(&table[slot]),method);
    VirtualProtect(&table[slot],sizeof(void*),protection,&ignored);
    ReleaseSRWLockExclusive(&tableLock);
    return true;
}

enum class DeviceKind { Other,Mouse,Keyboard };
struct DeviceRecord {
    void* object; DeviceKind kind; DWORD bytes,flags; unsigned stateSamples,dataSamples;
    bool metadataSampled;
    DeviceRecord* next;
};
static DeviceRecord* devices=nullptr;

static bool GameForeground() {
    DWORD pid=0;
    GetWindowThreadProcessId(GetForegroundWindow(),&pid);
    return pid==GetCurrentProcessId();
}

static DWORD CallerRva(void* address) {
    const DWORD error=GetLastError();
    MEMORY_BASIC_INFORMATION region={};
    HMODULE game=GetModuleHandleW(nullptr);
    DWORD result=0xffffffff;
    if (VirtualQuery(address,&region,sizeof(region)) && region.AllocationBase==game)
        result=static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(address)-reinterpret_cast<ULONG_PTR>(game));
    SetLastError(error);
    return result;
}

#pragma optimize("y",off)
__declspec(noinline) static void LogStartupStack() {
    if (!enabled || InterlockedCompareExchange(&startupStackLogged,1,0)!=0) return;
    const DWORD error=GetLastError();
    void* frames[24]={};
    const USHORT count=CaptureStackBackTrace(0,24,frames,nullptr);
    char chain[512]={};
    size_t used=0;
    unsigned gameFrames=0;
    for (USHORT i=0;i<count;++i) {
        const DWORD rva=CallerRva(frames[i]);
        if (rva==0xffffffff) continue;
        const int length=_snprintf_s(chain+used,sizeof(chain)-used,_TRUNCATE,
            "%s0x%08lx",gameFrames?",":"",rva);
        if (length<0) break;
        used+=static_cast<size_t>(length);
        ++gameFrames;
    }
    Log("StartupCallChain captured=%u exe_frames=%u max_frames=24 rvas_top_to_bottom=%s unwind=best_effort no_stack_bytes=1",
        static_cast<unsigned>(count),gameFrames,chain[0]?chain:"none");
    SetLastError(error);
}
#pragma optimize("",on)

// A/W object descriptors share this prefix. Names and state data are not read.
struct ObjectMetadataPrefix { DWORD size; GUID guid; DWORD offset,type,flags; };
static_assert(sizeof(ObjectMetadataPrefix)==32,"DirectInput object prefix layout");
using ObjectCallback=BOOL (CALLBACK*)(const void*,void*);
struct ObjectEnumeration {
    ObjectCallback callback; void* reference; void* device; unsigned seen; bool stopped;
};

static BOOL CALLBACK ObserveObject(const void* object,void* reference) {
    auto* context=static_cast<ObjectEnumeration*>(reference);
    const DWORD error=GetLastError();
    if (context->seen<0xffffffffu) ++context->seen;
    if (context->seen<=256 && object) {
        DWORD size=0;
        memcpy(&size,object,sizeof(size));
        if (size>=sizeof(ObjectMetadataPrefix)) {
            ObjectMetadataPrefix metadata={};
            memcpy(&metadata,object,sizeof(metadata));
            Log("EnumObjectMetadata device=%p ordinal=%u offset=%lu type=0x%lx flags=0x%lx descriptor_size=%lu no_name_or_key_state=1",
                context->device,context->seen,metadata.offset,metadata.type,metadata.flags,size);
        } else {
            Log("EnumObjectMetadata device=%p ordinal=%u descriptor_size=%lu prefix_unavailable=1",
                context->device,context->seen,size);
        }
    }
    SetLastError(error);
    const BOOL result=context->callback(object,context->reference);
    if (result==DIENUM_STOP) context->stopped=true;
    return result;
}

static HRESULT STDMETHODCALLTYPE DeviceObjects(void* object,ObjectCallback callback,void* reference,DWORD flags) {
    const DWORD error=GetLastError();
    auto original=reinterpret_cast<HRESULT (STDMETHODCALLTYPE*)(void*,ObjectCallback,void*,DWORD)>(Original(object,4));
    bool sample=false;
    // Ignore preliminary capability probes: this exact callback assigns key names.
    if (enabled && objectMetadata && callback && CallerRva(reinterpret_cast<void*>(callback))==0x576c30) {
        AcquireSRWLockExclusive(&deviceLock);
        for (auto* record=devices;record;record=record->next) if (record->object==object) {
            sample=record->kind==DeviceKind::Keyboard && !record->metadataSampled;
            if (sample) record->metadataSampled=true;
            break;
        }
        ReleaseSRWLockExclusive(&deviceLock);
    }
    if (!sample) {
        SetLastError(error);
        return original(object,callback,reference,flags);
    }
    ObjectEnumeration context={callback,reference,object,0,false};
    Log("EnumObjectMetadataBegin device=%p flags=0x%lx callback_rva=0x%08lx max_objects=256 existing_game_call_only=1",
        object,flags,CallerRva(reinterpret_cast<void*>(callback)));
    SetLastError(error);
    const HRESULT result=original(object,&ObserveObject,&context,flags);
    Log("EnumObjectMetadataEnd device=%p observed=%u truncated=%d callback_stopped=%d result=0x%08lx",
        object,context.seen,context.seen>256,context.stopped,static_cast<unsigned long>(result));
    return result;
}

static ULONG STDMETHODCALLTYPE DeviceRelease(void* object) {
    auto release=reinterpret_cast<ULONG (STDMETHODCALLTYPE*)(void*)>(Original(object,2));
    const ULONG remaining=release(object);
    if (!remaining) {
        AcquireSRWLockExclusive(&deviceLock);
        DeviceRecord** cursor=&devices;
        while (*cursor) {
            if ((*cursor)->object==object) {
                auto* record=*cursor; *cursor=record->next; delete record; break;
            }
            cursor=&(*cursor)->next;
        }
        ReleaseSRWLockExclusive(&deviceLock);
    }
    return remaining;
}

static HRESULT STDMETHODCALLTYPE DeviceFormat(void* object,LPCDIDATAFORMAT format) {
    auto original=reinterpret_cast<HRESULT (STDMETHODCALLTYPE*)(void*,LPCDIDATAFORMAT)>(Original(object,11));
    const HRESULT result=original(object,format);
    if (SUCCEEDED(result) && format) {
        AcquireSRWLockExclusive(&deviceLock);
        for (auto* record=devices;record;record=record->next) if (record->object==object) {
            record->bytes=format->dwDataSize; record->flags=format->dwFlags;
            record->stateSamples=0; record->dataSamples=0; break;
        }
        ReleaseSRWLockExclusive(&deviceLock);
        Log("SetDataFormat device=%p bytes=%lu flags=0x%lx objects=%lu object_size=%lu caller_rva=0x%08lx",
            object,format->dwDataSize,format->dwFlags,format->dwNumObjs,format->dwObjSize,CallerRva(_ReturnAddress()));
        if (format->dwDataSize<=sizeof(DIMOUSESTATE2) && format->rgodf && format->dwObjSize==sizeof(DIOBJECTDATAFORMAT)) {
            for (DWORD i=0;i<format->dwNumObjs && i<16;++i) {
                const auto& item=format->rgodf[i];
                Log("FormatObject device=%p offset=%lu type=0x%lx flags=0x%lx",object,item.dwOfs,item.dwType,item.dwFlags);
            }
        }
    }
    return result;
}

static HRESULT STDMETHODCALLTYPE DeviceState(void* object,DWORD size,void* data) {
    auto original=reinterpret_cast<HRESULT (STDMETHODCALLTYPE*)(void*,DWORD,void*)>(Original(object,9));
    const HRESULT result=original(object,size,data);
    if (FAILED(result) || !data || !GameForeground()) return result;
    DeviceKind kind=DeviceKind::Other;
    DWORD bytes=0,flags=0;
    bool sample=false;
    AcquireSRWLockExclusive(&deviceLock);
    for (auto* record=devices;record;record=record->next) if (record->object==object) {
        kind=record->kind; bytes=record->bytes; flags=record->flags;
        bool interesting=record->stateSamples<4;
        if (kind==DeviceKind::Mouse && bytes==size && (size==sizeof(DIMOUSESTATE) || size==sizeof(DIMOUSESTATE2))) {
            auto* mouse=static_cast<const DIMOUSESTATE*>(data);
            interesting=interesting || mouse->lX || mouse->lY || mouse->lZ;
        }
        sample=interesting && record->stateSamples<64;
        if (sample) ++record->stateSamples;
        break;
    }
    ReleaseSRWLockExclusive(&deviceLock);
    if (sample && kind==DeviceKind::Mouse && bytes==size && (size==sizeof(DIMOUSESTATE) || size==sizeof(DIMOUSESTATE2))) {
        auto* mouse=static_cast<const DIMOUSESTATE*>(data);
        unsigned buttons=0;
        for (unsigned i=0;i<(size==sizeof(DIMOUSESTATE2)?8u:4u);++i)
            if (reinterpret_cast<const unsigned char*>(data)[12+i]&0x80) buttons|=1u<<i;
        Log("MouseState device=%p bytes=%lu flags=0x%lx dx=%ld dy=%ld wheel=%ld buttons=0x%x caller_rva=0x%08lx",
            object,size,flags,mouse->lX,mouse->lY,mouse->lZ,buttons,CallerRva(_ReturnAddress()));
    } else if (sample) {
        Log("DeviceState device=%p kind=%u bytes=%lu format_bytes=%lu flags=0x%lx caller_rva=0x%08lx (no key contents recorded)",
            object,static_cast<unsigned>(kind),size,bytes,flags,CallerRva(_ReturnAddress()));
    }
    return result;
}

static HRESULT STDMETHODCALLTYPE DeviceData(void* object,DWORD objectSize,LPDIDEVICEOBJECTDATA data,LPDWORD count,DWORD flags) {
    auto original=reinterpret_cast<HRESULT (STDMETHODCALLTYPE*)(void*,DWORD,LPDIDEVICEOBJECTDATA,LPDWORD,DWORD)>(Original(object,10));
    const HRESULT result=original(object,objectSize,data,count,flags);
    if (SUCCEEDED(result) && count && *count && GameForeground()) {
        bool sample=false;
        AcquireSRWLockExclusive(&deviceLock);
        for (auto* record=devices;record;record=record->next) if (record->object==object) {
            sample=record->dataSamples++<32; break;
        }
        ReleaseSRWLockExclusive(&deviceLock);
        if (sample) Log("BufferedEvents device=%p object_size=%lu count=%lu flags=0x%lx caller_rva=0x%08lx (no event contents recorded)",object,objectSize,*count,flags,CallerRva(_ReturnAddress()));
    }
    return result;
}

static HRESULT STDMETHODCALLTYPE CreateDevice(void* object,REFGUID guid,void** device,IUnknown* outer) {
    auto original=reinterpret_cast<HRESULT (STDMETHODCALLTYPE*)(void*,REFGUID,void**,IUnknown*)>(Original(object,3));
    const HRESULT result=original(object,guid,device,outer);
    Log("CreateDevice guid=%08lx-%04x-%04x result=0x%08lx caller_rva=0x%08lx",guid.Data1,guid.Data2,guid.Data3,static_cast<unsigned long>(result),CallerRva(_ReturnAddress()));
    if (SUCCEEDED(result) && device && *device && !outer) {
        auto* record=new(std::nothrow) DeviceRecord;
        if (!record) return result;
        const DeviceKind kind=IsEqualGUID(guid,GUID_SysMouse)?DeviceKind::Mouse:
            (IsEqualGUID(guid,GUID_SysKeyboard)?DeviceKind::Keyboard:DeviceKind::Other);
        AcquireSRWLockExclusive(&deviceLock);
        for (auto* existing=devices;existing;existing=existing->next) {
            if (existing->object==*device) {
                ReleaseSRWLockExclusive(&deviceLock);
                delete record;
                return result;
            }
        }
        *record={*device,kind,0,0,0,0,false,devices}; devices=record;
        ReleaseSRWLockExclusive(&deviceLock);
        const bool release=Observe(*device,2,reinterpret_cast<void*>(&DeviceRelease));
        const bool format=Observe(*device,11,reinterpret_cast<void*>(&DeviceFormat));
        const bool state=Observe(*device,9,reinterpret_cast<void*>(&DeviceState));
        const bool buffered=Observe(*device,10,reinterpret_cast<void*>(&DeviceData));
        const bool metadata=objectMetadata && Observe(*device,4,reinterpret_cast<void*>(&DeviceObjects));
        Log("Device observation device=%p kind=%u release=%d format=%d state=%d buffered=%d object_metadata=%d",*device,static_cast<unsigned>(kind),release,format,state,buffered,metadata);
    }
    return result;
}

#pragma optimize("y",off)
extern "C" __declspec(noinline) HRESULT WINAPI ProxyInputCreate(HINSTANCE instance,DWORD version,REFIID iid,void** output,IUnknown* outer) {
    Ensure();
    auto create=reinterpret_cast<HRESULT (WINAPI*)(HINSTANCE,DWORD,REFIID,void**,IUnknown*)>(functions[0]);
    const HRESULT result=create(instance,version,iid,output,outer);
    Log("DirectInput8Create version=0x%lx iid=%08lx result=0x%08lx caller_rva=0x%08lx",version,iid.Data1,static_cast<unsigned long>(result),CallerRva(_ReturnAddress()));
    LogStartupStack();
    if (enabled && SUCCEEDED(result)) InstallCameraControls(CallerRva(_ReturnAddress()));
    if (inputObservation && SUCCEEDED(result) && output && *output && !outer &&
        (IsEqualGUID(iid,IID_IDirectInput8A) || IsEqualGUID(iid,IID_IDirectInput8W)))
        Log("Root observation=%d",Observe(*output,3,reinterpret_cast<void*>(&CreateDevice)));
    return result;
}
#pragma optimize("",on)
extern "C" HRESULT WINAPI ProxyCanUnload() {
    Ensure();
    return enabled?S_FALSE:reinterpret_cast<HRESULT (WINAPI*)()>(functions[1])();
}
extern "C" HRESULT WINAPI ProxyClassObject(REFCLSID clsid,REFIID iid,void** output) {
    Ensure(); return reinterpret_cast<HRESULT (WINAPI*)(REFCLSID,REFIID,void**)>(functions[2])(clsid,iid,output);
}
extern "C" HRESULT WINAPI ProxyRegister() { Ensure(); return reinterpret_cast<HRESULT (WINAPI*)()>(functions[3])(); }
extern "C" HRESULT WINAPI ProxyUnregister() { Ensure(); return reinterpret_cast<HRESULT (WINAPI*)()>(functions[4])(); }
extern "C" LPCDIDATAFORMAT WINAPI ProxyJoystickFormat() { Ensure(); return reinterpret_cast<LPCDIDATAFORMAT (WINAPI*)()>(functions[5])(); }
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID) {
    if (reason==DLL_PROCESS_ATTACH) { selfModule=module; DisableThreadLibraryCalls(module); }
    return TRUE;
}
