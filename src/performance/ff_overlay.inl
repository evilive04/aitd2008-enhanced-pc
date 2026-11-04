// Geometry-only FF indicator, drawn solely for confirmed, fresh acceleration state.
#include "fast_forward_state.h"
#include "combat_aim_state.h"
#include "weapon_hud_state.h"
struct FfVertex { float x,y,z,rhw;D3DCOLOR color; };
static constexpr unsigned ffMaxVertices=768;
struct FfMesh { FfVertex vertices[ffMaxVertices]{};unsigned count{}; };
static void FfTriangle(FfMesh& mesh,float ax,float ay,float bx,float by,float cx,float cy,D3DCOLOR color) {
    if(mesh.count+3>ffMaxVertices)return;
    mesh.vertices[mesh.count++]={ax-.5f,ay-.5f,0,1,color};
    mesh.vertices[mesh.count++]={bx-.5f,by-.5f,0,1,color};
    mesh.vertices[mesh.count++]={cx-.5f,cy-.5f,0,1,color};
}
static void FfRect(FfMesh& mesh,float x,float y,float w,float h,D3DCOLOR color) {
    FfTriangle(mesh,x,y,x+w,y,x+w,y+h,color);FfTriangle(mesh,x,y,x+w,y+h,x,y+h,color);
}
static FfMesh FfBuildMesh(UINT width,UINT height) {
    FfMesh mesh{};
    if(width<320 || height<200)return mesh;
    const float scale=static_cast<float>(height)/1080.0f;
    const float w=84*scale,h=44*scale;
    const float right=static_cast<float>(width)*.95f,bottom=static_cast<float>(height)*.95f;
    const float x=right-w,y=bottom-h;
    if(x<0 || y<0)return mesh;
    const auto plate=D3DCOLOR_ARGB(145,17,17,15),edge=D3DCOLOR_ARGB(170,111,103,85);
    const auto amber=D3DCOLOR_ARGB(235,200,138,61),cream=D3DCOLOR_ARGB(248,242,230,201);
    // Angular backplate: clipped top-right and bottom-left corners, no stretched art.
    const float cut=5*scale;
    FfTriangle(mesh,x,y,right-cut,y,right,y+cut,plate);
    FfTriangle(mesh,x,y,right,y+cut,right,bottom,plate);
    FfTriangle(mesh,x,y,right,bottom,x+cut,bottom,plate);
    FfTriangle(mesh,x,y,x+cut,bottom,x,bottom-cut,plate);
    FfRect(mesh,x,y,2*scale,h-cut,amber);
    FfRect(mesh,x+5*scale,y,13*scale,scale,edge);
    FfRect(mesh,right-18*scale,bottom-scale,18*scale,scale,edge);
    for(unsigned n=0;n<2;++n) {
        const float a=x+(20+24*static_cast<float>(n))*scale,b=y+10*scale;
        FfTriangle(mesh,a+scale,b+scale,a+scale,b+25*scale,a+19*scale,b+13*scale,D3DCOLOR_ARGB(95,0,0,0));
        FfTriangle(mesh,a,b,a,b+24*scale,a+18*scale,b+12*scale,cream);
    }
    return mesh;
}
static FfMesh FfBuildAimMesh(UINT width,UINT height){
    FfMesh mesh{};if(width<320||height<200)return mesh;
    const float scale=static_cast<float>(height)/1080.0f;
    const float x=static_cast<float>(width)*.5f,y=static_cast<float>(height)*.5f;
    const auto ink=D3DCOLOR_ARGB(180,15,14,12),cream=D3DCOLOR_ARGB(240,242,230,201);
    // Fine open-center sight with a dark outline for contrast over fire/daylight.
    const float bars[4][4]={{-11,-1,6,2},{5,-1,6,2},{-1,-11,2,6},{-1,5,2,6}};
    for(const auto& bar:bars){
        FfRect(mesh,x+(bar[0]-1)*scale,y+(bar[1]-1)*scale,(bar[2]+2)*scale,(bar[3]+2)*scale,ink);
        FfRect(mesh,x+bar[0]*scale,y+bar[1]*scale,bar[2]*scale,bar[3]*scale,cream);
    }
    return mesh;
}
static void HudStroke(FfMesh& mesh,float x,float y,float length,float thickness,bool vertical,D3DCOLOR color){
    const float h=thickness*.5f;
    const float p[6][2]={{0,h},{h,0},{length-h,0},{length,h},{length-h,thickness},{h,thickness}};
    for(unsigned n=1;n<5;++n){
        const auto px=[&](unsigned i){return x+(vertical?p[i][1]:p[i][0]);};
        const auto py=[&](unsigned i){return y+(vertical?p[i][0]:p[i][1]);};
        FfTriangle(mesh,px(0),py(0),px(n),py(n),px(n+1),py(n+1),color);
    }
}
static FfMesh FfBuildAmmoMesh(UINT width,UINT height,DWORD rounds){
    FfMesh mesh{};if(width<320||height<200||rounds>9999)return mesh;
    const float s=static_cast<float>(height)/1080.0f;
    const float right=width*.95f,bottom=height*.95f,x=right-170*s,y=bottom-68*s;
    const auto plate=D3DCOLOR_ARGB(170,15,14,12),edge=D3DCOLOR_ARGB(160,115,107,89);
    const auto accent=rounds?D3DCOLOR_ARGB(230,196,139,69):D3DCOLOR_ARGB(240,199,91,61);
    const auto text=D3DCOLOR_ARGB(248,242,230,201);
    const float cut=8*s;
    FfTriangle(mesh,x,y,right-cut,y,right,y+cut,plate);
    FfTriangle(mesh,x,y,right,y+cut,right,bottom,plate);
    FfTriangle(mesh,x,y,right,bottom,x+cut,bottom,plate);
    FfTriangle(mesh,x,y,x+cut,bottom,x,bottom-cut,plate);
    FfRect(mesh,x,y,2*s,48*s,accent);FfRect(mesh,x+8*s,y,35*s,s,edge);
    FfRect(mesh,right-40*s,bottom-s,40*s,s,edge);
    // A newly drawn cartridge silhouette; its aspect and geometry stay constant.
    FfRect(mesh,x+22*s,y+26*s,9*s,21*s,accent);
    FfTriangle(mesh,x+22*s,y+26*s,x+26.5f*s,y+18*s,x+31*s,y+26*s,accent);
    FfRect(mesh,x+20*s,y+48*s,13*s,2*s,accent);
    FfRect(mesh,x+43*s,y+19*s,s,31*s,edge);
    constexpr BYTE masks[10]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
    unsigned digits[4]{},count=0;DWORD remaining=rounds;
    do{digits[count++]=remaining%10;remaining/=10;}while(remaining&&count<4);
    if(count==1)digits[count++]=0;
    const float digitWidth=17*s,gap=7*s,t=2.5f*s,h=36*s;
    for(unsigned n=0;n<count;++n){
        const float dx=right-19*s-digitWidth-(digitWidth+gap)*static_cast<float>(n),dy=y+16*s;
        const unsigned bits=masks[digits[n]];
        const float segments[7][4]={{0,0,digitWidth,0},{digitWidth-t,0,h*.5f,1},
            {digitWidth-t,h*.5f,h*.5f,1},{0,h-t,digitWidth,0},{0,h*.5f,h*.5f,1},
            {0,0,h*.5f,1},{0,h*.5f-t*.5f,digitWidth,0}};
        for(unsigned k=0;k<7;++k)if(bits&(1u<<k))HudStroke(mesh,dx+segments[k][0],dy+segments[k][1],segments[k][2],t,segments[k][3]!=0,rounds?text:accent);
    }
    return mesh;
}
struct FfDevice {IDirect3DDevice9* device{};ULONGLONG generation{};UINT targets{},maxPrimitives{};bool usable{};};
static FfDevice ffDevices[8]{};
static SRWLOCK ffDeviceLock=SRWLOCK_INIT;
static ULONGLONG ffSerial=0;
static volatile LONG ffDrawing=0;
static FastForwardStateGetter ffGetter=nullptr;
static CombatAimStateGetter aimGetter=nullptr;
static WeaponHudStateGetter ammoGetter=nullptr;
static DWORD ammoNextLookup=0;
static DWORD aimNextLookup=0;
static DWORD ffNextLookup=0;
static SRWLOCK ffGetterLock=SRWLOCK_INIT;
static FfDevice* FfFind(IDirect3DDevice9* device) {for(auto& state:ffDevices)if(state.device==device)return &state;return nullptr;}
static void FfRegister(IDirect3DDevice9* device) {
    if(!ffOverlayEnabled&&!aimOverlayEnabled&&!ammoOverlayEnabled&&!favoritesOverlayEnabled)return;
    D3DCAPS9 caps{};const bool usable=SUCCEEDED(device->GetDeviceCaps(&caps)) && caps.NumSimultaneousRTs>0 && caps.NumSimultaneousRTs<=4;
    AcquireSRWLockExclusive(&ffDeviceLock);auto* state=FfFind(device);
    if(!state)for(auto& item:ffDevices)if(!item.device){state=&item;break;}
    if(state)*state={device,++ffSerial,caps.NumSimultaneousRTs,caps.MaxPrimitiveCount,usable};
    ReleaseSRWLockExclusive(&ffDeviceLock);
}
static ULONGLONG FfGeneration(IDirect3DDevice9* device) {
    AcquireSRWLockShared(&ffDeviceLock);const auto* state=FfFind(device);const auto generation=state?state->generation:0;ReleaseSRWLockShared(&ffDeviceLock);return generation;
}
static void FfForget(IDirect3DDevice9* device,ULONGLONG generation) {
    AcquireSRWLockExclusive(&ffDeviceLock);if(auto* state=FfFind(device))if(state->generation==generation)*state={};ReleaseSRWLockExclusive(&ffDeviceLock);
}
static void FfInvalidate(IDirect3DDevice9* device) {
    AcquireSRWLockExclusive(&ffDeviceLock);if(auto* state=FfFind(device))state->usable=false;ReleaseSRWLockExclusive(&ffDeviceLock);
}
static bool FfSameDirectory(HMODULE module) {
    wchar_t own[MAX_PATH]{},input[MAX_PATH]{};
    const DWORD ownLength=GetModuleFileNameW(selfModule,own,MAX_PATH);
    const DWORD inputLength=GetModuleFileNameW(module,input,MAX_PATH);
    if(!ownLength || ownLength>=MAX_PATH || !inputLength || inputLength>=MAX_PATH)return false;
    auto* a=wcsrchr(own,L'\\');auto* b=wcsrchr(input,L'\\');if(!a||!b)return false;*a=0;*b=0;
    return _wcsicmp(own,input)==0;
}
static FastForwardStateGetter FfResolveGetter() {
    if(!TryAcquireSRWLockExclusive(&ffGetterLock))return nullptr;
    const DWORD now=GetTickCount();
    if(!ffGetter && (!ffNextLookup || static_cast<LONG>(now-ffNextLookup)>=0)) {
        ffNextLookup=now+1000;
        const auto input=GetModuleHandleW(L"dinput8.dll");
        if(input && FfSameDirectory(input))ffGetter=reinterpret_cast<FastForwardStateGetter>(GetProcAddress(input,"AITDGetCutsceneFastForwardState"));
    }
    const auto result=ffGetter;ReleaseSRWLockExclusive(&ffGetterLock);return result;
}
static bool FfStateVisible(const FastForwardStateV1& state,DWORD now) {
    return state.size==sizeof(state) && state.version==1 && (state.flags&1)!=0 && state.rateQ16>65536 && state.rateQ16<=20*65536 && now-state.tick<=250;
}
static bool FfActive() {
    const auto getter=FfResolveGetter();FastForwardStateV1 state{};state.size=sizeof(state);state.version=1;
    return getter && getter(&state,sizeof(state)) && FfStateVisible(state,GetTickCount());
}
static bool AimStateVisible(const CombatAimStateV1& state,DWORD now){
    return state.size==sizeof(state)&&state.version==1&&(state.flags&1)!=0&&
        now-state.tick<100&&state.x==.5f&&state.y==.5f;
}
static bool AimActive(){
    if(!aimOverlayEnabled||!TryAcquireSRWLockExclusive(&ffGetterLock))return false;
    const DWORD now=GetTickCount();
    if(!aimGetter&&(!aimNextLookup||static_cast<LONG>(now-aimNextLookup)>=0)){
        aimNextLookup=now+1000;
        const auto input=GetModuleHandleW(L"dinput8.dll");
        if(input&&FfSameDirectory(input))aimGetter=reinterpret_cast<CombatAimStateGetter>(GetProcAddress(input,"AITDGetCombatAimState"));
    }
    const auto getter=aimGetter;ReleaseSRWLockExclusive(&ffGetterLock);
    CombatAimStateV1 state{};
    return getter&&getter(&state,sizeof(state))&&AimStateVisible(state,GetTickCount());
}
static bool AmmoStateVisible(const WeaponHudStateV1& state,DWORD now){
    return state.size==sizeof(state)&&state.version==1&&(state.flags&1)&&
        now-state.tick<=250&&state.weaponKind==1&&state.rounds<=9999;
}
static bool AmmoActive(WeaponHudStateV1* state){
    if(!ammoOverlayEnabled||!state||!TryAcquireSRWLockExclusive(&ffGetterLock))return false;
    const DWORD now=GetTickCount();
    if(!ammoGetter&&(!ammoNextLookup||static_cast<LONG>(now-ammoNextLookup)>=0)){
        ammoNextLookup=now+1000;const auto input=GetModuleHandleW(L"dinput8.dll");
        if(input&&FfSameDirectory(input))ammoGetter=reinterpret_cast<WeaponHudStateGetter>(GetProcAddress(input,"AITDGetWeaponHudState"));
    }
    const auto getter=ammoGetter;ReleaseSRWLockExclusive(&ffGetterLock);
    return getter&&getter(state,sizeof(*state))&&AmmoStateVisible(*state,now);
}
// Uses only a primary backbuffer already bound as sole render target. No RT/DS binding
// changes are made; uncertain/offscreen/MRT cases hide the indicator instead.
static bool FfDraw(IDirect3DDevice9* device,const FfDevice& metadata,bool aiming=false,const WeaponHudStateV1* ammo=nullptr) {
    IDirect3DSurface9 *back=nullptr,*target=nullptr,*extra=nullptr;
    IDirect3DStateBlock9* saved=nullptr;
    IDirect3DVertexBuffer9* stream=nullptr;
    IDirect3DIndexBuffer9* indices=nullptr;
    IDirect3DVertexDeclaration9* declaration=nullptr;
    D3DSURFACE_DESC description{};D3DVIEWPORT9 viewport{};RECT scissor{};
    UINT offset=0,stride=0,frequency=1;DWORD fvf=0;bool captured=false,scene=false,good=false,restore=true;
    do {
        if(FAILED(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back)) || !back || FAILED(back->GetDesc(&description)))break;
        if(FAILED(device->GetRenderTarget(0,&target)) || target!=back)break;
        bool multiple=false;
        for(UINT slot=1;slot<metadata.targets;++slot) {
            const HRESULT hr=device->GetRenderTarget(slot,&extra);
            if(extra){extra->Release();extra=nullptr;multiple=true;break;}
            if(FAILED(hr) && hr!=D3DERR_NOTFOUND){multiple=true;break;}
        }
        if(multiple)break;
        auto mesh=ammo?FfBuildAmmoMesh(description.Width,description.Height,ammo->rounds):
            (aiming?FfBuildAimMesh(description.Width,description.Height):FfBuildMesh(description.Width,description.Height));
        if(ammo&&aiming){const auto sight=FfBuildAimMesh(description.Width,description.Height);
            if(mesh.count+sight.count>ffMaxVertices)break;
            memcpy(mesh.vertices+mesh.count,sight.vertices,sight.count*sizeof(FfVertex));mesh.count+=sight.count;}
        if(!mesh.count || mesh.count/3>metadata.maxPrimitives)break;
        if(FAILED(device->GetViewport(&viewport)) || FAILED(device->GetScissorRect(&scissor)) ||
           FAILED(device->GetStreamSource(0,&stream,&offset,&stride)) || FAILED(device->GetStreamSourceFreq(0,&frequency)) ||
           FAILED(device->GetIndices(&indices)) || FAILED(device->GetVertexDeclaration(&declaration)) || FAILED(device->GetFVF(&fvf)))break;
        if(FAILED(device->CreateStateBlock(D3DSBT_ALL,&saved)) || !saved || FAILED(saved->Capture()))break;
        captured=true;
        if(FAILED(device->BeginScene()))break;
        scene=true;
        const D3DVIEWPORT9 ownViewport{0,0,description.Width,description.Height,0,1};
        HRESULT result=device->SetViewport(&ownViewport);
#define FF_TRY(call) if(SUCCEEDED(result))result=(call)
        FF_TRY(device->SetVertexShader(nullptr));FF_TRY(device->SetPixelShader(nullptr));
        FF_TRY(device->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE));FF_TRY(device->SetStreamSourceFreq(0,1));
        FF_TRY(device->SetTexture(0,nullptr));
        FF_TRY(device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1));FF_TRY(device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE));
        FF_TRY(device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1));FF_TRY(device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE));
        FF_TRY(device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE));
        const struct {D3DRENDERSTATETYPE key;DWORD value;} states[]={
            {D3DRS_ZENABLE,FALSE},{D3DRS_ZWRITEENABLE,FALSE},{D3DRS_STENCILENABLE,FALSE},{D3DRS_ALPHATESTENABLE,FALSE},
            {D3DRS_CULLMODE,D3DCULL_NONE},{D3DRS_LIGHTING,FALSE},{D3DRS_FOGENABLE,FALSE},{D3DRS_CLIPPLANEENABLE,0},
            {D3DRS_SCISSORTESTENABLE,FALSE},{D3DRS_COLORWRITEENABLE,15},{D3DRS_ALPHABLENDENABLE,TRUE},
            {D3DRS_SRCBLEND,D3DBLEND_SRCALPHA},{D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA},{D3DRS_BLENDOP,D3DBLENDOP_ADD},
            {D3DRS_SEPARATEALPHABLENDENABLE,FALSE},{D3DRS_FILLMODE,D3DFILL_SOLID},{D3DRS_SRGBWRITEENABLE,FALSE},
            {D3DRS_SHADEMODE,D3DSHADE_GOURAUD},{D3DRS_MULTISAMPLEMASK,0xffffffff}};
        for(const auto& state:states){FF_TRY(device->SetRenderState(state.key,state.value));}
        FF_TRY(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,mesh.count/3,mesh.vertices,sizeof(FfVertex)));
#undef FF_TRY
        good=SUCCEEDED(result);
    } while(false);
    if(scene && FAILED(device->EndScene()))restore=false;
    if(captured) {
        if(FAILED(saved->Apply()))restore=false;
        if(FAILED(device->SetViewport(&viewport)))restore=false;
        if(FAILED(device->SetScissorRect(&scissor)))restore=false;
        if(FAILED(device->SetStreamSource(0,stream,offset,stride)))restore=false;
        if(FAILED(device->SetStreamSourceFreq(0,frequency)))restore=false;
        if(FAILED(device->SetIndices(indices)))restore=false;
        if(FAILED(device->SetVertexDeclaration(declaration)))restore=false;
        if(fvf && FAILED(device->SetFVF(fvf)))restore=false;
    }
    if(saved)saved->Release();if(stream)stream->Release();if(indices)indices->Release();if(declaration)declaration->Release();
    if(extra)extra->Release();if(target)target->Release();if(back)back->Release();
    return restore && good;
}
#include "favorites_overlay.inl"
static void DrawFastForwardOverlay(IDirect3DDevice9* device,const RECT* source,const RECT* destination) {
    if(source||destination)return;
    const bool fastForward=ffOverlayEnabled&&FfActive(),aiming=!fastForward&&AimActive();
    WeaponHudStateV1 ammunition{};const bool ammo=!fastForward&&AmmoActive(&ammunition);
    FavoritesStateV1 favoritesState{};const bool favorites=favorites_overlay::State(favoritesState);
    if(!fastForward&&!aiming&&!ammo&&!favorites)return;
    if(InterlockedCompareExchange(&ffDrawing,1,0)!=0)return;
    FfDevice snapshot{};
    AcquireSRWLockShared(&ffDeviceLock);if(const auto* state=FfFind(device))snapshot=*state;ReleaseSRWLockShared(&ffDeviceLock);
    if(snapshot.usable && !(favorites?favorites_overlay::Draw(device,snapshot,favoritesState):FfDraw(device,snapshot,aiming,ammo?&ammunition:nullptr))) {
        FfInvalidate(device);
        Log("FF overlay disabled for device after draw/capture failure; next Reset may recover it");
    }
    InterlockedExchange(&ffDrawing,0);
}
