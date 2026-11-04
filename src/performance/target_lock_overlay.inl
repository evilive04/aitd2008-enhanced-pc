// Include after ff_overlay.inl and hud_presentation.inl, before sss_pipeline.inl.
#include "target_lock_state.h"
#include <cmath>
namespace target_lock_overlay {
static TargetLockProjectedStateGetter getter=nullptr;
static DWORD nextLookup{};
static SRWLOCK getterLock=SRWLOCK_INIT;
static bool State(TargetLockStateV2& state){
    const DWORD now=GetTickCount();
    if(!TryAcquireSRWLockExclusive(&getterLock))return false;
    if(!getter&&(!nextLookup||static_cast<LONG>(now-nextLookup)>=0)){
        nextLookup=now+1000;const auto input=GetModuleHandleW(L"dinput8.dll");
        if(input&&FfSameDirectory(input))getter=reinterpret_cast<TargetLockProjectedStateGetter>(GetProcAddress(input,"AITDGetTargetLockState"));
    }
    const auto read=getter;ReleaseSRWLockExclusive(&getterLock);
    if(!read||!read(&state,sizeof(state))||state.size!=sizeof(state)||state.version!=2||
        (state.flags&3)!=3||!state.actor||!state.camera||now-state.tick>100||now-state.projectionTick>100||
        !std::isfinite(state.screenX)||!std::isfinite(state.screenY)||state.screenX<=0||state.screenX>=1||state.screenY<=0||state.screenY>=1)return false;
    for(float value:state.world)if(!std::isfinite(value))return false;
    return true;
}
// No per-draw projection or GPU resource is retained across frames/resets.
static void Invalidate(){}
static void Stroke(ui_theme::Mesh& mesh,ui_theme::Point a,ui_theme::Point b,float width,D3DCOLOR color){
    const float dx=b.x-a.x,dy=b.y-a.y,length=sqrtf(dx*dx+dy*dy);if(length<.01f)return;
    const float nx=-dy/length*width*.5f,ny=dx/length*width*.5f;
    ui_theme::Triangle(mesh,{a.x+nx,a.y+ny},{b.x+nx,b.y+ny},{b.x-nx,b.y-ny},color);
    ui_theme::Triangle(mesh,{a.x+nx,a.y+ny},{b.x-nx,b.y-ny},{a.x-nx,a.y-ny},color);
}
static void Draw(IDirect3DDevice9* device,const RECT* source,const RECT* destination)noexcept{
    if(!device||source||destination)return;
    TargetLockStateV2 state{};if(!State(state))return;
    DWORD pid{};const auto window=GetForegroundWindow();GetWindowThreadProcessId(window,&pid);
    if(pid!=GetCurrentProcessId()||IsIconic(window))return;
    try{
        FfDevice metadata{};AcquireSRWLockShared(&ffDeviceLock);if(const auto* item=FfFind(device))metadata=*item;ReleaseSRWLockShared(&ffDeviceLock);
        D3DSURFACE_DESC desc{};if(!hud_presentation::Target(device,metadata,desc))return;
        if(InterlockedCompareExchange(&ffDrawing,1,0))return;
        struct Unlock{~Unlock(){InterlockedExchange(&ffDrawing,0);}}unlock;
        ui_theme::Mesh mesh;mesh.reserve(48);
        const float scale=std::clamp(desc.Height/1080.f,.65f,2.5f),radius=7*scale;
        const float x=state.screenX*desc.Width,y=state.screenY*desc.Height;
        const ui_theme::Point points[4]={{x,y-radius},{x+radius,y},{x,y+radius},{x-radius,y}};
        // Small hollow amber diamond, with a thin dark keyline for readability.
        for(unsigned n=0;n<4;++n)Stroke(mesh,points[n],points[(n+1)%4],3.2f*scale,0xc0100e0a);
        for(unsigned n=0;n<4;++n)Stroke(mesh,points[n],points[(n+1)%4],1.3f*scale,0xf0e4be73);
        ui_theme::Draw(device,mesh,1,0,0);
    }catch(...){/* A missing marker must never affect game rendering/input. */}
}
}
