// Exact supported r29 native HUD path: 511990 -> 50CBA0 -> 595B40.
// Runs on the input/game thread, never from the renderer's draw callback.
namespace target_lock_projector {
enum Status : DWORD {None,Ready,SignatureMismatch,MissingWorld,MissingCamera,InvalidAnchor,Outside,Fault};
static void* anchorEntry=reinterpret_cast<void*>(0x50cba0);
static void* projectEntry=reinterpret_cast<void*>(0x595b40);
static void* authoredEntry=reinterpret_cast<void*>(0x4b3a40);
static bool Signatures(){
    static int result=0;
    if(!result){
        const BYTE anchor[]={0x55,0x8b,0xec,0x83,0xe4,0xf0,0x81,0xec,0x84,0,0,0};
        const BYTE project[]={0x55,0x8b,0xec,0x83,0xe4,0xf0,0x83,0xec,0x18,0x53,0x56};
        const BYTE authored[]={0x55,0x8b,0xec,0x83,0xe4,0xf0,0x83,0xec,0x50};
        result=!memcmp(anchorEntry,anchor,sizeof anchor)&&!memcmp(projectEntry,project,sizeof project)&&
            !memcmp(authoredEntry,authored,sizeof authored)?1:-1;
    }
    return result==1;
}
static __declspec(noinline) void AuthoredBone(unsigned char* actor,unsigned bone,float* output){
    // 4B3A40's real custom ABI, established by its native 4BDBA0 callers.
    __asm {
        mov esi,actor
        mov edi,output
        mov ecx,bone
        call dword ptr [authoredEntry]
    }
}
static constexpr DWORD NameHash(const char* name){
    // Exact pure 425EB0 algorithm; each byte lane adds modulo256.
    DWORD hash=0;for(unsigned n=0;name[n];++n){const unsigned shift=(n&3)*8;
        const DWORD lane=(((hash>>shift)&255)+static_cast<unsigned char>(name[n]))&255;
        hash=(hash&~(255u<<shift))|(lane<<shift);}
    return hash;
}
static int BoneIndex(const unsigned char* asset,DWORD hash){
    // Same sorted key/index tables as native94FF80. This reads the authored
    // model asset only; it never allocates or requests an animation pose.
    if(!asset)return -1;const unsigned count=*reinterpret_cast<const WORD*>(asset);
    if(!count||count>4096)return -1;
    const auto* keys=reinterpret_cast<const DWORD*>(asset+4+count*4);
    const auto* indices=reinterpret_cast<const DWORD*>(asset+4+count*8);
    unsigned lo=0,hi=count;while(lo<hi){const unsigned mid=lo+(hi-lo)/2;if(keys[mid]<hash)lo=mid+1;else hi=mid;}
    if(lo==count||keys[lo]!=hash||indices[lo]>=count)return -1;
    return static_cast<int>(indices[lo]);
}
static unsigned AuthoredFallback(unsigned char* actor,float* point){
    using namespace modern_combat;
    if(!ActorClass(actor,0x41434852))return 0;
    auto* model=Pointer(actor,0x3b4);if(!model||Pointer(model,0x1c))return 0;
    // Physics flags alone do not prove that its native anchor is usable. The
    // normal walking ANHZ also has3408=1/mode3. Native50CBA0 nevertheless falls
    // through to604180's zero point whenever model+1C is absent. Preserve any
    // meaningful native result, and replace only this proven origin fallback.
    float displacement=0;for(unsigned n=0;n<3;++n){const float d=point[n]-*reinterpret_cast<const float*>(actor+0x220+n*4);displacement+=d*d;}
    if(!std::isfinite(displacement)||displacement>1e-6f)return 0;
    const auto* asset=Pointer(model,0x10);
    int index=BoneIndex(asset,NameHash("NECK_1"));unsigned source=1;
    if(index<0){index=BoneIndex(asset,NameHash("PELVIS"));source=2;}
    if(index<0)return 0;
    AuthoredBone(actor,static_cast<unsigned>(index),point);return source;
}
static __declspec(noinline) void Anchor(unsigned char* actor,float* output){
    __asm {
        mov eax,actor
        mov ecx,output
        call dword ptr [anchorEntry]
    }
}
static __declspec(noinline) bool Project(const float* point,float* x,float* y){
    unsigned result=0;
    __asm {
        mov edi,point
        push 0
        push y
        push x
        call dword ptr [projectEntry]
        movzx eax,al
        mov result,eax
    }
    return result!=0;
}
static bool Capture(unsigned char* actor,TargetLockStateV2& state){
    state.projectionStatus=Fault;
    __try {
        if(!Signatures()){state.projectionStatus=SignatureMismatch;return false;}
        // 595760 explicitly selects the main native graphics world's camera,
        // rather than the current GPU view (which may belong to UI or shadows).
        auto* world=*reinterpret_cast<unsigned char**>(0xff7164);
        if(!world){state.projectionStatus=MissingWorld;return false;}
        auto* camera=*reinterpret_cast<unsigned char**>(world+0x60);
        if(!camera){state.projectionStatus=MissingCamera;return false;}
        alignas(16) float point[4]{};Anchor(actor,point);
        // Some live actors have no model+1C CPU pose. Native604180 then returns
        // zero and 50CBA0 degenerates to the feet. The native authored-bone path
        // 4B3A40 -> 603F50 reads the real model's bind transform instead. Keep
        // animated/ragdoll native anchors whenever their pose is available.
        state.padding=AuthoredFallback(actor,point); // diagnostic:0native,1neck,2pelvis
        for(float value:point)if(!std::isfinite(value)||fabsf(value)>1000000.f){state.projectionStatus=InvalidAnchor;return false;}
        // 50CBA0 chooses NECK_1, otherwise PELVIS, with the native ragdoll and
        // authored bone transform paths. Non-ACHR actors retain native origin.
        for(unsigned n=0;n<3;++n)state.world[n]=point[n];
        float x=0,y=0;
        const bool visible=Project(point,&x,&y);
        if(!visible||!std::isfinite(x)||!std::isfinite(y)||x<=0||x>=1||y<=0||y>=1){state.projectionStatus=Outside;return false;}
        if(*reinterpret_cast<unsigned char**>(0xff7164)!=world||*reinterpret_cast<unsigned char**>(world+0x60)!=camera){state.projectionStatus=MissingCamera;return false;}
        state.screenX=x;state.screenY=y;state.projectionTick=GetTickCount();state.camera=reinterpret_cast<DWORD>(camera);
        state.projectionStatus=Ready;return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){state.projectionStatus=Fault;return false;}
}
}
