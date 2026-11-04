// Exact-build camera action adapters. No global mouse-state transformation.
static bool cameraRequested=false,cameraInvert=false;
static float cameraGainX=1.0f,cameraGainY=1.0f;
static void* cameraActionOriginal=nullptr;
static const DWORD cameraSites[]={0x1be55,0x1be6a,0x1be7f,0x1be94};
static volatile LONG cameraInstallAttempt=0,cameraSamples=0,cameraCalls=0;

static bool CameraNumber(const wchar_t* key,float* output,const wchar_t* path) {
    wchar_t text[64],*end=nullptr;
    GetPrivateProfileStringW(L"Camera",key,L"1.0",text,64,path);
    const double value=wcstod(text,&end);
    if (end==text || *end || !std::isfinite(value) || value<0.1 || value>4.0) return false;
    *output=static_cast<float>(value);
    return true;
}

static void ConfigureCamera(const wchar_t* path) {
    cameraRequested=ConfigFlag(L"General",L"Enabled",true,path) && ConfigFlag(L"Camera",L"Enabled",false,path);
    if (!cameraRequested) return;
    cameraInvert=ConfigFlag(L"Camera",L"InvertY",false,path);
    if (!CameraNumber(L"SensitivityX",&cameraGainX,path) || !CameraNumber(L"SensitivityY",&cameraGainY,path)) {
        cameraRequested=false;
    }
}

static bool CameraMouseBindings(const unsigned char* controller) {
    // Native configuration is independent of the disk serialization.
    // Restrict to four simple standard mouse bindings; preserve other devices.
    __try {
        const unsigned char* config=*reinterpret_cast<const unsigned char* const*>(controller+0x14);
        if (!config || *reinterpret_cast<const int*>(config)!=115 || *reinterpret_cast<const int*>(config+4)!=115) return false;
        const unsigned char* actions=*reinterpret_cast<const unsigned char* const*>(config+8);
        if (!actions) return false;
        const GUID mouse={0x6f1d2b60,0xd5a0,0x11cf,{0xbf,0xc7,0x44,0x45,0x53,0x54,0,0}};
        for (unsigned index=47;index<=50;++index) {
            const unsigned char* action=actions+index*0x20c;
            if (*reinterpret_cast<const int*>(action)!=0 || *reinterpret_cast<const int*>(action+12)!=1) return false;
            const unsigned char* binding=*reinterpret_cast<const unsigned char* const*>(action+16);
            if (!binding || memcmp(binding,"MOUSE\0",6) || memcmp(binding+0x44,&mouse,sizeof(mouse))) return false;
        }
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}

struct CameraActionSample { float gain; DWORD action,source; BOOL mouse; };
static_assert(sizeof(CameraActionSample)==16,"Adapter frame layout");

static void __cdecl PrepareCameraAction(const unsigned char* controller,void** argument,void* returnAddress,CameraActionSample* sample) {
    const DWORD error=GetLastError();
    sample->gain=1.0f; sample->action=0; sample->source=0; sample->mouse=FALSE;
    const auto* base=reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));
    const DWORD rva=static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(returnAddress)-reinterpret_cast<ULONG_PTR>(base));
    for (unsigned i=0;i<4;++i) if (rva==cameraSites[i]+5) sample->action=47+i;
    sample->source=sample->action;
    if (sample->action && CameraMouseBindings(controller)) {
        __try {
            const DWORD first=*reinterpret_cast<const DWORD*>(controller+8);
            auto* states=*reinterpret_cast<unsigned char* const*>(base+0xe13570);
            if (first<65536 && states && *argument==states+(first+sample->action)*0x3c) {
                sample->mouse=TRUE;
                sample->gain=sample->action<=48?cameraGainY:cameraGainX;
                if (cameraInvert && sample->action<=48) {
                    sample->source=sample->action==47?48:47;
                    *argument=states+(first+sample->source)*0x3c;
                }
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) { sample->mouse=FALSE; sample->gain=1.0f; }
    }
    SetLastError(error);
}

static void __cdecl FinishCameraAction(const CameraActionSample* sample,float* value) {
    const DWORD error=GetLastError();
    const float before=*value;
    if (sample->mouse && std::isfinite(before)) *value=before*sample->gain;
    const LONG call=InterlockedIncrement(&cameraCalls);
    if ((call<=4 || before!=0.0f) && InterlockedIncrement(&cameraSamples)<=64) {
        Log("CameraAction call=%ld action=%lu source_action=%lu mouse_binding=%d before=%.7g after=%.7g gain=%.7g invert_y=%d before_native_filter_and_accumulator=1",
            call,sample->action,sample->source,sample->mouse,before,*value,sample->gain,cameraInvert);
    }
    SetLastError(error);
}

// The original callee pops one pointer and returns its scalar in XMM0.
// Preserve register/flags/x87/SSE results, changing only that scalar result.
// Input metadata uses a separate 16-byte frame above the aligned FXSAVE area.
static __declspec(naked) void CameraActionAdapter() {
    __asm {
        push ebp
        mov ebp,esp
        sub esp,16
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        lea eax,[ebp-16]
        push eax
        push dword ptr [ebp+4]
        lea eax,[ebp+8]
        push eax
        push ebx
        call PrepareCameraAction
        add esp,16
        fxrstor [esp]
        lea esp,[ebp-52]
        popad
        popfd
        push dword ptr [ebp+8]
        call dword ptr [cameraActionOriginal]
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        lea eax,[esp+160]
        push eax
        lea eax,[ebp-16]
        push eax
        call FinishCameraAction
        add esp,8
        fxrstor [esp]
        lea esp,[ebp-52]
        popad
        popfd
        mov esp,ebp
        pop ebp
        ret 4
    }
}

static void InstallCameraControls(DWORD callerRva) {
    if (!cameraRequested || callerRva!=0x575904 || InterlockedCompareExchange(&cameraInstallAttempt,1,0)) return;
    auto* base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    const unsigned char expectedTarget[]={0x53,0x55,0x8b,0x6c,0x24,0x0c,0x8b,0x45,0x14};
    bool matches=memcmp(base+0x576920,expectedTarget,sizeof(expectedTarget))==0;
    for (DWORD rva:cameraSites) {
        const auto* site=base+rva;
        LONG displacement=0; memcpy(&displacement,site+1,4);
        if (site[-1]!=0x50 || site[0]!=0xe8 || site+5+displacement!=base+0x576920) matches=false;
    }
    if (!matches) {
        Log("CameraControls installed=0 reason=runtime_signature_mismatch");
        return;
    }
    cameraActionOriginal=base+0x576920;
    auto* begin=base+cameraSites[0];
    const SIZE_T span=cameraSites[3]+5-cameraSites[0];
    DWORD oldProtection=0,ignored=0;
    if (!VirtualProtect(begin,span,PAGE_EXECUTE_READWRITE,&oldProtection)) {
        Log("CameraControls installed=0 reason=protect_failed error=%lu",GetLastError());
        return;
    }
    for (DWORD rva:cameraSites) {
        auto* site=base+rva;
        const LONG displacement=static_cast<LONG>(reinterpret_cast<INT_PTR>(&CameraActionAdapter)-reinterpret_cast<INT_PTR>(site+5));
        memcpy(site+1,&displacement,4);
    }
    FlushInstructionCache(GetCurrentProcess(),begin,span);
    const BOOL restored=VirtualProtect(begin,span,oldProtection,&ignored);
    Log("CameraControls installed=1 call_sites=4 actions=47..50 original_target_rva=0x576920 init_caller_rva=0x%lx protection_restored=%d gain_x=%.7g gain_y=%.7g invert_y=%d mouse_bindings_required=1 before_native_filter_and_accumulator=1",callerRva,restored,cameraGainX,cameraGainY,cameraInvert);
}
