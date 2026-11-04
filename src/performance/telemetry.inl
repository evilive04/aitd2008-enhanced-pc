// Temporary, bounded diagnostics. No pacing or render settings are changed.
static LARGE_INTEGER perfFrequency{};
static LONGLONG perfStart=0,perfPrevious=0;
static unsigned perfFrames=0;
static double perfPresentMs=0,perfWorstMs=0;
static unsigned long long perfCpuStart=0;
static unsigned long long CpuTime() {
    FILETIME c{},e{},k{},u{};
    if (!GetProcessTimes(GetCurrentProcess(),&c,&e,&k,&u)) return 0;
    return (static_cast<unsigned long long>(k.dwHighDateTime)<<32|k.dwLowDateTime)+
           (static_cast<unsigned long long>(u.dwHighDateTime)<<32|u.dwLowDateTime);
}
static void PerfRecord(IDirect3DDevice9* device,LONGLONG before,LONGLONG after) {
    if (!diagnostics || !before || after<before) return;
    if (!perfFrequency.QuadPart) QueryPerformanceFrequency(&perfFrequency);
    if (!perfFrequency.QuadPart) return;
    if (!perfStart) {perfStart=before;perfPrevious=before;perfCpuStart=CpuTime();return;}
    const double elapsed=static_cast<double>(after-perfStart)/perfFrequency.QuadPart;
    const double frameMs=1000.0*(before-perfPrevious)/perfFrequency.QuadPart;
    perfPrevious=before;++perfFrames;
    perfPresentMs+=1000.0*(after-before)/perfFrequency.QuadPart;
    if (frameMs>perfWorstMs) perfWorstMs=frameMs;
    if (elapsed>=2.0) {
        const auto cpu=CpuTime();
        IDirect3DSwapChain9* chain=nullptr;D3DPRESENT_PARAMETERS params{};
        if (SUCCEEDED(device->GetSwapChain(0,&chain))) {chain->GetPresentParameters(&params);chain->Release();}
        DWORD_PTR mask=0,system=0;GetProcessAffinityMask(GetCurrentProcess(),&mask,&system);
        Log("PERF frames=%u seconds=%.4f fps=%.2f present_mean_ms=%.3f worst_frame_ms=%.3f cpu_cores=%.3f affinity=%08lx active=%d interval=%u size=%ux%u",perfFrames,elapsed,perfFrames/elapsed,perfPresentMs/perfFrames,perfWorstMs,
            static_cast<double>(cpu-perfCpuStart)/10000000.0/elapsed,static_cast<DWORD>(mask),FrameWindowActive(params.hDeviceWindow),params.PresentationInterval,params.BackBufferWidth,params.BackBufferHeight);
        perfStart=after;perfCpuStart=cpu;perfFrames=0;perfPresentMs=0;perfWorstMs=0;
    }
}
