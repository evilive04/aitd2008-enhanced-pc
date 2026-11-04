// Included by proxy.cpp. Bounded, opt-in pacing of ordinary Device9::Present.
// No simulation-clock changes, global timer resolution changes or spin waiting.
static UINT frameLimit=0;
static HANDLE frameTimer=nullptr;
static LONGLONG frameFrequency=0;
static SRWLOCK frameLock=SRWLOCK_INIT;
static bool frameTimerFailed=false;
static ULONGLONG frameGeneration=0;
struct FrameState {
    IDirect3DDevice9* device;
    HWND window;
    LONGLONG previous;
    DWORD thread;
    bool lost;
    ULONGLONG generation;
};
static FrameState frameStates[8]={};

static void InitializeFramePacing(const wchar_t* config) {
    wchar_t value[32];
    GetPrivateProfileStringW(L"Timing",L"MaxFPS",L"0",value,32,config);
    if (!wcscmp(value,L"30")) frameLimit=30;
    else if (!wcscmp(value,L"60")) frameLimit=60;
    else if (!wcscmp(value,L"120")) frameLimit=120;
    else {
        if (wcscmp(value,L"0")) Log("Frame pacing disabled: MaxFPS must be 0, 30, 60 or 120");
        return;
    }
    LARGE_INTEGER frequency;
    if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart<=0 || frequency.QuadPart>1000000000000LL) {
        frameLimit=0;
        Log("Frame pacing disabled: unsupported performance counter");
        return;
    }
    frameFrequency=frequency.QuadPart;
    frameTimer=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
        TIMER_MODIFY_STATE | SYNCHRONIZE);
    const bool highResolution=frameTimer!=nullptr;
    if (!frameTimer) frameTimer=CreateWaitableTimerExW(nullptr,nullptr,0,TIMER_MODIFY_STATE | SYNCHRONIZE);
    if (!frameTimer) { frameLimit=0; Log("Frame pacing disabled: timer creation failed"); return; }
    // One process-lifetime handle. No worker thread or destructor under loader lock.
    Log("Frame pacing requested=%u high_resolution_timer=%d; experimental, Device9::Present only",
        frameLimit,highResolution);
}

static FrameState* FindFrameState(IDirect3DDevice9* device) {
    // Caller holds frameLock. Never dereference the device stored here.
    for (auto& state:frameStates) if (state.device==device) return &state;
    return nullptr;
}

static void RegisterFrameDevice(IDirect3DDevice9* device,HWND window) {
    if (!frameLimit || !window) return;
    AcquireSRWLockExclusive(&frameLock);
    FrameState* available=FindFrameState(device);
    if (!available) for (auto& state:frameStates) if (!state.device) { available=&state; break; }
    if (available) *available={device,window,0,0,false,++frameGeneration};
    ReleaseSRWLockExclusive(&frameLock);
    Log("Frame pacing device=%p registered=%d",device,available!=nullptr);
}

static ULONGLONG FrameDeviceGeneration(IDirect3DDevice9* device) {
    AcquireSRWLockShared(&frameLock);
    const auto* state=FindFrameState(device);
    const ULONGLONG generation=state ? state->generation : 0;
    ReleaseSRWLockShared(&frameLock);
    return generation;
}

static void ForgetFrameDevice(IDirect3DDevice9* device,ULONGLONG generation) {
    AcquireSRWLockExclusive(&frameLock);
    if (auto* state=FindFrameState(device)) {
        if (state->generation==generation) *state={};
    }
    ReleaseSRWLockExclusive(&frameLock);
}

static void ResetFrameDevice(IDirect3DDevice9* device,bool lost,HWND window=nullptr) {
    if (!frameLimit) return;
    AcquireSRWLockExclusive(&frameLock);
    if (auto* state=FindFrameState(device)) {
        state->previous=0;
        state->thread=0;
        state->lost=lost;
        if (window) state->window=window;
    }
    ReleaseSRWLockExclusive(&frameLock);
}

static bool FrameWindowActive(HWND window) {
    if (!window || !IsWindow(window) || IsIconic(window)) return false;
    const HWND foreground=GetForegroundWindow();
    return foreground && GetAncestor(foreground,GA_ROOT)==GetAncestor(window,GA_ROOT);
}

static void PaceFrame(IDirect3DDevice9* device,HWND overrideWindow) {
    // Concurrent/reentrant rendering fails open rather than serializing COM calls.
    if (!frameLimit || !TryAcquireSRWLockExclusive(&frameLock)) return;
    auto* state=FindFrameState(device);
    LARGE_INTEGER now;
    if (!state || frameTimerFailed) { ReleaseSRWLockExclusive(&frameLock); return; }
    const HWND window=overrideWindow ? overrideWindow : state->window;
    if (state->lost || !FrameWindowActive(window) || !QueryPerformanceCounter(&now)) {
        state->previous=0;
        ReleaseSRWLockExclusive(&frameLock);
        return;
    }
    const DWORD thread=GetCurrentThreadId();
    const LONGLONG period=(frameFrequency+frameLimit-1)/frameLimit;
    const LONGLONG elapsed=now.QuadPart-state->previous;
    if (state->previous && state->thread==thread && elapsed>=0 && elapsed<period) {
        const LONGLONG remaining=period-elapsed;
        LARGE_INTEGER due;
        // Ceil conversion; maximum request is one 30/60/120 FPS period.
        due.QuadPart=-((remaining*10000000LL+frameFrequency-1)/frameFrequency);
        bool failed=!SetWaitableTimer(frameTimer,&due,0,nullptr,nullptr,FALSE);
        if (!failed) failed=WaitForSingleObject(frameTimer,50)!=WAIT_OBJECT_0;
        if (failed) {
            CancelWaitableTimer(frameTimer);
            frameTimerFailed=true;
            Log("Frame pacing disabled for this process: timer wait failed");
        }
        if (!QueryPerformanceCounter(&now)) now.QuadPart=0;
    }
    // Base each deadline on the actual start, never accumulate catch-up frames.
    state->previous=FrameWindowActive(window) ? now.QuadPart : 0;
    state->thread=thread;
    ReleaseSRWLockExclusive(&frameLock);
}
