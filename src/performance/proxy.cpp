// x86 enhancement candidate for the exact original protected Steam executable.
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
#include "native_lod.h"

static HMODULE selfModule;
static HMODULE systemD3D;
static INIT_ONCE initOnce = INIT_ONCE_STATIC_INIT;
static SRWLOCK logLock = SRWLOCK_INIT;
static SRWLOCK objectLock = SRWLOCK_INIT;
static bool diagnostics = false;
static bool GammaEnabled = false;
static bool ffOverlayEnabled=false;
static bool aimOverlayEnabled=false;
static bool ammoOverlayEnabled=false;
static bool favoritesOverlayEnabled=false;
static bool menuOverlayEnabled=false;
static bool displaySettingsFix = false;
static SRWLOCK patchLock = SRWLOCK_INIT;
static bool displayPatchApplied = false;
static volatile LONG stockIdentity=0;
static bool rendererGeneral=false;
static volatile LONG nativeRenderConfiguration=0;
static volatile LONG portableConfiguration=0;
static wchar_t rendererConfigurationPath[MAX_PATH]{};
static IDirect3DDevice9* nativeConfiguredDevice=nullptr;
static bool Observe(void*,unsigned,void*);
static void TryConfigureNativeRenderer();
static wchar_t logPath[MAX_PATH];
extern "C" { void* exports[23] = {}; }
static void ConfigureShadowResourceProbe(const wchar_t*,bool);
static void ConfigureNativeShadowQuality(const wchar_t*,bool);
static bool NativeShadowQualityEnabled();
static void ConfigureSubsurfaceScattering(const wchar_t*,bool);
static bool SubsurfaceObservationEnabled();
static void ConfigureRemakeMenu(const wchar_t*,bool);
static void ConfigureRemakeInterface(const wchar_t*,bool);
static void ConfigureModernUiSound(const wchar_t*,bool);

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

static bool InitializeInputAudioEarly(bool supportedAndEnabled){
    if(!supportedAndEnabled)return false;
    static HMODULE pairedInput=nullptr;
    if(!pairedInput){
        wchar_t path[MAX_PATH]{};const DWORD size=GetModuleFileNameW(selfModule,path,MAX_PATH);
        if(!size||size>=MAX_PATH)return false;wchar_t* slash=wcsrchr(path,L'\\');if(!slash)return false;
        if(wcscpy_s(slash,MAX_PATH-static_cast<size_t>(slash-path),L"\\dinput8.dll"))return false;
        HMODULE input=LoadLibraryW(path);if(!input){Log("Paired input load failed error=%lu",GetLastError());return false;}
        wchar_t loaded[MAX_PATH]{};const DWORD actual=GetModuleFileNameW(input,loaded,MAX_PATH);
        if(!actual||actual>=MAX_PATH||_wcsicmp(path,loaded)){Log("Paired input path mismatch; native startup retained");FreeLibrary(input);return false;}
        pairedInput=input;
    }
    const auto initialize=reinterpret_cast<DWORD(WINAPI*)()>(GetProcAddress(pairedInput,"AITDInitializeAudioStartup"));
    if(!initialize)return false;
    const DWORD state=initialize();
    static DWORD previous=~0u;if(state!=previous){previous=state;Log("Paired input startup state=%lu native_ready=%d stereo_requested=%d",state,(state&2)!=0,(state&1)!=0);}
    return (state&2)!=0;
}

static void RefreshNativeLod(){
    native_lod::Tick();
    const auto state=native_lod::GetStatus();
    static bool known=false;static float quality=1,distance=1;
    static native_lod::Reason reason=native_lod::Reason::Vanilla;
    if(!known||quality!=state.appliedQuality||distance!=state.appliedDrawDistance||reason!=state.reason){
        known=true;quality=state.appliedQuality;distance=state.appliedDrawDistance;reason=state.reason;
        Log("LOD installed=%d quality=%.3f distance=%.3f reason=%s availableVA=%llu",state.installed,quality,distance,native_lod::ReasonName(reason),state.availableVirtual);
    }
}

#include "../diagnostics/frame_pacing.inl"
#include "telemetry.inl"
#include "borderless.inl"

#include "../shared/executable_identity.h"
#include "stock_native_readiness.h"
#include "stock_native_frame_wait.inl"
static SRWLOCK rendererIdentityLock=SRWLOCK_INIT;
static unsigned rendererIdentityAttempts=0;
static ULONGLONG rendererIdentityRetryAfter=0;
static bool RefreshStockIdentity(){
 if(InterlockedCompareExchange(&stockIdentity,0,0))return true;
 if(!TryAcquireSRWLockExclusive(&rendererIdentityLock))return false;
 const auto now=GetTickCount64();
 if(!InterlockedCompareExchange(&stockIdentity,0,0)&&rendererIdentityAttempts<32&&now>=rendererIdentityRetryAfter){
  ++rendererIdentityAttempts;rendererIdentityRetryAfter=now+1000;
  const auto report=stock_file_identity::Verify();InterlockedExchange(&stockIdentity,report.matched?1:0);
  Log("Stock file identity attempt=%u matched=%d stage=%u error=%08lx bytes=%llu image=%ls opened=%ls computed_sha256=%s",rendererIdentityAttempts,report.matched,report.stage,report.error,report.totalBytes,report.imagePath,report.openedPath,report.sha256[0]?report.sha256:"unavailable");
 }
 const bool result=InterlockedCompareExchange(&stockIdentity,0,0)!=0;ReleaseSRWLockExclusive(&rendererIdentityLock);return result;
}
static void TryConfigureNativeRenderer(){
 if(InterlockedCompareExchange(&nativeRenderConfiguration,0,0)!=0)return;
 if(!RefreshStockIdentity())return;
 // Load our paired proxy while both independently see the exact original file.
 // Native patches still wait for normal game initialization in both modules.
 static volatile LONG pairedPreload=0;
 if(InterlockedCompareExchange(&pairedPreload,1,0)==0)InitializeInputAudioEarly(true);
 if(InterlockedCompareExchange(&portableConfiguration,1,0)==0){
  GammaEnabled=rendererGeneral&&ConfigFlag(L"Performance",L"CacheGamma",true,rendererConfigurationPath);
  borderlessFullscreen=rendererGeneral&&ConfigFlag(L"Display",L"BorderlessFullscreen",false,rendererConfigurationPath);
  if(rendererGeneral)InitializeFramePacing(rendererConfigurationPath);
 }
 if(!rendererGeneral){InterlockedExchange(&nativeRenderConfiguration,2);return;}
 if(InterlockedCompareExchange(&nativeRenderConfiguration,1,0)!=0)return;
 static ULONGLONG retryAfter=0;const auto now=GetTickCount64();
 if(now<retryAfter){InterlockedExchange(&nativeRenderConfiguration,0);return;}
 retryAfter=now+250;
 const auto readiness=stock_native_readiness::Check();
 if(readiness!=stock_native_readiness::Result::Ready){
  static unsigned messages=0;if(++messages<=8)Log("Stock renderer native readiness=%u; portable forwarding retained, native configuration deferred",static_cast<unsigned>(readiness));
  InterlockedExchange(&nativeRenderConfiguration,0);return;
 }
 // Configure the paired input before the wait detour changes its admission anchor.
 // Its existing export explicitly confirms native readiness; a generic E9 never does.
 if(!InitializeInputAudioEarly(true)){InterlockedExchange(&nativeRenderConfiguration,0);return;}
 const wchar_t* config=rendererConfigurationPath;
 ffOverlayEnabled=ConfigFlag(L"Cutscenes",L"FastForward",true,config);
 aimOverlayEnabled=ConfigFlag(L"Controls",L"ModernCombat",true,config);
 ammoOverlayEnabled=ConfigFlag(L"HUD",L"Ammunition",true,config);
 favoritesOverlayEnabled=ConfigFlag(L"Controls",L"Favorites",true,config);
 displaySettingsFix=ConfigFlag(L"Compatibility",L"FixDisplaySettingsCrash",true,config);
 ConfigureShadowResourceProbe(config,true);ConfigureNativeShadowQuality(config,true);
 ConfigureRemakeMenu(config,true);ConfigureRemakeInterface(config,true);
 ConfigureModernUiSound(config,true);ConfigureSubsurfaceScattering(config,true);
 native_lod::Configure(config);native_lod::InstallAuthenticated(true);RefreshNativeLod();
 const bool wait=stock_native_frame_wait::Install();
 InterlockedExchange(&nativeRenderConfiguration,2);
 Log("Stock renderer native configuration admitted=1 default120_wait=%d; exact feature signatures retained",wait);
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
    wcscpy_s(rendererConfigurationPath,config);
    rendererGeneral=ConfigFlag(L"General",L"Enabled",true,config);
    diagnostics=rendererGeneral&&ConfigFlag(L"Diagnostics",L"Enabled",false,config);
    DWORD envLength=GetEnvironmentVariableW(L"LOCALAPPDATA",path,MAX_PATH);
    if (envLength && envLength+65 < MAX_PATH) {
        wcscat_s(path,L"\\AITD2008Enhanced");
        CreateDirectoryW(path,nullptr);
        wcscat_s(path,L"\\logs");
        CreateDirectoryW(path,nullptr);
        swprintf_s(logPath,L"%s\\d3d9-%lu.log",path,GetCurrentProcessId());
    }
    Log("Stock renderer candidate loaded; exact file identity and normal native initialization required");
    TryConfigureNativeRenderer();
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

#include "gamma_cache.inl"
#include "ff_overlay.inl"
#include "hud_presentation.inl"
#include "menu_overlay.inl"
#include "ui_sound_backend.inl"
static void ConfigureModernUiSound(const wchar_t* config,bool supported){ui_sound_backend::Configure(config,supported);}
#include "dialog_overlay.inl"
#include "options_overlay.inl"
#include "options_children_overlay.inl"
#include "pause_overlay.inl"
#include "phone_overlay.inl"
#include "phone_pages_overlay.inl"
#include "phone_content_overlay.inl"
#include "controls_overlay.inl"
#include "main_popup_overlay.inl"
#include "chapters_overlay.inl"
#include "loading_overlay.inl"
#include "episode_end_overlay.inl"
#include "inventory_ui_overlay.inl"
#include "inventory_popup_overlay.inl"
#include "general_hud_overlay.inl"
#include "interaction_ui_overlay.inl"
#include "tutorial_ui_overlay.inl"
#include "qte_ui_overlay.inl"
#include "healing_ui_overlay.inl"
#include "ui_guard_overlay.inl"
#include "bleeding_ui_overlay.inl"
#include "recap_overlay.inl"
#include "calibration_overlay.inl"
#include "phone_backdrop.inl"
static void ConfigureRemakeMenu(const wchar_t* config,bool supported){menu_overlay::Configure(config,supported);menuOverlayEnabled=menu_overlay::enabled;}
#include "ui_geometry_budget.inl"
static void ConfigureRemakeInterface(const wchar_t* config,bool supported){dialog_overlay::Configure(config,supported);options_overlay::Configure(config,supported);hud_presentation::enabled=dialog_overlay::enabled;ui_geometry_budget::Install(dialog_overlay::enabled);}
#include "shadow_resource_probe.inl"
#include "native_shadow_quality.inl"
#include "target_lock_overlay.inl"
#include "sss_pipeline.inl"
static bool NativeShadowQualityEnabled(){return native_shadows::enabled;}

static void LogParameters(const char* operation,const D3DPRESENT_PARAMETERS* p) {
    if (!p) { Log("%s parameters=null",operation); return; }
    Log("%s width=%u height=%u format=%u count=%u windowed=%d refresh=%u interval=%u swap=%u hwnd=%p",
        operation,p->BackBufferWidth,p->BackBufferHeight,p->BackBufferFormat,p->BackBufferCount,
        p->Windowed,p->FullScreen_RefreshRateInHz,p->PresentationInterval,p->SwapEffect,p->hDeviceWindow);
}

static void EnsureDeviceNativeHooks(IDirect3DDevice9* object,bool beforeNativePools){
 if(!object)return;
 D3DDEVICE_CREATION_PARAMETERS creation{};if(FAILED(object->GetCreationParameters(&creation)))return;
 if(GammaEnabled&&!Original(object,21)){
  const HWND window=GetAncestor(creation.hFocusWindow,GA_ROOT);
  Log("Deferred gamma observer=%d hook=%d",GammaRegister(object,window),Observe(object,21,reinterpret_cast<void*>(&ObservedGamma)));
 }
 if(InterlockedCompareExchange(&nativeRenderConfiguration,0,0)!=2||!rendererGeneral||nativeConfiguredDevice==object)return;
 if(creation.DeviceType!=D3DDEVTYPE_HAL||(creation.BehaviorFlags&D3DCREATE_ADAPTERGROUP_DEVICE))return;
 if(SubsurfaceObservationEnabled()){
  sss_pipeline::device=object;
  const bool hooks=Observe(object,81,reinterpret_cast<void*>(&ObservedSssPrimitive))&&
   Observe(object,82,reinterpret_cast<void*>(&ObservedSssIndexed))&&
   Observe(object,83,reinterpret_cast<void*>(&ObservedSssPrimitiveUP))&&
   Observe(object,84,reinterpret_cast<void*>(&ObservedSssIndexedUP));
  Log("Deferred SSS native draw observation=%d",hooks);
 }
 if(beforeNativePools)InstallNativeShadowQuality(object);
 else if(NativeShadowQualityEnabled())Log("Native shadow size installation deferred: device predates native admission; next device recreation required");
 if(shadow_probe::enabled){Observe(object,23,reinterpret_cast<void*>(&ObservedShadowTexture));Observe(object,28,reinterpret_cast<void*>(&ObservedShadowColorSurface));Observe(object,29,reinterpret_cast<void*>(&ObservedShadowDepthSurface));}
 FfRegister(object);nativeConfiguredDevice=object;
}

static HRESULT STDMETHODCALLTYPE ObservedReset(IDirect3DDevice9* object,D3DPRESENT_PARAMETERS* parameters) {
    target_lock_overlay::Invalidate();
    sss_pipeline::Invalidate(object);
    phone_backdrop::Invalidate(object);
    auto reset=reinterpret_cast<HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,D3DPRESENT_PARAMETERS*)>(Original(object,16));
    GammaInvalidate(object);
    FfInvalidate(object);
    hud_presentation::Invalidate(object);
    menu_overlay::Invalidate(object);
    dialog_overlay::Invalidate(object);
    options_overlay::Invalidate(object);pause_overlay::Invalidate(object);recap_overlay::Invalidate(object);calibration_overlay::Invalidate(object);phone_overlay::Invalidate(object);
    phone_pages_overlay::Invalidate(object);phone_content_overlay::Invalidate(object);controls_overlay::Invalidate(object);main_popup_overlay::Invalidate(object);chapters_overlay::Invalidate(object);loading_overlay::Invalidate(object);episode_end_overlay::Invalidate(object);inventory_ui_overlay::Invalidate(object);inventory_popup_overlay::Invalidate(object);general_hud_overlay::Invalidate(object);interaction_ui_overlay::Invalidate(object);tutorial_ui_overlay::Invalidate(object);qte_ui_overlay::Invalidate(object);healing_ui_overlay::Invalidate(object);bleeding_ui_overlay::Invalidate(object);
    options_children_overlay::Invalidate(object);
    ResetFrameDevice(object,true);
    LogParameters("Reset requested",parameters);
    BorderlessChange borderlessChange{};
    BorderlessPrepare(parameters,borderlessWindow,borderlessChange);
    const HRESULT result=reset(object,parameters);
    if(FAILED(result))BorderlessRollback(borderlessChange,parameters);
    GammaInvalidate(object);
    if(SUCCEEDED(result))FfRegister(object);
    if (SUCCEEDED(result) && GammaEnabled && parameters && parameters->hDeviceWindow)
        GammaRegister(object,GetAncestor(parameters->hDeviceWindow,GA_ROOT));
    ResetFrameDevice(object,FAILED(result),SUCCEEDED(result) && parameters ? parameters->hDeviceWindow : nullptr);
    Log("Reset result=0x%08lx",static_cast<unsigned long>(result));
    LogParameters("Reset returned",parameters);
    return result;
}

static HRESULT STDMETHODCALLTYPE ObservedPresent(IDirect3DDevice9* object,const RECT* source,
    const RECT* destination,HWND window,const RGNDATA* dirty) {
    TryConfigureNativeRenderer();EnsureDeviceNativeHooks(object,false);
    auto present=reinterpret_cast<HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,const RECT*,const RECT*,HWND,const RGNDATA*)>(Original(object,17));
    PaceFrame(object,window);
    if(InterlockedCompareExchange(&nativeRenderConfiguration,0,0)!=2){
        LARGE_INTEGER before{},after{};if(diagnostics)QueryPerformanceCounter(&before);
        const HRESULT result=present(object,source,destination,window,dirty);
        if(diagnostics){QueryPerformanceCounter(&after);PerfRecord(object,before.QuadPart,after.QuadPart);}
        return result;
    }
    RefreshNativeLod();
    sss_pipeline::BeginPresent(object);
    ui_sound_backend::Refresh();
    ui_guard_overlay::Begin();
    phone_backdrop::Draw(object,source,destination);
    hud_presentation::DrawFavoritesBackdrop(object,source,destination);
    target_lock_overlay::Draw(object,source,destination);
    inventory_ui_overlay::Draw(object,source,destination);
    if(!DrawThemedGameOverlay(object,source,destination))DrawFastForwardOverlay(object,source,destination);
    menu_overlay::Draw(object,source,destination);
    pause_overlay::Draw(object,source,destination);
    recap_overlay::Draw(object,source,destination);
    options_overlay::Draw(object,source,destination);
    options_children_overlay::Draw(object,source,destination);
    calibration_overlay::Draw(object,source,destination);
    dialog_overlay::Draw(object,source,destination);
    phone_overlay::Draw(object,source,destination);
    phone_pages_overlay::Draw(object,source,destination);
    phone_content_overlay::Draw(object,source,destination);
    controls_overlay::Draw(object,source,destination);
    chapters_overlay::Draw(object,source,destination);
    main_popup_overlay::Draw(object,source,destination);
    tutorial_ui_overlay::Draw(object,source,destination);
    qte_ui_overlay::Draw(object,source,destination);
    healing_ui_overlay::Draw(object,source,destination);
    bleeding_ui_overlay::Draw(object,source,destination);
    interaction_ui_overlay::Draw(object,source,destination);
    general_hud_overlay::Draw(object,source,destination);
    inventory_popup_overlay::Draw(object,source,destination);
    episode_end_overlay::Draw(object,source,destination);
    loading_overlay::Draw(object,source,destination);
    ui_guard_overlay::Draw(object,source,destination);
    LARGE_INTEGER before{},after{};if(diagnostics)QueryPerformanceCounter(&before);
    sss_pipeline::EndPresent();
    const HRESULT result=present(object,source,destination,window,dirty);
    if(diagnostics){QueryPerformanceCounter(&after);PerfRecord(object,before.QuadPart,after.QuadPart);}
    if (FAILED(result)) { GammaInvalidate(object);FfInvalidate(object);menu_overlay::Invalidate(object);chapters_overlay::Invalidate(object);loading_overlay::Invalidate(object);episode_end_overlay::Invalidate(object);inventory_ui_overlay::Invalidate(object);inventory_popup_overlay::Invalidate(object);general_hud_overlay::Invalidate(object);interaction_ui_overlay::Invalidate(object);tutorial_ui_overlay::Invalidate(object);qte_ui_overlay::Invalidate(object);healing_ui_overlay::Invalidate(object);bleeding_ui_overlay::Invalidate(object); }
    if (FAILED(result)) ResetFrameDevice(object,result==D3DERR_DEVICELOST || result==D3DERR_DEVICEREMOVED);
    return result;
}

static ULONG STDMETHODCALLTYPE ObservedRelease(IDirect3DDevice9* object) {
    menu_overlay::Invalidate(object);
    auto release=reinterpret_cast<ULONG (STDMETHODCALLTYPE*)(IDirect3DDevice9*)>(Original(object,2));
    const ULONGLONG generation=FrameDeviceGeneration(object);
    const ULONGLONG gammaGeneration=GammaGeneration(object);
    const ULONGLONG ffGeneration=FfGeneration(object);
    const ULONG references=release(object);
    if (!references) { if(nativeConfiguredDevice==object)nativeConfiguredDevice=nullptr;hud_presentation::Invalidate(object);dialog_overlay::Invalidate(object);options_overlay::Invalidate(object);pause_overlay::Invalidate(object);recap_overlay::Invalidate(object);calibration_overlay::Invalidate(object);phone_overlay::Invalidate(object);phone_pages_overlay::Invalidate(object);phone_content_overlay::Invalidate(object);controls_overlay::Invalidate(object);main_popup_overlay::Invalidate(object);chapters_overlay::Invalidate(object);loading_overlay::Invalidate(object);episode_end_overlay::Invalidate(object);inventory_ui_overlay::Invalidate(object);inventory_popup_overlay::Invalidate(object);general_hud_overlay::Invalidate(object);interaction_ui_overlay::Invalidate(object);tutorial_ui_overlay::Invalidate(object);qte_ui_overlay::Invalidate(object);healing_ui_overlay::Invalidate(object);bleeding_ui_overlay::Invalidate(object);options_children_overlay::Invalidate(object);ForgetFrameDevice(object,generation);GammaForget(object,gammaGeneration);FfForget(object,ffGeneration); } // Only compare cached identities; never dereference destroyed/reused objects.
    return references;
}

static bool Observe(void* object,unsigned index,void* method) {
    if (!object || !(rendererGeneral || diagnostics || displaySettingsFix || frameLimit || GammaEnabled || borderlessFullscreen || ffOverlayEnabled || aimOverlayEnabled || ammoOverlayEnabled || favoritesOverlayEnabled || menuOverlayEnabled || ui_sound_backend::enabled || NativeShadowQualityEnabled() || SubsurfaceObservationEnabled())) return false;
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
    TryConfigureNativeRenderer();
    ApplyDisplaySettingsFix();
    Log("CreateDevice adapter=%u type=%u behavior=0x%08lx",adapter,type,behavior);
    if(diagnostics){
        D3DADAPTER_IDENTIFIER9 identifier{};
        if(SUCCEEDED(object->GetAdapterIdentifier(adapter,0,&identifier)))
            Log("Graphics adapter description=%.*s vendor=%08lx device=%08lx",511,identifier.Description,identifier.VendorId,identifier.DeviceId);
    }
    LogParameters("CreateDevice requested",parameters);
    BorderlessChange borderlessChange{};
    if(type==D3DDEVTYPE_HAL && !(behavior&D3DCREATE_ADAPTERGROUP_DEVICE))
        BorderlessPrepare(parameters,window,borderlessChange);
    const HRESULT result=create(object,adapter,type,window,behavior,parameters,device);
    if(FAILED(result))BorderlessRollback(borderlessChange,parameters);
    else if(borderlessChange.active)borderlessWindow=borderlessChange.window;
    Log("CreateDevice result=0x%08lx",static_cast<unsigned long>(result));
    LogParameters("CreateDevice returned",parameters);
    if (SUCCEEDED(result) && device && *device) {
        EnsureDeviceNativeHooks(*device,true);
        if (rendererGeneral || diagnostics || frameLimit || GammaEnabled || borderlessFullscreen || ffOverlayEnabled || aimOverlayEnabled || ammoOverlayEnabled || favoritesOverlayEnabled || menuOverlayEnabled) {
            const bool resetHook=Observe(*device,16,reinterpret_cast<void*>(&ObservedReset));
            if (GammaEnabled) {
                const HWND gammaWindow=GetAncestor(parameters && parameters->hDeviceWindow ? parameters->hDeviceWindow : window,GA_ROOT);
                Log("Gamma observer=%d hook=%d",GammaRegister(*device,gammaWindow),Observe(*device,21,reinterpret_cast<void*>(&ObservedGamma)));
            }
            Log("Device observation=%d",resetHook);
            if ((rendererGeneral || frameLimit || diagnostics || GammaEnabled || borderlessFullscreen || ffOverlayEnabled || aimOverlayEnabled || ammoOverlayEnabled || favoritesOverlayEnabled) && type==D3DDEVTYPE_HAL && !(behavior & D3DCREATE_ADAPTERGROUP_DEVICE) && resetHook &&
                Observe(*device,2,reinterpret_cast<void*>(&ObservedRelease)) &&
                Observe(*device,17,reinterpret_cast<void*>(&ObservedPresent))) {
                FfRegister(*device);
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
    EnsureExports();TryConfigureNativeRenderer();
    auto create=reinterpret_cast<IDirect3D9* (WINAPI*)(UINT)>(exports[21]);
    IDirect3D9* object=create(sdk);
    Log("Direct3DCreate9 sdk=%u result=%p",sdk,object);
    if (object && (rendererGeneral || diagnostics || displaySettingsFix || frameLimit || GammaEnabled || borderlessFullscreen || ffOverlayEnabled || aimOverlayEnabled || ammoOverlayEnabled || favoritesOverlayEnabled || menuOverlayEnabled || NativeShadowQualityEnabled())) {
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
