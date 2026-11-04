// Included after Original(), GammaEnabled and selfModule in the x86 D3D9 proxy.
// Only identical LUT submissions are skipped. No periodic gamma refresh.
struct GammaEntry {
    IDirect3DDevice9* device{};
    HWND window{};
    ULONGLONG lifetime{},version{};
    unsigned activeCalls{};
    bool valid{},observerReady{};
    LONG epoch{};
    UINT swap{};
    DWORD flags{};
    D3DGAMMARAMP ramp{};
};
struct GammaThreadWatch { DWORD thread{}; HHOOK hook{}; bool pending{}; };
struct GammaWindowWatch { HWND window{}; DWORD thread{}; bool destroyed{}; };
static GammaEntry gammaEntries[8]{};
static GammaThreadWatch gammaThreads[8]{};
static GammaWindowWatch gammaWindows[8]{};
static SRWLOCK gammaStateLock=SRWLOCK_INIT;
static SRWLOCK gammaWatchLock=SRWLOCK_INIT;
static ULONGLONG gammaSerial=0; // Protected by gammaStateLock; never reset with an entry.
static volatile LONG gammaFocusEpoch=0;

static LONG GammaEpoch() { return InterlockedCompareExchange(&gammaFocusEpoch,0,0); }
static GammaEntry* GammaFind(IDirect3DDevice9* device) {
    for(auto& entry:gammaEntries) if(entry.device==device) return &entry;
    return nullptr; // Caller holds gammaStateLock. No COM dereference.
}
static LRESULT CALLBACK GammaWindowObserver(int code,WPARAM wParam,LPARAM lParam) {
    if(code>=0 && lParam) {
        const auto* message=reinterpret_cast<const CWPSTRUCT*>(lParam);
        const UINT id=message->message;
        if(id==WM_ACTIVATEAPP || id==WM_ACTIVATE || id==WM_DISPLAYCHANGE || id==WM_SIZE || id==WM_NCDESTROY) {
            AcquireSRWLockExclusive(&gammaWatchLock);
            for(auto& watch:gammaWindows) if(watch.window==message->hwnd) {
                if(id==WM_NCDESTROY) watch.destroyed=true;
                InterlockedIncrement(&gammaFocusEpoch);
                break;
            }
            ReleaseSRWLockExclusive(&gammaWatchLock);
        }
    }
    return CallNextHookEx(nullptr,code,wParam,lParam);
}
static bool GammaObserveWindow(HWND window) {
    DWORD process=0;
    const DWORD thread=GetWindowThreadProcessId(window,&process);
    if(!window || !thread || process!=GetCurrentProcessId() || !IsWindow(window)) return false;
    GammaThreadWatch* threadWatch=nullptr;
    GammaWindowWatch* windowWatch=nullptr;
    bool install=false;
    AcquireSRWLockExclusive(&gammaWatchLock);
    for(auto& watch:gammaWindows) if(watch.window==window) { windowWatch=&watch;break; }
    if(!windowWatch) for(auto& watch:gammaWindows) if(!watch.window || watch.destroyed) {windowWatch=&watch;break;}
    if(windowWatch) {
        for(auto& watch:gammaThreads) if(watch.thread==thread) {threadWatch=&watch;break;}
        if(!threadWatch) for(auto& watch:gammaThreads) if(!watch.thread) {
            threadWatch=&watch;watch.thread=thread;watch.pending=true;install=true;break;
        }
    }
    if(!threadWatch || (!install && (!threadWatch->hook || threadWatch->pending))) {
        ReleaseSRWLockExclusive(&gammaWatchLock);return false;
    }
    if(!install) {
        *windowWatch={window,thread,false};
        ReleaseSRWLockExclusive(&gammaWatchLock);return true;
    }
    ReleaseSRWLockExclusive(&gammaWatchLock);
    // Hook installation may invoke window machinery. No internal lock is held.
    const HHOOK hook=SetWindowsHookExW(WH_CALLWNDPROC,&GammaWindowObserver,selfModule,thread);
    AcquireSRWLockExclusive(&gammaWatchLock);
    threadWatch->pending=false;
    if(hook) {
        threadWatch->hook=hook;
        // Re-select a window slot after releasing the lock for installation.
        windowWatch=nullptr;
        for(auto& watch:gammaWindows) if(watch.window==window) {windowWatch=&watch;break;}
        if(!windowWatch) for(auto& watch:gammaWindows) if(!watch.window || watch.destroyed) {windowWatch=&watch;break;}
        if(windowWatch) *windowWatch={window,thread,false};
    } else *threadWatch={};
    const bool ready=hook && windowWatch;
    ReleaseSRWLockExclusive(&gammaWatchLock);
    return ready;
}
static bool GammaWindowReady(HWND window) {
    bool ready=false;
    AcquireSRWLockShared(&gammaWatchLock);
    for(const auto& watch:gammaWindows) if(watch.window==window && !watch.destroyed) {ready=true;break;}
    ReleaseSRWLockShared(&gammaWatchLock);
    return ready;
}
static bool GammaWindowActive(HWND window) {
    if(!window || !IsWindow(window) || IsIconic(window)) return false;
    const HWND foreground=GetForegroundWindow();
    return foreground && GetAncestor(foreground,GA_ROOT)==GetAncestor(window,GA_ROOT);
}
static bool GammaRegister(IDirect3DDevice9* device,HWND window) {
    if(!device || !GammaEnabled) return false;
    const bool observed=GammaObserveWindow(window);
    AcquireSRWLockExclusive(&gammaStateLock);
    auto* entry=GammaFind(device);
    if(!entry) for(auto& candidate:gammaEntries) if(!candidate.device) {entry=&candidate;break;}
    if(entry) {
        // Registration denotes a fresh creation/window lifecycle, including address reuse.
        *entry={};entry->device=device;entry->lifetime=++gammaSerial;
        entry->window=window;entry->observerReady=observed;
        entry->valid=false;entry->version=++gammaSerial;
    }
    ReleaseSRWLockExclusive(&gammaStateLock);
    return entry && observed;
}
static void GammaInvalidate(IDirect3DDevice9* device) {
    AcquireSRWLockExclusive(&gammaStateLock);
    if(auto* entry=GammaFind(device)) {entry->valid=false;entry->version=++gammaSerial;}
    ReleaseSRWLockExclusive(&gammaStateLock);
}
static ULONGLONG GammaGeneration(IDirect3DDevice9* device) {
    AcquireSRWLockShared(&gammaStateLock);
    const auto* entry=GammaFind(device);
    const ULONGLONG result=entry ? entry->lifetime : 0;
    ReleaseSRWLockShared(&gammaStateLock);
    return result;
}
static void GammaForget(IDirect3DDevice9* device,ULONGLONG expectedLifetime=0) {
    AcquireSRWLockExclusive(&gammaStateLock);
    if(auto* entry=GammaFind(device)) {
        if(!expectedLifetime || entry->lifetime==expectedLifetime) {++gammaSerial;*entry={};}
    }
    ReleaseSRWLockExclusive(&gammaStateLock);
}
static void STDMETHODCALLTYPE ObservedGamma(IDirect3DDevice9* device,UINT swap,DWORD flags,const D3DGAMMARAMP* ramp) {
    using GammaMethod=void (STDMETHODCALLTYPE*)(IDirect3DDevice9*,UINT,DWORD,const D3DGAMMARAMP*);
    const auto original=reinterpret_cast<GammaMethod>(Original(device,21));
    D3DGAMMARAMP snapshot{};
    if(ramp) snapshot=*ramp;
    HWND window=nullptr;ULONGLONG lifetime=0;
    AcquireSRWLockShared(&gammaStateLock);
    if(const auto* entry=GammaFind(device)) {window=entry->window;lifetime=entry->lifetime;}
    ReleaseSRWLockShared(&gammaStateLock);
    const bool active=GammaWindowActive(window) && GammaWindowReady(window);
    LONG epoch=GammaEpoch();
    bool tracked=false,solo=false,eligible=false;
    ULONGLONG version=0;
    AcquireSRWLockExclusive(&gammaStateLock);
    auto* entry=GammaFind(device);
    if(entry && entry->lifetime==lifetime) {
        epoch=GammaEpoch();
        eligible=GammaEnabled && ramp && swap==0 && active && entry->observerReady;
        if(eligible && entry->valid && !entry->activeCalls && entry->epoch==epoch &&
            entry->swap==swap && entry->flags==flags && !memcmp(&entry->ramp,&snapshot,sizeof(snapshot))) {
            ReleaseSRWLockExclusive(&gammaStateLock);return;
        }
        solo=entry->activeCalls==0;
        ++entry->activeCalls;entry->valid=false;entry->version=version=++gammaSerial;tracked=true;
    }
    ReleaseSRWLockExclusive(&gammaStateLock);
    // Never hold either internal lock across a COM call or callback.
    original(device,swap,flags,ramp ? &snapshot : nullptr);
    const LONG afterEpoch=GammaEpoch();
    if(tracked) {
        AcquireSRWLockExclusive(&gammaStateLock);
        entry=GammaFind(device);
        if(entry && entry->lifetime==lifetime) {
            if(entry->activeCalls) --entry->activeCalls;
            if(solo && eligible && entry->version==version && !entry->activeCalls && epoch==afterEpoch) {
                entry->ramp=snapshot;entry->swap=swap;entry->flags=flags;entry->epoch=epoch;entry->valid=true;
            }
        }
        ReleaseSRWLockExclusive(&gammaStateLock);
    }
}
