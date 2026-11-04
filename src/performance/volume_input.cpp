// Game-local DirectInput forwarding, borderless keyboard sharing and optional free orbit.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <bcrypt.h>
#include <dinput.h>
#include "keyboard_sharing_policy.h"
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <new>
#include "native_ui_text_hook.h"
#include "crash_client.inl"
static HMODULE selfModule,systemModule;
static INIT_ONCE initOnce=INIT_ONCE_STATIC_INIT;
static SRWLOCK logLock=SRWLOCK_INIT,tableLock=SRWLOCK_INIT;
static bool enabled=false,diagnostics=false;
static bool stereoSpatialAudioEnabled=false;
static bool keyboardSharing=false;
static bool mouseCursorSharing=false;
static bool interfaceRemakeEnabled=false;
static wchar_t logPath[MAX_PATH]{};
static FARPROC functions[6]{};
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

#include "../shared/executable_identity.h"
#include "stock_native_readiness.h"
static volatile LONG stockIdentity=0;
static bool startupObservation=false;
static volatile LONG nativeConfiguration=0;
static wchar_t configurationPath[MAX_PATH]{};
static SRWLOCK identityRetryLock=SRWLOCK_INIT;
static unsigned identityAttempts=0;
static ULONGLONG identityRetryAfter=0;
static bool RefreshStockIdentity(){
 if(InterlockedCompareExchange(&stockIdentity,0,0))return true;
 if(!TryAcquireSRWLockExclusive(&identityRetryLock))return false;
 const auto now=GetTickCount64();
 if(!InterlockedCompareExchange(&stockIdentity,0,0)&&identityAttempts<32&&now>=identityRetryAfter){
  ++identityAttempts;identityRetryAfter=now+1000;
  const auto report=stock_file_identity::Verify();InterlockedExchange(&stockIdentity,report.matched?1:0);
  Log("Stock file identity attempt=%u matched=%d stage=%u error=%08lx bytes=%llu image=%ls opened=%ls computed_sha256=%s",identityAttempts,report.matched,report.stage,report.error,report.totalBytes,report.imagePath,report.openedPath,report.sha256[0]?report.sha256:"unavailable");
 }
 const bool result=InterlockedCompareExchange(&stockIdentity,0,0)!=0;ReleaseSRWLockExclusive(&identityRetryLock);return result;
}



static bool orbitThrowFacing=false;
#include "free_camera.inl"
#include "inventory_safety.inl"
#define AITD_COMBAT_NATIVE
#include "combat_controls.inl"
#include "target_lock_capture.inl"
#include "fp_mouse_sensitivity.inl"
#include "rope_timing.inl"
#include "cloth_timing.inl"
#include "ballistic_aim.inl"
#include "fiery_trail.inl"
#include "first_person_sight.inl"
#include "pc_keyboard_prompts.inl"
#include "pc_keycap_icons.inl"
#include "cinematic_fast_forward.inl"
#include "audio_output_silence.inl"
#include "audio_spatial.inl"
#include "weapon_hud.inl"
#include "asset_overrides.inl"
#include "favorites_native.inl"
#include "favorites.inl"
#include "menu_context.inl"
#include "startup_skip.inl"
#include "options_context.inl"
#include "options_children_context.inl"
#include "ui_transition_timing.inl"
#include "pause_context.inl"
#include "dialog_native_hook.inl"
#include "ui_sound_native.inl"
#include "phone_context.inl"
#include "phone_parameters_context.inl"
#include "phone_content_context.inl"
#include "controls_context.inl"
#include "main_popup_context.inl"
#include "chapters_context.inl"
#include "loading_context.inl"
#include "general_hud_context.inl"
#include "continuous_episodes.inl"
#include "episode_end_context.inl"
#include "inventory_ui_context.inl"
#include "inventory_popup_context.inl"
#include "interaction_ui_context.inl"
#include "tutorial_ui_context.inl"
#include "qte_ui_context.inl"
#include "healing_ui_context.inl"
#include "bleeding_ui_context.inl"
#include "recap_context.inl"
#include "calibration_context.inl"
#include "phone_portraits.inl"
#include "inventory_ui_images.inl"
#include "interaction_ui_images.inl"
#include "loading_tip_images.inl"
#include "chapters_previews.inl"
#include "chapter_preview_probe.inl"
#include "map_shortcut.inl"
static bool OwnsNativeMainOverlayInput(){return pause_context::ConsumesInput()||recap_context::ConsumesInput()||calibration_context::ConsumesInput();}
static bool OwnsPhoneSubpageInput(){return phone_parameters_context::ConsumesInput()||phone_content_context::ConsumesInput();}
#include "ui_guard_context.inl"
static bool __cdecl OwnsNativeUiPaint(std::uintptr_t root) noexcept {
    return healing_ui_context::OwnsMinimapPaint(root)||ui_guard_context::OwnsPaint(root)||calibration_context::OwnsPaint(root)||recap_context::OwnsPaint(root)||bleeding_ui_context::OwnsPaint(root)||qte_ui_context::OwnsPaint(root)||tutorial_ui_context::OwnsPaint(root)||interaction_ui_context::OwnsPaint(root)||general_hud_context::OwnsPaint(root)||inventory_popup_context::OwnsPaint(root)||inventory_ui_context::OwnsPaint(root)||episode_end_context::OwnsPaint(root)||loading_context::OwnsPaint(root)||chapters_context::OwnsPaint(root)||main_popup_context::OwnsPaint(root)||controls_context::OwnsPaint(root)||options_context::OwnsPaint(root)||options_children_context::OwnsPaint(root)||pause_context::OwnsPaint(root)||dialog_native_hook::OwnsPaint(root)||phone_context::OwnsPaint(root)||phone_parameters_context::OwnsPaint(root)||phone_content_context::OwnsPaint(root);
}
static bool __cdecl OwnsNativeUiContent(std::uintptr_t root,std::uintptr_t shape)noexcept{
    return recap_context::OwnsVideo(root,shape)||phone_content_context::OwnsMapContent(root,shape);
}
static bool __cdecl OwnsNativeUiInstance(std::uintptr_t root,std::uintptr_t shape,std::uintptr_t instance)noexcept{
    return inventory_ui_context::OwnsNativeEquipment(root,shape,instance)||inventory_popup_context::OwnsNativeAnimation(root,shape,instance);
}
static bool __cdecl TransformNativeUiContent(std::uintptr_t root,std::uintptr_t shape,std::uintptr_t instance,float* matrix)noexcept{
    return inventory_ui_context::TransformNativeEquipment(root,shape,instance,matrix);
}
static void __cdecl OwnsNativePhonePointer()noexcept{
    __try{
        UiGuardStateV1 guard{};const bool globalPointer=ui_guard_context::GetState(&guard,sizeof guard)&&(guard.flags&UiGuardPointer);
        if(!globalPointer&&!inventory_popup_context::OwnsPointer()&&!inventory_ui_context::OwnsPointer()&&!episode_end_context::OwnsPointer()&&!chapters_context::OwnsPointer()&&!main_popup_context::OwnsPointer()&&!controls_context::OwnsPointer()){
            const auto movie=*reinterpret_cast<unsigned char**>(0xff4790);if(!movie)return;
            const auto handle=*reinterpret_cast<unsigned char**>(movie+0x24);if(!handle)return;
            const auto root=*reinterpret_cast<DWORD*>(handle);
            if(!phone_context::OwnsPaint(root)&&!phone_parameters_context::OwnsPaint(root)&&!phone_content_context::OwnsPaint(root))return;
        }
        auto* general=*reinterpret_cast<unsigned char**>(0xff43ac);DWORD node{};
        if(general&&phone_context::Resolve(general,"/mouse",129,node))*reinterpret_cast<DWORD*>(node+0x6c)&=~1u;
    }__except(EXCEPTION_EXECUTE_HANDLER){}
}
extern "C" BOOL WINAPI AITDGetUiGuardState(void* out,DWORD bytes){return ui_guard_context::GetState(out,bytes);}
extern "C" BOOL WINAPI AITDSetUiGuardRenderer(BOOL value,DWORD version){return ui_guard_context::SetAvailable(value,version);}
extern "C" BOOL WINAPI AITDGetMainPopupState(void* out,DWORD bytes){return main_popup_context::GetState(out,bytes);}
extern "C" BOOL WINAPI AITDQueueMainPopupResponse(DWORD response,DWORD generation){return main_popup_context::Queue(response,generation);}
extern "C" void WINAPI AITDSetMainPopupReady(BOOL ready,DWORD generation){main_popup_context::SetReady(ready,generation);}
extern "C" LONG WINAPI AITDConsumeControlsWheel(DWORD generation){return controls_context::TakeWheel(generation);}
extern "C" BOOL WINAPI AITDGetBleedingUiState(void* out,DWORD bytes){return bleeding_ui_context::GetState(out,bytes);}
extern "C" void WINAPI AITDSetBleedingUiReady(BOOL value,DWORD epoch){bleeding_ui_context::SetReady(value,epoch);}
extern "C" BOOL WINAPI AITDGetHealingUiState(void* out,DWORD bytes){return healing_ui_context::GetState(out,bytes);}
extern "C" BOOL WINAPI AITDGetQteUiState(void* output,DWORD bytes){return qte_ui_context::GetState(output,bytes);}
extern "C" void WINAPI AITDSetQteUiReady(BOOL value,DWORD generation){qte_ui_context::SetReady(value,generation);}
extern "C" BOOL WINAPI AITDGetTutorialUiState(void* output,DWORD bytes){return tutorial_ui_context::GetState(output,bytes);}
extern "C" void WINAPI AITDSetTutorialUiReady(BOOL value,DWORD generation){tutorial_ui_context::SetReady(value,generation);}
extern "C" BOOL WINAPI AITDGetInteractionUiState(void* output,DWORD bytes){return interaction_ui_context::GetState(output,bytes);}
extern "C" void WINAPI AITDSetInteractionUiReady(BOOL value,DWORD generation){interaction_ui_context::SetReady(value,generation);}
extern "C" BOOL WINAPI AITDGetGeneralHudState(void* output,DWORD bytes){return general_hud_context::GetState(output,bytes);}
extern "C" void WINAPI AITDSetGeneralHudReady(BOOL value,DWORD generation){general_hud_context::SetReady(value,generation);}
extern "C" BOOL WINAPI AITDGetInventoryPopupState(void* output,DWORD bytes){return inventory_popup_context::GetState(output,bytes);}
extern "C" BOOL WINAPI AITDQueueInventoryPopupAction(DWORD action,DWORD row,DWORD generation){return inventory_popup_context::Queue(action,row,generation);}
extern "C" void WINAPI AITDSetInventoryPopupReady(BOOL value,DWORD generation){inventory_popup_context::SetReady(value,generation);}
extern "C" HRESULT WINAPI AITDAcquireInventoryImage(DWORD slot,DWORD epoch,IDirect3DTexture9** output){return inventory_ui_images::Acquire(slot,epoch,output);}
extern "C" HRESULT WINAPI AITDAcquireInteractionImage(DWORD slot,DWORD epoch,IDirect3DTexture9** output){return interaction_ui_images::Acquire(slot,epoch,output);}
extern "C" BOOL WINAPI AITDGetInventoryUiState(void* output,DWORD bytes){return inventory_ui_context::GetState(output,bytes);}
extern "C" void WINAPI AITDSetInventoryUiReady(BOOL value,DWORD generation){inventory_ui_context::SetReady(value,generation);}
extern "C" BOOL WINAPI AITDGetEpisodeEndState(void* output,DWORD bytes){return episode_end_context::GetState(output,bytes);}
extern "C" BOOL WINAPI AITDQueueEpisodeEndAction(DWORD action,DWORD row,DWORD generation){return episode_end_context::Queue(action,row,generation);}
extern "C" void WINAPI AITDSetEpisodeEndReady(BOOL value,DWORD generation){episode_end_context::SetReady(value,generation);}
extern "C" BOOL WINAPI AITDGetLoadingState(void* output,DWORD bytes){return loading_context::GetState(output,bytes);}
extern "C" void WINAPI AITDSetLoadingReady(BOOL value,DWORD generation){loading_context::SetReady(value,generation);}
extern "C" BOOL WINAPI AITDGetChaptersState(void* output,DWORD bytes){return chapters_context::GetState(output,bytes);}
extern "C" BOOL WINAPI AITDQueueChaptersAction(DWORD action,LONG row,DWORD generation){return chapters_context::Queue(action,row,generation);}
extern "C" void WINAPI AITDSetChaptersReady(BOOL ready,DWORD generation){chapters_context::SetReady(ready,generation);}
extern "C" HRESULT WINAPI AITDAcquireChapterPreview(DWORD slot,DWORD generation,IDirect3DTexture9** output){return chapters_previews::Acquire(slot,generation,output);}
extern "C" BOOL WINAPI AITDGetControlsState(void* out,DWORD bytes){return controls_context::GetState(out,bytes);}
extern "C" BOOL WINAPI AITDQueueControlsAction(DWORD action,LONG row,double value,DWORD generation){return controls_context::Queue(action,row,value,generation);}
extern "C" void WINAPI AITDSetControlsReady(BOOL ready,DWORD generation){controls_context::SetReady(ready,generation);}
extern "C" BOOL WINAPI AITDGetPhoneContentState(void* output,DWORD bytes){return phone_content_context::GetState(output,bytes);}
extern "C" BOOL WINAPI AITDQueuePhoneContentAction(DWORD action,DWORD row,DWORD generation){return phone_content_context::Queue(action,row,generation);}
extern "C" void WINAPI AITDSetPhoneContentReady(BOOL ready,DWORD generation){phone_content_context::SetReady(ready,generation);}
extern "C" LONG WINAPI AITDConsumePhoneWheel(DWORD generation){return phone_content_context::TakeWheel(generation);}
extern "C" BOOL WINAPI AITDGetPhonePageState(void* output,DWORD bytes){return phone_parameters_context::GetState(output,bytes);}
extern "C" BOOL WINAPI AITDQueuePhonePageAction(DWORD action,LONG row,double value,DWORD generation){return phone_parameters_context::Queue(action,row,value,generation);}
extern "C" void WINAPI AITDSetPhonePagePresentationReady(BOOL ready,DWORD generation){phone_parameters_context::SetPresentationReady(ready,generation);}
extern "C" BOOL WINAPI AITDGetPhoneState(void* output,DWORD bytes){return phone_context::GetState(output,bytes);}
extern "C" BOOL WINAPI AITDQueuePhoneAction(DWORD action,DWORD generation){return phone_context::Queue(action,generation);}
extern "C" void WINAPI AITDSetPhonePresentationReady(BOOL ready,DWORD generation){phone_context::SetPresentationReady(ready,generation);}
extern "C" HRESULT WINAPI AITDAcquirePhonePortrait(DWORD row,DWORD generation,IDirect3DTexture9** output){return phone_portraits::Acquire(row,generation,output);}
extern "C" HRESULT WINAPI AITDAcquireInformationImage(DWORD generation,IDirect3DTexture9** output){return loading_tip_images::Acquire(generation,output);}
extern "C" BOOL WINAPI AITDGetCalibrationState(void* output,DWORD bytes){return calibration_context::GetState(output,bytes);}
extern "C" BOOL WINAPI AITDQueueCalibrationAction(DWORD action,DWORD row,double normalized,DWORD generation){return calibration_context::Queue(action,row,normalized,generation);}
extern "C" void WINAPI AITDSetCalibrationReady(BOOL ready,DWORD generation){calibration_context::SetReady(ready,generation);}
extern "C" BOOL WINAPI AITDGetRecapState(void* output,DWORD bytes){return recap_context::GetState(output,bytes);}
extern "C" BOOL WINAPI AITDQueueRecapAction(DWORD action,DWORD generation){return recap_context::Queue(action,generation);}
extern "C" void WINAPI AITDSetRecapReady(BOOL ready,DWORD generation){recap_context::SetReady(ready,generation);}
extern "C" BOOL WINAPI AITDGetPauseState(void* output,DWORD bytes){return pause_context::GetState(output,bytes);}
extern "C" BOOL WINAPI AITDQueuePauseAction(DWORD slot,DWORD generation){return pause_context::Queue(slot,generation);}
extern "C" void WINAPI AITDSetPausePresentationReady(BOOL ready,DWORD generation){pause_context::SetPresentationReady(ready,generation);}
extern "C" BOOL WINAPI AITDGetOptionsChildrenState(void* output,DWORD bytes){return options_children_context::GetState(output,bytes);}
extern "C" BOOL WINAPI AITDQueueOptionsChildrenAction(DWORD action,LONG row,double normalized,DWORD generation){return options_children_context::Queue(action,row,normalized,generation);}
extern "C" void WINAPI AITDSetOptionsChildrenPresentationReady(BOOL ready,DWORD generation){options_children_context::SetPresentationReady(ready,generation);}
extern "C" BOOL WINAPI AITDGetOptionsState(void* output,DWORD bytes){return options_context::GetState(output,bytes);}
extern "C" BOOL WINAPI AITDQueueOptionsAction(DWORD action,DWORD generation){return options_context::Queue(action,generation);}
extern "C" void WINAPI AITDSetOptionsPresentationReady(BOOL ready,DWORD generation){options_context::SetPresentationReady(ready,generation);}
extern "C" BOOL WINAPI AITDGetMenuState(void* output,DWORD bytes){return menu_context::GetState(output,bytes);}
extern "C" BOOL WINAPI AITDQueueMenuAction(DWORD action){return menu_context::Queue(action);}
extern "C" BOOL WINAPI AITDSetMenuPresentationReady(BOOL ready){
    if(!menu_context::enabled)return FALSE;
    menu_context::SetPresentationReady(ready);return TRUE;
}
static const char* __cdecl FlashlightTutorialPrompt(const char* command,const char* output,std::size_t remaining){
    if(!pc_prompts::modernControls||!pc_prompts::Equal(command,"ACTION_ENTER"))return nullptr;
    //54D750 passes its own512-byte manager+42 output buffer to54A8A0.
    // Do not relabel another interaction merely because this tip is visible.
    const auto address=reinterpret_cast<ULONG_PTR>(output);
    constexpr ULONG_PTR begin=0xff4528+0x42,end=begin+0x200;
    if(address<begin||address>=end||!remaining||remaining>end-address)return nullptr;
    __try{
        // Native54D750 formats manager+30 through599340 on every Tips update.
        // Authenticate that exact movie/key, independently of equipped items:
        // the tutorial can outlive the brief equipment transition.
        if(*reinterpret_cast<DWORD*>(0xff4530)!=0xc792dc)return nullptr;
        const auto phase=*reinterpret_cast<DWORD*>(0xff4548);
        auto* movie=*reinterpret_cast<BYTE**>(0xff4528);
        if(!phase||phase>3||!movie||*reinterpret_cast<DWORD*>(movie+12)!=0xff4530||!movie[0x65])return nullptr;
        const auto message=*reinterpret_cast<const unsigned long long*>(0xff4558);
        return pc_prompts::TutorialLabel(command,message,true);
    }__except(EXCEPTION_EXECUTE_HANDLER){return nullptr;}
}
static const char* __cdecl ModernControlPrompt(const char* command){
    using namespace modern_combat;
    const DWORD now=GetTickCount();
    if(interactionActive&&now-interactionTick<=100&&
        (pc_prompts::Equal(command,"ACTION_ENTER")||pc_prompts::Equal(command,"ACTION_LEAVE")))return "F";
    if(!actionRoutingActive||!actionControls.active||now-actionControls.lastTick>100)return nullptr;
    if(pc_prompts::Equal(command,"OBJECT_HANDLE")&&
        (actionControls.equipment==Equipment::Melee||actionControls.equipment==Equipment::PistolThrowable||
        actionControls.equipment==Equipment::PistolFlashlight||actionControls.equipment==Equipment::Throwable))return "E";
    if(pc_prompts::Equal(command,"OBJECT_LAUNCH")){
        if(actionControls.equipment==Equipment::Melee)return "RMB";
        if(actionControls.equipment==Equipment::PistolThrowable||actionControls.equipment==Equipment::Throwable)return "E";
    }
    if((pc_prompts::Equal(command,"OBJECT_USE_LEFT")||pc_prompts::Equal(command,"LEFT_OBJECT_USE"))&&
       (actionControls.equipment==Equipment::PistolFlashlight||actionControls.equipment==Equipment::Flashlight))return "Q";
    if((pc_prompts::Equal(command,"OBJECT_USE_RIGHT")||pc_prompts::Equal(command,"RIGHT_OBJECT_USE"))&&actionControls.equipment==Equipment::Flashlight)return "Q";
    return nullptr;
}

static BOOL CALLBACK Initialize(PINIT_ONCE,void*,void**);
static void ConfigureNativeFeatures(const wchar_t* config){
    wchar_t path[MAX_PATH]{};
    const bool general=ConfigFlag(L"General",L"Enabled",true,config);
    diagnostics=general&&ConfigFlag(L"Diagnostics",L"Enabled",false,config);
    const DWORD envLength=GetEnvironmentVariableW(L"LOCALAPPDATA",path,MAX_PATH);
    if(envLength && envLength+75<MAX_PATH){
        wcscat_s(path,L"\\AITD2008Enhanced");CreateDirectoryW(path,nullptr);
        wcscat_s(path,L"\\logs");CreateDirectoryW(path,nullptr);
        swprintf_s(logPath,L"%s\\input-enhanced-%lu.log",path,GetCurrentProcessId());
    }
    const bool supported=true;
    rope_reference_rate::requested=general&&supported&&ConfigFlag(L"Compatibility",L"FixHighFramerateRopes",true,config);
    cloth_rigidity_rate::requested=general&&supported&&ConfigFlag(L"Compatibility",L"FixHighFramerateCloth",true,config);
    keyboardSharing=general&&supported&&ConfigFlag(L"Display",L"BorderlessFullscreen",false,config)&&
        ConfigFlag(L"Display",L"WindowsVolumeKeys",true,config);
    mouseCursorSharing=general&&supported&&ConfigFlag(L"Display",L"BorderlessFullscreen",false,config);
    ConfigureFreeCamera(config,general&&supported);
    fixed_camera::Configure(config,general&&supported&&orbitSettingsValid);
    ConfigureCinematicFastForward(config,general&&supported);
    continuous_episodes::Configure(config,general&&supported);
    modern_combat::Configure(config,general&&supported);
    modern_combat::targetCapture=&target_lock_capture::Capture;
    ConfigureInventorySafety(config,general&&supported);
    weapon_hud::Configure(config,general&&supported);
    ConfigureBallisticAim(config,general&&supported);
    ConfigureFieryTrail(config,general&&supported);
    interfaceRemakeEnabled=general&&supported&&ConfigFlag(L"UI",L"RemakeInterface",false,config);
    ui_sound_native::enabled=general&&supported&&ConfigFlag(L"UI",L"ModernSounds",false,config);
    options_context::Configure(interfaceRemakeEnabled);
    pause_context::Configure(interfaceRemakeEnabled);
    recap_context::Configure(interfaceRemakeEnabled&&ConfigFlag(L"UI",L"RemakeRecap",false,config));
    calibration_context::Configure(interfaceRemakeEnabled&&ConfigFlag(L"UI",L"RemakeCalibration",false,config));
    options_children_context::Configure(interfaceRemakeEnabled);
    phone_context::Configure(interfaceRemakeEnabled&&ConfigFlag(L"UI",L"RemakePhone",false,config));
    phone_parameters_context::Configure(interfaceRemakeEnabled&&ConfigFlag(L"UI",L"RemakePhone",false,config));
    phone_portraits::enabled=interfaceRemakeEnabled&&ConfigFlag(L"UI",L"RemakePhone",false,config);
    phone_content_context::Configure(interfaceRemakeEnabled&&ConfigFlag(L"UI",L"RemakePhone",false,config));
    controls_context::Configure(interfaceRemakeEnabled&&ConfigFlag(L"UI",L"RemakeControls",false,config));
    main_popup_context::Configure(interfaceRemakeEnabled);
    chapters_context::presenterEnabled=interfaceRemakeEnabled&&ConfigFlag(L"UI",L"RemakeEpisodes",false,config);
    chapter_preview_probe::enabled=interfaceRemakeEnabled&&diagnostics&&ConfigFlag(L"Diagnostics",L"ChapterContent",false,config);
    chapters_context::Configure(chapters_context::presenterEnabled||chapter_preview_probe::enabled);
    chapters_previews::enabled=chapters_context::presenterEnabled;
    chapter_preview_probe::consumer=chapters_previews::enabled?chapters_previews::Observe:nullptr;
    tutorial_ui_context::Configure(interfaceRemakeEnabled&&ConfigFlag(L"UI",L"RemakeTutorials",false,config));
    qte_ui_context::Configure(interfaceRemakeEnabled&&ConfigFlag(L"UI",L"RemakeQte",false,config));
    healing_ui_context::Configure(interfaceRemakeEnabled&&ConfigFlag(L"UI",L"RemakeHealing",false,config));
    bleeding_ui_context::Configure(interfaceRemakeEnabled&&ConfigFlag(L"UI",L"RemakeBleeding",false,config));
    interaction_ui_context::Configure(interfaceRemakeEnabled&&ConfigFlag(L"UI",L"RemakeInteraction",false,config));
    general_hud_context::Configure(interfaceRemakeEnabled&&ConfigFlag(L"UI",L"RemakeGeneralHud",false,config));
    inventory_popup_context::Configure(interfaceRemakeEnabled&&ConfigFlag(L"UI",L"RemakeInventoryPopups",false,config));
    inventory_ui_context::Configure(interfaceRemakeEnabled&&ConfigFlag(L"UI",L"RemakeInventory",false,config));
    episode_end_context::Configure(interfaceRemakeEnabled&&ConfigFlag(L"UI",L"RemakeEpisodeEnd",false,config),ConfigFlag(L"Gameplay",L"ContinuousEpisodes",false,config));
    loading_context::Configure(interfaceRemakeEnabled&&ConfigFlag(L"UI",L"RemakeLoading",false,config));
    phone_context::subpageInputOwned=OwnsPhoneSubpageInput;
    dialog_native_hook::Configure(general&&supported,interfaceRemakeEnabled,0);
    fp_mouse::Configure(config,general&&supported);
    first_person_sight::enabled=general&&supported&&modern_combat::enabled;
    ConfigurePcKeyboardPrompts(config,general&&supported);
    ConfigurePcKeycapIcons(config,general&&supported);
    pc_prompts::contextLabelProvider=&ModernControlPrompt;
    pc_prompts::bufferLabelProvider=&FlashlightTutorialPrompt;
    asset_overrides::Configure(config,general&&supported);
    favorites::Configure(config,general&&supported);
    menu_context::Configure(general&&supported&&ConfigFlag(L"UI",L"RemakeMenu",false,config));
    startup_skip::Configure(general&&supported&&ConfigFlag(L"Startup",L"SkipIntro",false,config));
    // Ensure also runs at the renderer's explicit early bootstrap. Install the
    // startup-only requests there, before waiting for the first input device.
    startup_skip::Install();
    ui_guard_context::Configure(interfaceRemakeEnabled);
    favorites_native::enabled=favorites::enabled;
    favorites_native::uiActiveProvider=&favorites::Active;
    modern_combat::externalInputBlocked=&favorites::ConsumesInput;
    modern_combat::vehicleKeyFocus=&interaction_ui_context::VehicleKeyFocus;
    modern_combat::externalActionRoute=&favorites::ActionRoute;
    ConfigureStereoSpatialAudio(config,general&&supported);
    stereoSpatialAudioEnabled=StereoSpatialAudioRequested();
    enabled=continuous_episodes::requested||rope_reference_rate::requested||cloth_rigidity_rate::requested||stereoSpatialAudioEnabled||favorites::enabled||keyboardSharing||mouseCursorSharing||freeThirdPerson||fixed_camera::available||fp_mouse::enabled||companionFollowNormally||cinematic_ff::enabled||
        modern_combat::enabled||inventory_safety::enabled||weapon_hud::enabled||
        ballistic_aim::enabled||pc_prompts::enabled||asset_overrides::enabled||menu_context::enabled||startup_skip::Enabled()||interfaceRemakeEnabled;
    Log("input_component=0.5.9 supported_sha256=%d borderless_keyboard_sharing=%d free_orbit=%d normal_follow=%d",supported,keyboardSharing,freeThirdPerson,companionFollowNormally);
    if(enabled){
        HMODULE pinned=nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&Initialize),&pinned);
    }
}
static void TryConfigureNativeFeatures(){
    if(InterlockedCompareExchange(&nativeConfiguration,0,0)!=0)return;
    if(!RefreshStockIdentity())return;
    if(InterlockedCompareExchange(&nativeConfiguration,1,0)!=0)return;
    static ULONGLONG retryAfter=0;
    const auto now=GetTickCount64();
    if(now<retryAfter){InterlockedExchange(&nativeConfiguration,0);return;}
    retryAfter=now+250;
    const auto readiness=stock_native_readiness::Check();
    if(readiness!=stock_native_readiness::Result::Ready){
        static unsigned messages=0;if(++messages<=8)Log("Stock native readiness=%u; normal protected startup retained, hooks deferred",static_cast<unsigned>(readiness));
        InterlockedExchange(&nativeConfiguration,0);return;
    }
    ConfigureNativeFeatures(configurationPath);
    InterlockedExchange(&nativeConfiguration,2);
    Log("Stock native readiness=0; feature configuration admitted; per-feature signatures still required");
}

static BOOL CALLBACK Initialize(PINIT_ONCE,void*,void**) {
    crash_client::Initialize();
    wchar_t path[MAX_PATH]{};
    const UINT length=GetSystemDirectoryW(path,MAX_PATH);
    if(!length || length+14>=MAX_PATH)return FALSE;
    wcscat_s(path,L"\\dinput8.dll");systemModule=LoadLibraryW(path);
    if(!systemModule)return FALSE;
    const char* names[]={"DirectInput8Create","DllCanUnloadNow","DllGetClassObject","DllRegisterServer","DllUnregisterServer","GetdfDIJoystick"};
    for(unsigned n=0;n<6;++n){functions[n]=GetProcAddress(systemModule,names[n]);if(!functions[n])return FALSE;}
    wchar_t config[MAX_PATH]{};
    if(!GetModuleFileNameW(selfModule,config,MAX_PATH))return TRUE;
    wchar_t* slash=wcsrchr(config,L'\\');if(!slash)return TRUE;
    wcscpy_s(slash,MAX_PATH-static_cast<size_t>(slash-config),L"\\aitd-enhanced.ini");
    wcscpy_s(configurationPath,config);

    const bool general=ConfigFlag(L"General",L"Enabled",true,config);
    diagnostics=general&&ConfigFlag(L"Diagnostics",L"Enabled",false,config);
    const DWORD envLength=GetEnvironmentVariableW(L"LOCALAPPDATA",path,MAX_PATH);
    if(envLength && envLength+75<MAX_PATH){
        wcscat_s(path,L"\\AITD2008Enhanced");CreateDirectoryW(path,nullptr);
        wcscat_s(path,L"\\logs");CreateDirectoryW(path,nullptr);
        swprintf_s(logPath,L"%s\\input-enhanced-%lu.log",path,GetCurrentProcessId());
    }
    if(general){
        HMODULE pinned=nullptr;
        startupObservation=GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&Initialize),&pinned)!=FALSE;
    }
    Log("Stock enhancement candidate; exact file identity and normal native initialization required; no activation or protection changes");
    TryConfigureNativeFeatures();
    return TRUE;
}
static void Ensure(){if(!InitOnceExecuteOnce(&initOnce,Initialize,nullptr,nullptr))RaiseFailFastException(nullptr,nullptr,0);}
static void InstallConfiguredNativeHooks(){
    if(!enabled||InterlockedCompareExchange(&nativeConfiguration,0,0)!=2)return;
        if(asset_overrides::enabled&&!asset_overrides::originalOpen)Log("Asset override native archive hook=%d",asset_overrides::Install());
        startup_skip::Install();continuous_episodes::Install();InstallFreeCamera();fixed_camera::Install();InstallCinematicFastForward();modern_combat::Install();
        InstallBallisticAim();InstallPcKeyboardPrompts();InstallPcKeycapIcons();
        InstallFieryTrail();
        first_person_sight::Install();
        fp_mouse::Install();
        rope_reference_rate::Install();
        cloth_rigidity_rate::Install();
        favorites_native::Install();
        menu_context::Install();
        if(chapter_preview_probe::enabled||chapter_preview_probe::consumer)Log("Chapter preview observer install=%d",chapter_preview_probe::Install());
        InstallStereoSpatialAudio(stereoSpatialAudioEnabled);
        InstallCinematicAudioOutputSilence(cinematic_ff::enabled);InstallInventorySafety();
}
// Explicit renderer startup bridge; runs outside either DllMain, before native audio creation.
extern "C" DWORD WINAPI AITDInitializeAudioStartup(){
 Ensure();TryConfigureNativeFeatures();InstallConfiguredNativeHooks();InstallStereoSpatialAudio(stereoSpatialAudioEnabled);
 return (stereoSpatialAudioEnabled?1u:0u)|(InterlockedCompareExchange(&nativeConfiguration,0,0)==2?2u:0u);
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
    if ((!enabled&&!startupObservation) || !object) return false;
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


static bool favoritesCursorActive(){return favorites::Active()||menu_context::ConsumesInput()||inventory_popup_context::ConsumesInput()||episode_end_context::ConsumesInput()||OwnsNativeMainOverlayInput()||chapters_context::ConsumesInput()||main_popup_context::ConsumesInput()||controls_context::ConsumesInput()||options_context::ConsumesInput()||options_children_context::ConsumesInput()||dialog_native_hook::ConsumesInput()||phone_context::ConsumesInput()||phone_parameters_context::ConsumesInput()||phone_content_context::ConsumesInput();}
#include "mouse_cursor_sharing.inl"
#include "input_recovery.inl"
static HRESULT STDMETHODCALLTYPE KeyboardCooperative(void* object,HWND window,DWORD flags) {
    favorites::RegisterWindow(window);
    auto original=reinterpret_cast<HRESULT (STDMETHODCALLTYPE*)(void*,HWND,DWORD)>(Original(object,13));
    // GetCapabilities is layout-identical for A/W and verifies the live device kind.
    // Do not track object addresses: COM may reuse them for another device later.
    auto** table=*reinterpret_cast<void***>(object);
    auto getCaps=reinterpret_cast<HRESULT (STDMETHODCALLTYPE*)(void*,LPDIDEVCAPS)>(table[3]);
    DIDEVCAPS caps{};caps.dwSize=sizeof(caps);
    const HRESULT capsResult=getCaps(object,&caps);
    static_assert(DISCL_EXCLUSIVE==1&&DISCL_NONEXCLUSIVE==2&&DISCL_NOWINKEY==16,"DirectInput cooperative flag ABI");
    DWORD adjusted=SharedKeyboardFlags(keyboardSharing,SUCCEEDED(capsResult)&&
        GET_DIDEVICE_TYPE(caps.dwDevType)==DI8DEVTYPE_KEYBOARD,flags);
    const bool mouse=mouseCursorSharing&&SUCCEEDED(capsResult)&&GET_DIDEVICE_TYPE(caps.dwDevType)==DI8DEVTYPE_MOUSE;
    if(mouse)adjusted=mouse_cursor::Flags(mouse_cursor::Visible(),flags);
    HRESULT result=original(object,window,adjusted);
    if(FAILED(result)&&adjusted!=flags){
        Log("Keyboard sharing rejected result=0x%08lx; retaining original mode",static_cast<unsigned long>(result));
        result=original(object,window,flags);
        adjusted=flags;
    }
    Log("SetCooperativeLevel device_type=%lu caps_result=0x%08lx requested=0x%lx effective=0x%lx result=0x%08lx",
        caps.dwDevType,static_cast<unsigned long>(capsResult),flags,adjusted,static_cast<unsigned long>(result));
    if(mouse&&SUCCEEDED(result)){mouse_cursor::enabled=true;mouse_cursor::Remember(object,window,flags,adjusted);}
    return result;
}
static HRESULT STDMETHODCALLTYPE CameraMouseState(void* object,DWORD bytes,void* data){
    TryConfigureNativeFeatures();InstallConfiguredNativeHooks();
    if(InterlockedCompareExchange(&nativeConfiguration,0,0)!=2){
        const auto forward=reinterpret_cast<HRESULT (STDMETHODCALLTYPE*)(void*,DWORD,void*)>(Original(object,9));
        return forward(object,bytes,data);
    }
    cloth_rigidity_rate::Report();
    TickStereoSpatialAudio();
    favorites::Pulse();
    if(bytes==sizeof(DIMOUSESTATE)||bytes==sizeof(DIMOUSESTATE2))mouse_cursor::Update(object);
    auto original=reinterpret_cast<HRESULT (STDMETHODCALLTYPE*)(void*,DWORD,void*)>(Original(object,9));
    HRESULT result=original(object,bytes,data);
    result=input_recovery::Retry(object,bytes,data,result,original,(favorites::enabled||menu_context::enabled||interfaceRemakeEnabled)&&favorites::Focused());
    DWORD observedKind=0;
    if((favorites::enabled||menu_context::enabled||interfaceRemakeEnabled)&&SUCCEEDED(result)&&data&&(bytes==256||bytes==sizeof(DIMOUSESTATE)||bytes==sizeof(DIMOUSESTATE2))){
        auto** table=*reinterpret_cast<void***>(object);
        auto capsMethod=reinterpret_cast<HRESULT (STDMETHODCALLTYPE*)(void*,LPDIDEVCAPS)>(table[3]);
        DIDEVCAPS caps{};caps.dwSize=sizeof(caps);
        if(SUCCEEDED(capsMethod(object,&caps))){const auto kind=GET_DIDEVICE_TYPE(caps.dwDevType);
            observedKind=kind;
            if(bytes==256&&kind==DI8DEVTYPE_KEYBOARD&&ui_sound_native::enabled)ui_sound_native::Install();
            if(bytes==256&&kind==DI8DEVTYPE_KEYBOARD&&menu_context::enabled){
                ui_transition_timing::Tick();
                static unsigned timingReported=0;
                if(timingReported!=ui_transition_timing::advances){timingReported=ui_transition_timing::advances;Log("Native UI return timing advances=%u sprite=%u frame=%u target=%u",timingReported,ui_transition_timing::lastSprite,ui_transition_timing::lastFrom,ui_transition_timing::lastTarget);}
                menu_context::Tick();
                static DWORD startupGuardReported=~0u;
                if(!menu_context::enteredMenu&&(menu_context::state.flags&MenuAuthenticated)){
                    const DWORD stamp=menu_context::state.flags|(menu_context::state.nativeState<<8);
                    if(stamp!=startupGuardReported){startupGuardReported=stamp;Log("Startup UI guard flags=%lu state=%lu progress=%lu warning=%u",menu_context::state.flags,menu_context::state.nativeState,*reinterpret_cast<DWORD*>(0xff4390),static_cast<unsigned>(*reinterpret_cast<BYTE*>(0xff437c)));}
                }
            }
            if(bytes==256&&kind==DI8DEVTYPE_KEYBOARD&&interfaceRemakeEnabled){
                options_context::Tick();
                pause_context::Tick();
                recap_context::Tick();
                calibration_context::Tick();
                options_children_context::Tick();
                phone_parameters_context::Tick();
                phone_portraits::Tick();
                phone_content_context::Tick();
                controls_context::Tick();
                main_popup_context::Tick();
                tutorial_ui_context::Tick();
                qte_ui_context::Tick();
                healing_ui_context::Tick();
                bleeding_ui_context::Tick();
                interaction_ui_context::Tick();
                interaction_ui_images::Tick();
                general_hud_context::Tick();
                inventory_popup_context::Tick();
                inventory_ui_context::Tick();
                inventory_ui_images::Tick();
                episode_end_context::Tick();
                loading_context::Tick();
                loading_tip_images::Tick();
                chapters_context::Tick();
                chapters_previews::Tick();
                phone_context::Tick();
                map_shortcut::Capture(data);
                ui_guard_context::Tick();
                dialog_native_hook::InitializeOnUiThreadTick();
                static bool textAttempted=false;
                if(!textAttempted){textAttempted=true;native_ui_text_hook::SetPaintOwner(OwnsNativeUiPaint);native_ui_text_hook::SetContentPaint(OwnsNativeUiContent);native_ui_text_hook::SetContentInstancePaint(OwnsNativeUiInstance);native_ui_text_hook::SetContentTransform(TransformNativeUiContent);native_ui_text_hook::SetPointerOwner(OwnsNativePhonePointer);Log("Native vector UI text install=%d",native_ui_text_hook::Install(true));}
                static DWORD paintLogTick=0;static unsigned priorPaint=0;
                const auto paint=native_ui_text_hook::GetPaintStatistics();const unsigned totalPaint=paint.staticText+paint.dynamicText+paint.shapes;
                if(totalPaint!=priorPaint&&GetTickCount()-paintLogTick>=2000){paintLogTick=GetTickCount();priorPaint=totalPaint;Log("Native owned UI paint static=%u dynamic=%u shapes=%u",paint.staticText,paint.dynamicText,paint.shapes);}
            }
            if(bytes!=256&&kind==DI8DEVTYPE_MOUSE&&interfaceRemakeEnabled){auto* mouse=static_cast<DIMOUSESTATE*>(data);if(controls_context::CaptureWheel(mouse->lZ)||phone_content_context::CaptureWheel(mouse->lZ))mouse->lZ=0;}
            if((bytes==256&&kind==DI8DEVTYPE_KEYBOARD)||(bytes!=256&&kind==DI8DEVTYPE_MOUSE))favorites::Capture(data,bytes,kind==DI8DEVTYPE_KEYBOARD);
        }
    }
    if(SUCCEEDED(result)&&data&&(bytes==256||bytes==sizeof(DIMOUSESTATE)||bytes==sizeof(DIMOUSESTATE2)))weapon_hud::Capture();
    if(SUCCEEDED(result)&&data&&bytes==256&&fixed_camera::available){
        auto** table=*reinterpret_cast<void***>(object);
        auto capsMethod=reinterpret_cast<HRESULT (STDMETHODCALLTYPE*)(void*,LPDIDEVCAPS)>(table[3]);
        DIDEVCAPS caps{};caps.dwSize=sizeof(caps);
        if(SUCCEEDED(capsMethod(object,&caps))&&GET_DIDEVICE_TYPE(caps.dwDevType)==DI8DEVTYPE_KEYBOARD)fixed_camera::Keyboard(data);
    }
    if(SUCCEEDED(result)&&data&&bytes==256&&cinematic_ff::enabled){
        auto** table=*reinterpret_cast<void***>(object);
        auto capsMethod=reinterpret_cast<HRESULT (STDMETHODCALLTYPE*)(void*,LPDIDEVCAPS)>(table[3]);
        DIDEVCAPS caps{};caps.dwSize=sizeof(caps);
        if(SUCCEEDED(capsMethod(object,&caps))&&GET_DIDEVICE_TYPE(caps.dwDevType)==DI8DEVTYPE_KEYBOARD)
            FilterFastForwardKeyboard(data,bytes);
    }
    if((freeThirdPerson||fixed_camera::available||fp_mouse::enabled)&&SUCCEEDED(result)&&data&&(bytes==sizeof(DIMOUSESTATE)||bytes==sizeof(DIMOUSESTATE2))){
        auto** table=*reinterpret_cast<void***>(object);
        auto capsMethod=reinterpret_cast<HRESULT (STDMETHODCALLTYPE*)(void*,LPDIDEVCAPS)>(table[3]);
        DIDEVCAPS caps{};caps.dwSize=sizeof(caps);
        if(SUCCEEDED(capsMethod(object,&caps))&&GET_DIDEVICE_TYPE(caps.dwDevType)==DI8DEVTYPE_MOUSE){
            fp_mouse::Capture(data,bytes);
            if(freeThirdPerson||fixed_camera::available)CaptureOrbitMouse(data,bytes);
        }
    }
    if(SUCCEEDED(result)&&data&&(menu_context::ConsumesInput()||inventory_popup_context::ConsumesInput()||episode_end_context::ConsumesInput()||OwnsNativeMainOverlayInput()||chapters_context::ConsumesInput()||main_popup_context::ConsumesInput()||controls_context::ConsumesInput()||options_context::ConsumesInput()||options_children_context::ConsumesInput()||dialog_native_hook::ConsumesInput()||phone_context::ConsumesInput()||phone_parameters_context::ConsumesInput()||phone_content_context::ConsumesInput())&&
       ((bytes==256&&observedKind==DI8DEVTYPE_KEYBOARD)||
        ((bytes==sizeof(DIMOUSESTATE)||bytes==sizeof(DIMOUSESTATE2))&&observedKind==DI8DEVTYPE_MOUSE)))memset(data,0,bytes);
    return result;
}
static HRESULT STDMETHODCALLTYPE CreateDevice(void* object,REFGUID guid,void** device,IUnknown* outer) {
    auto original=reinterpret_cast<HRESULT (STDMETHODCALLTYPE*)(void*,REFGUID,void**,IUnknown*)>(Original(object,3));
    const HRESULT result=original(object,guid,device,outer);
    if(SUCCEEDED(result)) {TryConfigureNativeFeatures();InstallConfiguredNativeHooks();}
    if(SUCCEEDED(result)&&device&&*device&&!outer){
        Log("CooperativeLevel observer=%d",Observe(*device,13,reinterpret_cast<void*>(&KeyboardCooperative)));
        if(InterlockedCompareExchange(&nativeConfiguration,0,0)!=2||(cloth_rigidity_rate::requested&&diagnostics)||stereoSpatialAudioEnabled||favorites::enabled||menu_context::enabled||interfaceRemakeEnabled||mouseCursorSharing||freeThirdPerson||fixed_camera::available||fp_mouse::enabled||cinematic_ff::enabled||weapon_hud::enabled)Log("Camera/cinematic input observer=%d",Observe(*device,9,reinterpret_cast<void*>(&CameraMouseState)));
    }
    return result;
}
extern "C" HRESULT WINAPI ProxyInputCreate(HINSTANCE instance,DWORD version,REFIID iid,void** output,IUnknown* outer) {
    Ensure();auto create=reinterpret_cast<HRESULT (WINAPI*)(HINSTANCE,DWORD,REFIID,void**,IUnknown*)>(functions[0]);
    const HRESULT result=create(instance,version,iid,output,outer);
    if(SUCCEEDED(result))TryConfigureNativeFeatures();
    if(SUCCEEDED(result))InstallConfiguredNativeHooks();
    if(SUCCEEDED(result)&&output&&*output&&!outer&&
        (IsEqualGUID(iid,IID_IDirectInput8A)||IsEqualGUID(iid,IID_IDirectInput8W)))
        Log("DirectInput8 root observer=%d",Observe(*output,3,reinterpret_cast<void*>(&CreateDevice)));
    return result;
}
extern "C" HRESULT WINAPI ProxyCanUnload(){Ensure();return (enabled||startupObservation)?S_FALSE:reinterpret_cast<HRESULT (WINAPI*)()>(functions[1])();}
extern "C" HRESULT WINAPI ProxyClassObject(REFCLSID clsid,REFIID iid,void** output){Ensure();return reinterpret_cast<HRESULT (WINAPI*)(REFCLSID,REFIID,void**)>(functions[2])(clsid,iid,output);}
extern "C" HRESULT WINAPI ProxyRegister(){Ensure();return reinterpret_cast<HRESULT (WINAPI*)()>(functions[3])();}
extern "C" HRESULT WINAPI ProxyUnregister(){Ensure();return reinterpret_cast<HRESULT (WINAPI*)()>(functions[4])();}
extern "C" LPCDIDATAFORMAT WINAPI ProxyJoystickFormat(){Ensure();return reinterpret_cast<LPCDIDATAFORMAT (WINAPI*)()>(functions[5])();}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){selfModule=module;DisableThreadLibraryCalls(module);}return TRUE;}
