// Local window/D3D transaction; no desktop mode, registry or global DPI changes.
static bool borderlessFullscreen=false;
static HWND borderlessWindow=nullptr;
struct BorderlessDpiScope {
    DPI_AWARENESS_CONTEXT previous;
    BorderlessDpiScope():previous(SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {}
    ~BorderlessDpiScope(){if(previous)SetThreadDpiAwarenessContext(previous);}
};
struct BorderlessChange {
    HWND window{};
    LONG_PTR style{},extended{};
    RECT rectangle{};
    D3DPRESENT_PARAMETERS original{};
    bool active{},windowChanged{};
};
static bool BorderlessSetStyle(HWND window,int index,LONG_PTR value) {
    SetLastError(0);
    return SetWindowLongPtrW(window,index,value)!=0 || GetLastError()==0;
}
static void BorderlessRollback(BorderlessChange& change,D3DPRESENT_PARAMETERS* parameters) {
    if(!change.active)return;
    BorderlessDpiScope dpi;
    if(change.windowChanged) {
    BorderlessSetStyle(change.window,GWL_STYLE,change.style);
    BorderlessSetStyle(change.window,GWL_EXSTYLE,change.extended);
    SetWindowPos(change.window,(change.extended&WS_EX_TOPMOST)?HWND_TOPMOST:HWND_NOTOPMOST,
        change.rectangle.left,change.rectangle.top,change.rectangle.right-change.rectangle.left,
        change.rectangle.bottom-change.rectangle.top,SWP_NOACTIVATE|SWP_FRAMECHANGED);
    }
    if(parameters)*parameters=change.original;
    change.active=false;
}
static bool BorderlessPrepare(D3DPRESENT_PARAMETERS* parameters,HWND fallback,BorderlessChange& change) {
    if(!borderlessFullscreen || !parameters)return false;
    const HWND window=parameters->hDeviceWindow?parameters->hDeviceWindow:fallback;
    DWORD process=0;GetWindowThreadProcessId(window,&process);
    if(!window || process!=GetCurrentProcessId() || !IsWindow(window))return false;
    // The supported game first creates a hidden "D3D Tester" device to
    // validate its display mode. Changing that probe to windowed makes the
    // native test reject the mode and restore 1280x720. Apply our layout only
    // to the authenticated game's actual engine window, including its Resets.
    wchar_t windowClass[64]{};
    if(GetClassNameW(window,windowClass,64)<=0||wcscmp(windowClass,L"Eden Games")) {
        Log("Borderless skipped non-engine window hwnd=%p class=%ls; native display test retained",window,windowClass);
        return false;
    }
    BorderlessDpiScope dpi;
    MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);
    if(!GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor) || !GetWindowRect(window,&change.rectangle))return false;
    const LONG width=monitor.rcMonitor.right-monitor.rcMonitor.left;
    const LONG height=monitor.rcMonitor.bottom-monitor.rcMonitor.top;
    if(width<=0 || height<=0)return false;
    change.window=window;change.style=GetWindowLongPtrW(window,GWL_STYLE);
    change.extended=GetWindowLongPtrW(window,GWL_EXSTYLE);change.original=*parameters;
    if(change.style&WS_CHILD)return false;
    change.active=true;
    const LONG_PTR style=(change.style&~static_cast<LONG_PTR>(WS_OVERLAPPEDWINDOW))|WS_POPUP;
    const LONG_PTR extended=change.extended&~static_cast<LONG_PTR>(WS_EX_WINDOWEDGE|WS_EX_CLIENTEDGE|WS_EX_DLGMODALFRAME|WS_EX_TOPMOST);
    // A no-op Reset must not send WM_SIZE/WM_NCCALCSIZE and trigger another Reset.
    if(style!=change.style || extended!=change.extended || !EqualRect(&change.rectangle,&monitor.rcMonitor)) {
        change.windowChanged=true;
        if(!BorderlessSetStyle(window,GWL_STYLE,style) || !BorderlessSetStyle(window,GWL_EXSTYLE,extended) ||
            !SetWindowPos(window,HWND_NOTOPMOST,monitor.rcMonitor.left,monitor.rcMonitor.top,width,height,SWP_NOACTIVATE|SWP_FRAMECHANGED)) {
            BorderlessRollback(change,parameters);Log("Borderless layout failed; original mode retained");return false;
        }
    }
    parameters->Windowed=TRUE;parameters->hDeviceWindow=window;
    parameters->BackBufferWidth=static_cast<UINT>(width);parameters->BackBufferHeight=static_cast<UINT>(height);
    parameters->BackBufferFormat=D3DFMT_UNKNOWN;parameters->FullScreen_RefreshRateInHz=0;
    parameters->PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    // Preserve swap/depth/MSAA semantics; no stretching a smaller render buffer.
    Log("Borderless prepared hwnd=%p monitor=%ld,%ld %ldx%ld",window,monitor.rcMonitor.left,monitor.rcMonitor.top,width,height);
    return true;
}
