#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstring>
static bool diagnostics=true;
static unsigned messages=0,calls=0,checks=0;
static HRESULT outcome=S_OK;
static bool ConfigFlag(const wchar_t*,const wchar_t*,bool,const wchar_t*){return true;}
static void Log(const char*,...){++messages;SetLastError(987);}
static void Check(bool value,const char* text){++checks;if(!value){fprintf(stderr,"FAIL %s\n",text);ExitProcess(1);}}
static HRESULT STDMETHODCALLTYPE Texture(IDirect3DDevice9* dev,UINT w,UINT h,UINT levels,DWORD usage,D3DFORMAT format,D3DPOOL pool,IDirect3DTexture9** output,HANDLE* shared){
    ++calls;Check(dev==reinterpret_cast<IDirect3DDevice9*>(1)&&w==2048&&h==1024&&levels==1&&
        usage==D3DUSAGE_RENDERTARGET&&format==D3DFMT_R32F&&pool==D3DPOOL_DEFAULT&&!shared,"texture arguments forwarded exactly");
    if(SUCCEEDED(outcome))*output=reinterpret_cast<IDirect3DTexture9*>(2);SetLastError(123);return outcome;
}
static HRESULT STDMETHODCALLTYPE Depth(IDirect3DDevice9* dev,UINT w,UINT h,D3DFORMAT format,D3DMULTISAMPLE_TYPE sample,DWORD quality,BOOL discard,IDirect3DSurface9** output,HANDLE* shared){
    ++calls;Check(dev==reinterpret_cast<IDirect3DDevice9*>(1)&&w==2048&&h==1024&&format==D3DFMT_D24S8&&
        sample==D3DMULTISAMPLE_NONE&&quality==0&&discard&&!shared,"depth arguments forwarded exactly");
    if(SUCCEEDED(outcome))*output=reinterpret_cast<IDirect3DSurface9*>(3);SetLastError(123);return outcome;
}
static HRESULT STDMETHODCALLTYPE Color(IDirect3DDevice9* dev,UINT w,UINT h,D3DFORMAT format,D3DMULTISAMPLE_TYPE sample,DWORD quality,BOOL lockable,IDirect3DSurface9** output,HANDLE* shared){
    ++calls;Check(dev==reinterpret_cast<IDirect3DDevice9*>(1)&&w==2048&&h==1024&&format==D3DFMT_A8R8G8B8&&
        sample==D3DMULTISAMPLE_NONE&&quality==0&&!lockable&&!shared,"surface arguments forwarded exactly");
    if(SUCCEEDED(outcome))*output=reinterpret_cast<IDirect3DSurface9*>(4);SetLastError(123);return outcome;
}
static void* Original(void*,unsigned index) {
    if(index==23)return reinterpret_cast<void*>(&Texture);
    if(index==29)return reinterpret_cast<void*>(&Depth);
    Check(index==28,"correct original vtable slot");return reinterpret_cast<void*>(&Color);
}
#include "../shadow_resource_probe.inl"
int main(){
    ConfigureShadowResourceProbe(L"unused",false);Check(!shadow_probe::enabled,"unsupported disabled");
    diagnostics=false;ConfigureShadowResourceProbe(L"unused",true);Check(!shadow_probe::enabled,"diagnostics off disabled");
    diagnostics=true;ConfigureShadowResourceProbe(L"unused",true);Check(shadow_probe::enabled,"opt-in diagnostics enabled");
    auto* dev=reinterpret_cast<IDirect3DDevice9*>(1);
    for(unsigned n=0;n<2;++n) {
        outcome=n?D3DERR_OUTOFVIDEOMEMORY:S_OK;
        IDirect3DTexture9* texture=reinterpret_cast<IDirect3DTexture9*>(9);
        Check(ObservedShadowTexture(dev,2048,1024,1,D3DUSAGE_RENDERTARGET,D3DFMT_R32F,D3DPOOL_DEFAULT,&texture,nullptr)==outcome,"texture HRESULT unchanged");
        Check(texture==reinterpret_cast<IDirect3DTexture9*>(n?9:2)&&GetLastError()==123,"texture output/error unchanged");
        IDirect3DSurface9* surface=reinterpret_cast<IDirect3DSurface9*>(9);
        Check(ObservedShadowDepthSurface(dev,2048,1024,D3DFMT_D24S8,D3DMULTISAMPLE_NONE,0,TRUE,&surface,nullptr)==outcome,"depth HRESULT unchanged");
        Check(surface==reinterpret_cast<IDirect3DSurface9*>(n?9:3)&&GetLastError()==123,"depth output/error unchanged");
        surface=reinterpret_cast<IDirect3DSurface9*>(9);
        Check(ObservedShadowColorSurface(dev,2048,1024,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&surface,nullptr)==outcome,"color HRESULT unchanged");
        Check(surface==reinterpret_cast<IDirect3DSurface9*>(n?9:4)&&GetLastError()==123,"color output/error unchanged");
    }
    const auto start=shadow_probe::used;
    for(UINT n=0;n<200;++n)shadow_probe::Observe("synthetic",4000+n,4096,1,D3DUSAGE_RENDERTARGET,D3DFMT_R32F,S_OK,nullptr);
    Check(start>=3&&shadow_probe::used==64&&shadow_probe::saturated==1,"bounded unique resource capture");
    Check(calls==6&&messages==64,"bounded logging/no extra COM calls");
    printf("PASS %u shadow probe forwarding, failure, LastError and bounded-capture checks\n",checks);
    return 0;
}
