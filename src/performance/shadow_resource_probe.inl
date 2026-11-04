// Bounded observation of real render/depth resources. This deliberately does
// not resize targets: shadow viewport, texel constants and projection must be
// changed together once the native producer has been identified by evidence.
#pragma optimize("y",off)
namespace shadow_probe {
static bool enabled=false;
static SRWLOCK lock=SRWLOCK_INIT;
struct Record {UINT width{},height{},levels{};DWORD usage{};D3DFORMAT format{};HRESULT result{};DWORD callers[8]{};};
static Record records[64]{};
static unsigned used=0;
static volatile LONG saturated=0;
static bool Same(const Record& a,const Record& b) {
    return a.width==b.width&&a.height==b.height&&a.levels==b.levels&&a.usage==b.usage&&
        a.format==b.format&&a.result==b.result&&!memcmp(a.callers,b.callers,sizeof(a.callers));
}
static __declspec(noinline) void Observe(const char* kind,UINT width,UINT height,UINT levels,DWORD usage,D3DFORMAT format,HRESULT result,void* resource) {
    if(!enabled||InterlockedCompareExchange(&saturated,0,0))return;
    const DWORD saved=GetLastError();
    Record item{};item.width=width;item.height=height;item.levels=levels;item.usage=usage;item.format=format;item.result=result;
    void* stack[16]{};const USHORT count=CaptureStackBackTrace(1,16,stack,nullptr);
    const auto base=reinterpret_cast<ULONG_PTR>(GetModuleHandleW(nullptr));
    const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* pe=reinterpret_cast<const IMAGE_NT_HEADERS*>(base+dos->e_lfanew);
    unsigned targetFrames=0;
    for(unsigned n=0;n<count&&targetFrames<8;++n) {
        const auto address=reinterpret_cast<ULONG_PTR>(stack[n]);
        if(address>=base&&address-base<pe->OptionalHeader.SizeOfImage)item.callers[targetFrames++]=static_cast<DWORD>(address-base);
    }
    bool fresh=false;
    AcquireSRWLockExclusive(&lock);
    if(used<64) {
        fresh=true;for(unsigned n=0;n<used;++n)if(Same(records[n],item)){fresh=false;break;}
        if(fresh){records[used++]=item;if(used==64)InterlockedExchange(&saturated,1);}
    }
    ReleaseSRWLockExclusive(&lock);
    if(fresh)Log("GPU resource probe kind=%s request=%ux%u levels=%u usage=%08lx format=%u result=%08lx resource=%p exe_rvas=%lx,%lx,%lx,%lx,%lx,%lx,%lx,%lx",
        kind,width,height,levels,usage,static_cast<unsigned>(format),static_cast<unsigned long>(result),resource,
        item.callers[0],item.callers[1],item.callers[2],item.callers[3],item.callers[4],item.callers[5],item.callers[6],item.callers[7]);
    SetLastError(saved);
}
}
static void ConfigureShadowResourceProbe(const wchar_t* config,bool supported) {
    shadow_probe::enabled=supported&&diagnostics&&ConfigFlag(L"Diagnostics",L"ShadowResources",false,config);
}
static __declspec(noinline) HRESULT STDMETHODCALLTYPE ObservedShadowTexture(IDirect3DDevice9* device,UINT width,UINT height,UINT levels,
    DWORD usage,D3DFORMAT format,D3DPOOL pool,IDirect3DTexture9** texture,HANDLE* shared) {
    auto original=reinterpret_cast<HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,UINT,UINT,UINT,DWORD,D3DFORMAT,D3DPOOL,IDirect3DTexture9**,HANDLE*)>(Original(device,23));
    const HRESULT result=original(device,width,height,levels,usage,format,pool,texture,shared);
    if(usage&(D3DUSAGE_RENDERTARGET|D3DUSAGE_DEPTHSTENCIL))
        shadow_probe::Observe("texture",width,height,levels,usage,format,result,SUCCEEDED(result)&&texture?*texture:nullptr);
    return result;
}
static __declspec(noinline) HRESULT STDMETHODCALLTYPE ObservedShadowDepthSurface(IDirect3DDevice9* device,UINT width,UINT height,D3DFORMAT format,
    D3DMULTISAMPLE_TYPE multisample,DWORD quality,BOOL discard,IDirect3DSurface9** surface,HANDLE* shared) {
    auto original=reinterpret_cast<HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,UINT,UINT,D3DFORMAT,D3DMULTISAMPLE_TYPE,DWORD,BOOL,IDirect3DSurface9**,HANDLE*)>(Original(device,29));
    const HRESULT result=original(device,width,height,format,multisample,quality,discard,surface,shared);
    shadow_probe::Observe("depth-surface",width,height,1,D3DUSAGE_DEPTHSTENCIL,format,result,SUCCEEDED(result)&&surface?*surface:nullptr);
    return result;
}
static __declspec(noinline) HRESULT STDMETHODCALLTYPE ObservedShadowColorSurface(IDirect3DDevice9* device,UINT width,UINT height,D3DFORMAT format,
    D3DMULTISAMPLE_TYPE multisample,DWORD quality,BOOL lockable,IDirect3DSurface9** surface,HANDLE* shared) {
    auto original=reinterpret_cast<HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,UINT,UINT,D3DFORMAT,D3DMULTISAMPLE_TYPE,DWORD,BOOL,IDirect3DSurface9**,HANDLE*)>(Original(device,28));
    const HRESULT result=original(device,width,height,format,multisample,quality,lockable,surface,shared);
    shadow_probe::Observe("color-surface",width,height,1,D3DUSAGE_RENDERTARGET,format,result,SUCCEEDED(result)&&surface?*surface:nullptr);
    return result;
}
#pragma optimize("y",on)
