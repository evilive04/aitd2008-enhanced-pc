#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstring>
static void Log(const char*,...){}
#include "borderless.inl"
static void Check(bool ok,const char* text){if(!ok){printf("FAIL: %s\n",text);fflush(stdout);ExitProcess(1);}}
int main(){
    Check(SetProcessDPIAware()!=FALSE,"pixel aware target (set before window creation)");
    WNDCLASSW klass{};klass.lpfnWndProc=DefWindowProcW;klass.hInstance=GetModuleHandleW(nullptr);klass.lpszClassName=L"AITDBorderlessContract";
    Check(RegisterClassW(&klass)!=0,"register own class");
    const HWND window=CreateWindowExW(WS_EX_CLIENTEDGE,klass.lpszClassName,L"Enhanced borderless test",WS_OVERLAPPEDWINDOW,
        100,110,640,480,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    Check(window!=nullptr,"create hidden own window");
    const auto dpi=GetThreadDpiAwarenessContext();
    D3DPRESENT_PARAMETERS original{};original.Windowed=FALSE;original.BackBufferWidth=1280;
    original.BackBufferHeight=720;original.BackBufferFormat=D3DFMT_X8R8G8B8;original.FullScreen_RefreshRateInHz=60;
    original.SwapEffect=D3DSWAPEFFECT_FLIP;original.EnableAutoDepthStencil=TRUE;original.AutoDepthStencilFormat=D3DFMT_D24S8;
    auto parameters=original;BorderlessChange change{};
    Check(!BorderlessPrepare(&parameters,window,change)&&!memcmp(&parameters,&original,sizeof(parameters)),"disabled identity");
    borderlessFullscreen=true;
    Check(!BorderlessPrepare(nullptr,window,change),"null rejected");
    Check(BorderlessPrepare(&parameters,window,change),"prepare real own hidden window");
    Check(AreDpiAwarenessContextsEqual(dpi,GetThreadDpiAwarenessContext())!=FALSE,"thread DPI restored");
    Check(parameters.Windowed&&parameters.hDeviceWindow==window&&parameters.FullScreen_RefreshRateInHz==0&&
        parameters.BackBufferFormat==D3DFMT_UNKNOWN&&parameters.PresentationInterval==D3DPRESENT_INTERVAL_IMMEDIATE,"windowed D3D contract");
    Check(parameters.SwapEffect==original.SwapEffect&&parameters.AutoDepthStencilFormat==original.AutoDepthStencilFormat&&
        parameters.EnableAutoDepthStencil==original.EnableAutoDepthStencil,"swap and depth preserved");
    {
        BorderlessDpiScope physical;MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);
        Check(GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor)!=FALSE,"monitor");
        RECT outer{},client{};GetWindowRect(window,&outer);GetClientRect(window,&client);
        Check(EqualRect(&outer,&monitor.rcMonitor)!=FALSE,"exact monitor rectangle");
        printf("client %ld,%ld render %u,%u style %lx ex %lx dpi %u\n",client.right,client.bottom,parameters.BackBufferWidth,parameters.BackBufferHeight,static_cast<DWORD>(GetWindowLongPtrW(window,GWL_STYLE)),static_cast<DWORD>(GetWindowLongPtrW(window,GWL_EXSTYLE)),GetDpiForWindow(window));Check(client.right==static_cast<LONG>(parameters.BackBufferWidth)&&client.bottom==static_cast<LONG>(parameters.BackBufferHeight),"physical client equals render pixels");
        Check(!(GetWindowLongPtrW(window,GWL_STYLE)&WS_OVERLAPPEDWINDOW)&&!(GetWindowLongPtrW(window,GWL_EXSTYLE)&WS_EX_TOPMOST),"no frame or topmost");
    }
    BorderlessChange unchanged{};auto repeated=parameters;
    Check(BorderlessPrepare(&repeated,window,unchanged)&&!unchanged.windowChanged,"unchanged Reset performs no window mutation");
    BorderlessRollback(change,&parameters);
    Check(!memcmp(&parameters,&original,sizeof(parameters)),"failed D3D rollback restores parameters");
    Check(GetWindowLongPtrW(window,GWL_STYLE)==change.style&&GetWindowLongPtrW(window,GWL_EXSTYLE)==change.extended,"rollback styles");
    {BorderlessDpiScope physical;RECT rectangle{};GetWindowRect(window,&rectangle);Check(EqualRect(&rectangle,&change.rectangle)!=FALSE,"rollback rectangle");}
    borderlessWindow=window;Check(IsWindow(borderlessWindow)!=FALSE,"reset window fallback");
    BorderlessChange reset{};Check(BorderlessPrepare(&parameters,borderlessWindow,reset),"Reset prepares windowed again");
    BorderlessRollback(reset,&parameters);DestroyWindow(window);
    printf("PASS: real hidden window, disabled/null, physical monitor/client, D3D fields, DPI restored, rollback, Reset fallback\n");
}
