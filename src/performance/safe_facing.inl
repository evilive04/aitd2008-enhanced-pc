// r29 native angular motor and the game's complete ResetCloth script adapter.
// Call only from an authenticated ordinary hero-input owner on the game thread.
namespace safe_facing {
static unsigned char* lastHero=nullptr;
static unsigned char* lastNode=nullptr;
static unsigned char* lastCloth=nullptr;
static unsigned char* lastSecondary=nullptr;
static float lastYaw=0;
static DWORD lastTick=0;
static volatile LONG requests=0,clothResets=0,rejections=0;
static bool Reject(){
    lastHero=nullptr;lastNode=nullptr;lastCloth=nullptr;lastSecondary=nullptr;lastTick=0;
    InterlockedIncrement(&rejections);return false;
}
static __declspec(noinline) bool Discontinuous(float actual,float requested,float previous){
    return std::fabs(std::remainder(requested-actual,6.28318530718f))>1.04719755f&&
        std::fabs(std::remainder(requested-previous,6.28318530718f))>1.04719755f;
}
static __declspec(noinline) bool ClothOwned(unsigned char* hero,unsigned char* cloth,unsigned char* secondary){
    if(cloth&&(*reinterpret_cast<DWORD*>(cloth)!=0xc70a08||
        *reinterpret_cast<unsigned char**>(cloth+0x5d0)!=hero||
        *reinterpret_cast<unsigned*>(cloth+0xd0)!=48))return false;
    return !secondary||(*reinterpret_cast<DWORD*>(secondary)==0xc711c4&&
        *reinterpret_cast<unsigned char**>(secondary+0x14)==hero&&
        *reinterpret_cast<unsigned*>(secondary+0x10)<=2);
}
static __declspec(noinline) void ResetCloth(unsigned char* hero){
    unsigned char* actor=hero;
    DWORD result[4]{}; // Native output cursor+8 equals end+C: empty script return.
    using Native=DWORD(__thiscall*)(unsigned char**,DWORD*);
    reinterpret_cast<Native>(0x446990)(&actor,result);
    InterlockedIncrement(&clothResets);
}
static __declspec(noinline) bool Turn(unsigned char* hero,unsigned char* node,const float* direction,float step=-1.f){
    if(!hero||!node||!direction)return Reject();
    __try {
        static const unsigned char resetSignature[]={0x8b,0x01,0x8b,0x88,0x10,0x05,0,0,0x85,0xc9,0x74,0x07,0xc6,0x81,0xdc,0};
        static const unsigned char turnSignature[]={0x55,0x8b,0xec,0x83,0xe4,0xf0,0x83,0xec,0x70,0x8b,0x06,0x8b,0x50,0x40,0x8d,0x4c};
        if(*reinterpret_cast<unsigned char**>(0xfd61ec)!=hero||
            *reinterpret_cast<unsigned char**>(hero+0x20)!=node||
            std::memcmp(reinterpret_cast<void*>(0x446990),resetSignature,sizeof(resetSignature))||
            std::memcmp(reinterpret_cast<void*>(0x571ef0),turnSignature,sizeof(turnSignature)))return Reject();
        const float x=direction[0],z=direction[2];
        const float fx=*reinterpret_cast<float*>(hero+0x210),fz=*reinterpret_cast<float*>(hero+0x218);
        const float dt=step<0?*reinterpret_cast<float*>(0xf503c8):step;
        if(!std::isfinite(x)||!std::isfinite(z)||!std::isfinite(fx)||!std::isfinite(fz)||
            x*x+z*z<.000001f||fx*fx+fz*fz<.000001f||!std::isfinite(dt)||dt<=0||dt>.1f)return Reject();
        auto* cloth=*reinterpret_cast<unsigned char**>(hero+0x510);
        auto* secondary=*reinterpret_cast<unsigned char**>(hero+0x50c);
        if(!ClothOwned(hero,cloth,secondary))return Reject();
        const float actual=std::atan2(fx,fz),requested=std::atan2(x,z);
        const DWORD now=GetTickCount();
        const bool same=hero==lastHero&&node==lastNode&&cloth==lastCloth&&secondary==lastSecondary&&now-lastTick<=100;
        if((cloth||secondary)&&Discontinuous(actual,requested,same?lastYaw:actual))ResetCloth(hero);
        // 180 degrees takes at most 79ms. Native angular motor, timestep clamp and
        // tolerance still own the transform; no teleport, skeleton or particle edit.
        const float speed=40.f,elapsed=step,tolerance=.0174532925f;
        __asm {
            push tolerance
            push elapsed
            push speed
            mov esi,node
            mov edi,direction
            mov eax,571ef0h
            call eax
        }
        lastHero=hero;lastNode=node;lastCloth=cloth;lastSecondary=secondary;lastYaw=requested;lastTick=now;
        InterlockedIncrement(&requests);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return Reject();}
}
}
