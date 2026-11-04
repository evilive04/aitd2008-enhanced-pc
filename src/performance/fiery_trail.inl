// Fire-only visual WORLD spawn-frame alignment and scoped diagnostics.
// Include after ballistic_aim.inl. Already-correct physical shots stay untouched.
#include "fiery_trail.h"
namespace fiery_trail {
static bool enabled=false,correctionEnabled=false;
static constexpr LONG logShotLimit=3; // One bounded user-shot diagnosis, not a per-frame capture.
static volatile LONG muzzleCorrected=0,worldUpdated=0,worldObserved=0,worldProbed=0,worldUnowned=0;
static void* updateContinuation=reinterpret_cast<void*>(0x639808);
static void* spawnContinuation=reinterpret_cast<void*>(0x6395af);
static volatile LONG installation=0,sampleSequence=0,captured=0,emitterCaptured=0;
static void* shotContinuation=reinterpret_cast<void*>(0x6ff5eb);
static void* nativeEmitter=reinterpret_cast<void*>(0x701740);
static void* muzzleContinuation=reinterpret_cast<void*>(0x6fe89e);
struct Sample {
    DWORD tick{},hero{},weapon{},effect{},trail{},queryHitValid{},geometryValid{},emitterTick{},particleActor{},particleSystem{};
    DWORD particleTemplate{},particleResource{};
    DWORD muzzleTick{},behavior{},muzzleActors[10]{},muzzleConfig[24]{};
    float nativeOrigin[4]{},nativeEnd[4]{},queryOrigin[4]{},queryDirection[4]{},queryHit[4]{};
    float actualOrigin[4]{},actualDirection[4]{},actualLength{},actualEnd[4]{};
    float physicalTarget[4]{},nativeAimAfterShot[4]{},particleMatrix[16]{};
    float endpointToHit{},nativeEndpointToHit{},endpointToRequested{};
};
static Sample lastSample{};
static SRWLOCK sampleLock=SRWLOCK_INIT;
static Sample Snapshot(){AcquireSRWLockShared(&sampleLock);const auto value=lastSample;ReleaseSRWLockShared(&sampleLock);return value;}
static bool Publish(const Sample& value,unsigned updatePart){
    AcquireSRWLockExclusive(&sampleLock);
    const bool okay=!updatePart||(lastSample.tick==value.tick&&lastSample.weapon==value.weapon&&lastSample.trail==value.trail);
    if(okay){
        InterlockedIncrement(&sampleSequence);
        if(!updatePart)lastSample=value;
        else if(updatePart==1){
            lastSample.behavior=value.behavior;lastSample.muzzleTick=value.muzzleTick;
            memcpy(lastSample.muzzleActors,value.muzzleActors,sizeof(value.muzzleActors));
            memcpy(lastSample.muzzleConfig,value.muzzleConfig,sizeof(value.muzzleConfig));
        }else{
            lastSample.emitterTick=value.emitterTick;lastSample.particleActor=value.particleActor;lastSample.particleSystem=value.particleSystem;
            memcpy(lastSample.particleMatrix,value.particleMatrix,sizeof(value.particleMatrix));
        }
        InterlockedIncrement(&sampleSequence);
    }
    ReleaseSRWLockExclusive(&sampleLock);return okay;
}
static void __cdecl CaptureShot(unsigned char* weapon,const float* origin,const float* endpoint,unsigned char* effect){
    __try {
        if((!enabled&&!correctionEnabled)||!weapon||effect!=weapon+0x7a0||!ballistic_aim::enabled)return;
        const auto physical=ballistic_aim::lastSample;
        if(physical.reason!=static_cast<DWORD>(ballistic_aim::Result::Applied)||physical.weapon!=reinterpret_cast<DWORD>(weapon)||
           GetTickCount()-physical.tick>100||physical.hero!=*reinterpret_cast<DWORD*>(0xfd61ec))return;
        Sample s{};s.tick=GetTickCount();s.hero=physical.hero;s.weapon=physical.weapon;s.effect=reinterpret_cast<DWORD>(effect);
        memcpy(s.nativeOrigin,origin,16);memcpy(s.nativeEnd,endpoint,16);
        auto* query=weapon+0xa80;s.queryHitValid=query[0x720];
        memcpy(s.queryOrigin,query+0x10,16);memcpy(s.queryDirection,query+0x20,16);memcpy(s.queryHit,query+0x600,16);
        memcpy(s.physicalTarget,physical.target,16);memcpy(s.nativeAimAfterShot,weapon+0xa60,16);
        auto* trail=*reinterpret_cast<unsigned char**>(0x10734a0);
        // 7023B0 allocates at list head. Reject an old/unrelated head if no
        // new segment was allocated (missing config, tiny segment, exhausted pool).
        if(trail&&*reinterpret_cast<unsigned char**>(trail+0x30)==effect&&memcmp(trail,origin,16)==0){
            s.trail=reinterpret_cast<DWORD>(trail);
            memcpy(s.actualOrigin,trail,16);memcpy(s.actualDirection,trail+0x10,16);s.actualLength=*reinterpret_cast<float*>(trail+0x20);
            s.geometryValid=Endpoint(s.actualOrigin,s.actualDirection,s.actualLength,s.actualEnd)?1u:0u;
        }
        s.particleTemplate=*reinterpret_cast<DWORD*>(effect+0x118);
        s.particleResource=*reinterpret_cast<DWORD*>(effect+0x120);
        s.nativeEndpointToHit=s.queryHitValid?Distance3(s.nativeEnd,s.queryHit):-1.0f;
        s.endpointToHit=s.geometryValid&&s.queryHitValid?Distance3(s.actualEnd,s.queryHit):-1.0f;
        s.endpointToRequested=s.geometryValid?Distance3(s.actualEnd,s.nativeEnd):-1.0f;
        Publish(s,0);
        const LONG count=InterlockedIncrement(&captured);
        if(count<=logShotLimit)Log("Fiery trail shot=%ld weapon=%08lx trail=%08lx nativeEnd=(%.6f,%.6f,%.6f) queryHitValid=%lu queryHit=(%.6f,%.6f,%.6f) actualEnd=(%.6f,%.6f,%.6f) endHitError=%.7f nativeHitError=%.7f geometry=%lu particleTemplate=%08lx resource=%08lx",
            count,s.weapon,s.trail,s.nativeEnd[0],s.nativeEnd[1],s.nativeEnd[2],s.queryHitValid,s.queryHit[0],s.queryHit[1],s.queryHit[2],
            s.actualEnd[0],s.actualEnd[1],s.actualEnd[2],s.endpointToHit,s.nativeEndpointToHit,s.geometryValid,s.particleTemplate,s.particleResource);
        if(count<=logShotLimit){float originError{};const bool valid=AxisDistance(s.actualOrigin,s.queryOrigin,s.queryDirection,&originError);
            Log("Fiery path origins shot=%ld trail=%08lx nativeOrigin=(%.7f,%.7f,%.7f) visualOrigin=(%.7f,%.7f,%.7f) queryOrigin=(%.7f,%.7f,%.7f) queryDirection=(%.7f,%.7f,%.7f) visualDirection=(%.7f,%.7f,%.7f) fullLength=%.7f originAxisValid=%u originAxisError=%.7f",
                count,s.trail,s.nativeOrigin[0],s.nativeOrigin[1],s.nativeOrigin[2],s.actualOrigin[0],s.actualOrigin[1],s.actualOrigin[2],s.queryOrigin[0],s.queryOrigin[1],s.queryOrigin[2],s.queryDirection[0],s.queryDirection[1],s.queryDirection[2],s.actualDirection[0],s.actualDirection[1],s.actualDirection[2],s.actualLength,valid?1u:0u,valid?originError:-1.0f);}
    }__except(EXCEPTION_EXECUTE_HANDLER){}
}
static void __cdecl CaptureMuzzle(unsigned char* behavior){
    __try {
        const auto current=Snapshot();
        if(!enabled||!behavior||!current.weapon||GetTickCount()-current.tick>100||
           *reinterpret_cast<DWORD*>(behavior+0x20)!=current.weapon||
           reinterpret_cast<DWORD>(behavior)!=ballistic_aim::lastSample.behavior)return;
        auto updated=current;updated.behavior=reinterpret_cast<DWORD>(behavior);updated.muzzleTick=GetTickCount();
        // 700720 fire-ammo branch selects behavior+180; records its original
        // spawn configuration and the resulting ten native FX actor references.
        memcpy(updated.muzzleConfig,behavior+0x180,sizeof(updated.muzzleConfig));
        memcpy(updated.muzzleActors,behavior+0x320,sizeof(updated.muzzleActors));
        if(!Publish(updated,1))return;
        if(captured<=logShotLimit)Log("Fiery muzzle effect behavior=%08lx config=%08lx firstActors=(%08lx,%08lx,%08lx) localPosition=(%.6f,%.6f,%.6f)",
            updated.behavior,*reinterpret_cast<const DWORD*>(behavior+0x188),updated.muzzleActors[0],updated.muzzleActors[1],updated.muzzleActors[2],
            reinterpret_cast<const float*>(behavior+0x180)[16],reinterpret_cast<const float*>(behavior+0x180)[17],reinterpret_cast<const float*>(behavior+0x180)[18]);
    }__except(EXCEPTION_EXECUTE_HANDLER){}
}
static void __cdecl CaptureEmitter(unsigned char* trail,const float* matrix){
    __try {
        const auto current=Snapshot();
        if(!enabled||!trail||current.trail!=reinterpret_cast<DWORD>(trail)||!matrix||
           GetTickCount()-current.tick>2000||current.emitterTick)return;
        const auto* effect=reinterpret_cast<unsigned char*>(current.effect);
        if(*reinterpret_cast<unsigned char**>(trail+0x30)!=effect)return;
        auto updated=current;
        memcpy(updated.particleMatrix,matrix,64);
        updated.particleActor=*reinterpret_cast<DWORD*>(trail+0x28);
        updated.particleSystem=*reinterpret_cast<DWORD*>(trail+0x2c);
        updated.emitterTick=GetTickCount();
        if(!Publish(updated,2))return;
        if(InterlockedIncrement(&emitterCaptured)>logShotLimit)return;
        Log("Fiery trail emitter trail=%08lx actor=%08lx system=%08lx matrix=(%.6f,%.6f,%.6f,%.6f; %.6f,%.6f,%.6f,%.6f; %.6f,%.6f,%.6f,%.6f; %.6f,%.6f,%.6f,%.6f)",
            updated.trail,updated.particleActor,updated.particleSystem,matrix[0],matrix[1],matrix[2],matrix[3],
            matrix[4],matrix[5],matrix[6],matrix[7],matrix[8],matrix[9],matrix[10],matrix[11],matrix[12],matrix[13],matrix[14],matrix[15]);
        const float travel=*reinterpret_cast<const float*>(trail+0x40),maximum=*reinterpret_cast<const float*>(effect+0x128);
        float head[3]{},tail[3]{},length{},headQuery{},tailQuery{},headVisual{},tailVisual{};
        const bool valid=StreakCenters(matrix,travel,maximum,head,tail,&length)&&
            AxisDistance(head,current.queryOrigin,current.queryDirection,&headQuery)&&AxisDistance(tail,current.queryOrigin,current.queryDirection,&tailQuery)&&
            AxisDistance(head,current.actualOrigin,current.actualDirection,&headVisual)&&AxisDistance(tail,current.actualOrigin,current.actualDirection,&tailVisual);
        if(valid)Log("Fiery streak centers trail=%08lx travel=%.7f stripLength=%.7f maximum=%.7f near=(%.7f,%.7f,%.7f) far=(%.7f,%.7f,%.7f) queryAxisError=(%.7f,%.7f) visualAxisError=(%.7f,%.7f) materials=(%08lx,%08lx,%08lx,%08lx)",
            current.trail,travel,length,maximum,head[0],head[1],head[2],tail[0],tail[1],tail[2],headQuery,tailQuery,headVisual,tailVisual,
            *reinterpret_cast<const DWORD*>(effect+0x1c),*reinterpret_cast<const DWORD*>(effect+0x5c),*reinterpret_cast<const DWORD*>(effect+0x9c),*reinterpret_cast<const DWORD*>(effect+0xdc));
        else Log("Fiery streak centers invalid trail=%08lx travel=%.7f maximum=%.7f; no finite centerline claim",current.trail,travel,maximum);
    }__except(EXCEPTION_EXECUTE_HANDLER){}
}

// A native FX context immediately recomputes its transform in639750, including
// once inside639480 itself. Retain only authenticated fire-shot ownership so
// that the corresponding final WORLD update receives the same rigid correction.
struct OwnedMuzzle {
    DWORD context{},actor{},identity{},hero{},weapon{},behavior{};
    float target[4]{},localForward[3]{};
    bool fixedShotPose{};
    float shotPose[16]{};
};
static OwnedMuzzle ownedMuzzles[32]{};
static unsigned ownedCursor=0;
static SRWLOCK ownedLock=SRWLOCK_INIT;
static void ForgetMuzzle(DWORD context){
    AcquireSRWLockExclusive(&ownedLock);
    for(auto& own:ownedMuzzles)if(own.context==context)own={};
    ReleaseSRWLockExclusive(&ownedLock);
}
static void RememberMuzzle(const OwnedMuzzle& own){
    AcquireSRWLockExclusive(&ownedLock);
    for(auto& old:ownedMuzzles)if(old.context==own.context)old={};
    ownedMuzzles[ownedCursor++%32]=own;
    ReleaseSRWLockExclusive(&ownedLock);
}
static OwnedMuzzle FindMuzzle(DWORD context){
    OwnedMuzzle result{};AcquireSRWLockShared(&ownedLock);
    for(const auto& own:ownedMuzzles)if(own.context==context){result=own;break;}
    ReleaseSRWLockShared(&ownedLock);return result;
}
// Authenticated Weapon_Firebullet_Glock GROUP and its two observed particle
// resources only. Other weapons, arbitrary GROUPs and unknown resource versions
// retain the preceding marker-frame policy. All reads are under the caller SEH.
static bool GlockAuthoredLine(DWORD actor,float* nearPoint,float* farPoint){
    if(!actor||*reinterpret_cast<const DWORD*>(actor)!=0xccf31c)return false;
    const auto* root=*reinterpret_cast<const unsigned char* const*>(actor+0xa4);
    if(!root||*reinterpret_cast<const DWORD*>(root+0x48)!=0xe1bb7853||
       *reinterpret_cast<const DWORD*>(root+0x40)!=0x99ce10d1||*reinterpret_cast<const DWORD*>(root+0x44)!=0x0814af22||
       *reinterpret_cast<const DWORD*>(root+0x94)!=3)return false;
    const auto* entries=*reinterpret_cast<const unsigned char* const*>(root+0x98);
    if(!entries)return false;
    const unsigned char* trailResource=nullptr;DWORD trailBytes=0;bool canon=false,trail=false,sound=false;
    for(unsigned index=0;index<3;++index){
        const auto* child=*reinterpret_cast<const unsigned char* const*>(entries+index*12);
        if(!child)return false;
        const DWORD low=*reinterpret_cast<const DWORD*>(child+0x40),high=*reinterpret_cast<const DWORD*>(child+0x44);
        const DWORD type=*reinterpret_cast<const DWORD*>(child+0x48);
        if(type==0x13f6958c&&low==0x8ac25b74&&high==0xc9090c9c){if(sound)return false;sound=true;continue;}
        if(type!=0xf71d732e)return false;
        const DWORD prtLow=*reinterpret_cast<const DWORD*>(child+0x90),prtHigh=*reinterpret_cast<const DWORD*>(child+0x94);
        const auto* resource=*reinterpret_cast<const unsigned char* const*>(child+0x98);
        const DWORD bytes=*reinterpret_cast<const DWORD*>(child+0x9c);
        if(low==0x409e879d&&high==0xd01c6156&&prtLow==0x92596277&&prtHigh==0x07422131&&bytes==9520){
            if(canon||!resource||*reinterpret_cast<const DWORD*>(resource+8)!=bytes||
               *reinterpret_cast<const DWORD*>(resource+12)!=0x5452502e||*reinterpret_cast<const DWORD*>(resource+16)!=0x5452502e)return false;
            canon=true;
        }else if(low==0x03e01e0d&&high==0x3445775b&&prtLow==0x7f5bf5af&&prtHigh==0x8ffaa620&&bytes==57040){
            if(trail)return false;trail=true;trailResource=resource;trailBytes=bytes;
        }else return false;
    }
    return canon&&trail&&sound&&GlockTrailLine(trailResource,trailBytes,nearPoint,farPoint);
}
// Only temporary WORLD visual matrices change; physical query/damage, weapon
// attachment pose and original emitter-local coordinates remain native.
static bool __cdecl CorrectMuzzle(unsigned char* behavior,unsigned char* config,float* matrix,OwnedMuzzle* ownership=nullptr){
    __try{
        if(!correctionEnabled||!behavior||config!=behavior+0x180||!matrix||!ballistic_aim::enabled)return false;
        const auto s=Snapshot();const auto physical=ballistic_aim::lastSample;
        const bool measured=s.geometryValid&&s.endpointToRequested>=0&&s.endpointToRequested<=.001f&&
            (!s.queryHitValid||(s.endpointToHit>=0&&s.endpointToHit<=.001f));
        if(!s.weapon||GetTickCount()-s.tick>100||!measured||
           s.hero!=*reinterpret_cast<DWORD*>(0xfd61ec)||physical.hero!=s.hero||physical.weapon!=s.weapon||
           physical.reason!=static_cast<DWORD>(ballistic_aim::Result::Applied)||GetTickCount()-physical.tick>100||
           physical.behavior!=reinterpret_cast<DWORD>(behavior)||*reinterpret_cast<DWORD*>(behavior+0x20)!=s.weapon)return false;
        float revised[16],localForward[3];memcpy(revised,matrix,sizeof(revised));
        float nearPoint[3]{},farPoint[3]{};
        if(ownership&&GlockAuthoredLine(ownership->actor,nearPoint,farPoint)){
            if(!AimAuthoredLine(revised,nearPoint,farPoint,s.actualOrigin,s.actualEnd))return false;
            memcpy(matrix,revised,sizeof(revised));ownership->hero=s.hero;ownership->weapon=s.weapon;
            ownership->behavior=reinterpret_cast<DWORD>(behavior);ownership->fixedShotPose=true;
            memcpy(ownership->target,s.actualEnd,16);memcpy(ownership->shotPose,revised,sizeof(revised));
            const LONG count=InterlockedIncrement(&muzzleCorrected);
            if(count<=logShotLimit){float a[3]{},b[3]{},nearError=-1,farError=-1;
                if(FrameWorldDirection(revised,nearPoint,a)&&FrameWorldDirection(revised,farPoint,b)){
                    for(unsigned i=0;i<3;++i){a[i]+=revised[12+i];b[i]+=revised[12+i];}
                    AxisDistance(a,s.queryOrigin,s.queryDirection,&nearError);AxisDistance(b,s.queryOrigin,s.queryDirection,&farError);
                }
                Log("Fiery authored Glock line aligned=%ld origin=(%.6f,%.6f,%.6f) hit=(%.6f,%.6f,%.6f) authoredNear=(%.6f,%.6f,%.6f) authoredFar=(%.6f,%.6f,%.6f) queryAxisError=(%.7f,%.7f); rigid fixed single-shot pose",count,s.actualOrigin[0],s.actualOrigin[1],s.actualOrigin[2],s.actualEnd[0],s.actualEnd[1],s.actualEnd[2],a[0],a[1],a[2],b[0],b[1],b[2],nearError,farError);
            }
            return true;
        }
        if(!FrameLocalDirection(matrix,s.nativeAimAfterShot,localForward))return false;
        //6FEDF0 supplies the actual world direction between authored gun markers;
        //6FF3D0 saves it to weapon+A60 before CaptureShot. Rotating from that
        // direction preserves the resource's authored particle-axis convention.
        if(!AimEmitterFromDirection(revised,s.nativeAimAfterShot,s.nativeEnd))return false;
        memcpy(matrix,revised,sizeof(revised));
        if(ownership){ownership->hero=s.hero;ownership->weapon=s.weapon;ownership->behavior=reinterpret_cast<DWORD>(behavior);
            memcpy(ownership->target,s.nativeEnd,16);memcpy(ownership->localForward,localForward,sizeof(localForward));}
        const LONG count=InterlockedIncrement(&muzzleCorrected);
        if(count<=logShotLimit)Log("Fiery visual muzzle aligned=%ld origin=(%.6f,%.6f,%.6f) nativeForward=(%.6f,%.6f,%.6f) hit=(%.6f,%.6f,%.6f); phase=world-fx marker-delta physical query unchanged",count,matrix[12],matrix[13],matrix[14],s.nativeAimAfterShot[0],s.nativeAimAfterShot[1],s.nativeAimAfterShot[2],s.nativeEnd[0],s.nativeEnd[1],s.nativeEnd[2]);
        return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static void __cdecl CorrectWorldMuzzle(unsigned char* context,float* matrix,unsigned char* frame){
    __try {
        if(!correctionEnabled||!context||!matrix||!frame)return;
        if(enabled){const auto sample=Snapshot();if(sample.weapon&&GetTickCount()-sample.tick<=100&&
           *reinterpret_cast<DWORD*>(context+0x98)==sample.weapon&&InterlockedIncrement(&worldProbed)<=logShotLimit){
            auto* parent=*reinterpret_cast<unsigned char**>(frame);
            Log("Fiery world provenance context=%08lx return=%08lx parentReturn=%08lx source=%08lx origin=(%.6f,%.6f,%.6f)",reinterpret_cast<DWORD>(context),*reinterpret_cast<DWORD*>(frame+4),parent?*reinterpret_cast<DWORD*>(parent+4):0,*reinterpret_cast<DWORD*>(context+0x98),matrix[12],matrix[13],matrix[14]);}}
        if(*reinterpret_cast<const DWORD*>(frame+4)!=0x4799cc)return;
        auto* actorFrame=*reinterpret_cast<unsigned char**>(frame);
        if(!actorFrame||*reinterpret_cast<const DWORD*>(actorFrame+4)!=0x7008c6)return;
        // A later ordinary shot using this pooled context must erase ownership.
        ForgetMuzzle(reinterpret_cast<DWORD>(context));
        auto* muzzleFrame=*reinterpret_cast<unsigned char**>(actorFrame);
        if(!muzzleFrame)return;
        auto* behavior=*reinterpret_cast<unsigned char**>(muzzleFrame+8);
        auto* weapon=behavior?*reinterpret_cast<unsigned char**>(behavior+0x20):nullptr;
        const bool fire=muzzleFrame[0xc]!=0;
        if(enabled&&InterlockedIncrement(&worldObserved)<=logShotLimit)Log("Fiery world creation context=%08lx fire=%u weapon=%08lx source=%08lx actor=%08lx identity=%08lx origin=(%.6f,%.6f,%.6f)",reinterpret_cast<DWORD>(context),fire?1u:0u,reinterpret_cast<DWORD>(weapon),*reinterpret_cast<DWORD*>(context+0x98),*reinterpret_cast<DWORD*>(context+0xbc),*reinterpret_cast<DWORD*>(context+0xb8),matrix[12],matrix[13],matrix[14]);
        if(!fire||!weapon||*reinterpret_cast<unsigned char**>(context+0x98)!=weapon)return;
        OwnedMuzzle own{};own.context=reinterpret_cast<DWORD>(context);
        own.actor=*reinterpret_cast<DWORD*>(context+0xbc);own.identity=*reinterpret_cast<DWORD*>(context+0xb8);
        if(!own.actor||!own.identity||*reinterpret_cast<DWORD*>(own.actor+0xf8)!=own.identity)return;
        if(CorrectMuzzle(behavior,behavior+0x180,matrix,&own))RememberMuzzle(own);
        else if(enabled&&worldObserved<=logShotLimit){const auto s=Snapshot();const auto physical=ballistic_aim::lastSample;
            Log("Fiery world creation rejected context=%08lx sampleWeapon=%08lx sampleBehavior=%08lx physicalBehavior=%08lx reason=%lu geometry=%lu requestError=%.7f hitError=%.7f age=%lu nativeForward=(%.6f,%.6f,%.6f)",own.context,s.weapon,s.behavior,physical.behavior,physical.reason,s.geometryValid,s.endpointToRequested,s.endpointToHit,GetTickCount()-s.tick,s.nativeAimAfterShot[0],s.nativeAimAfterShot[1],s.nativeAimAfterShot[2]);}
    }__except(EXCEPTION_EXECUTE_HANDLER){}
}
//6395A7 is subtype-conditional. Dynamic effects are owned/corrected only in
// the guaranteed immediate639750 update instead. A native static context skips
//639750 entirely (94bit1), so only that case uses the earlier creation frame.
static void __cdecl CorrectStaticWorldMuzzle(unsigned char* context,float* matrix,unsigned char* frame){
    __try {if(context&&(*reinterpret_cast<DWORD*>(context+0x94)&1))CorrectWorldMuzzle(context,matrix,frame);}
    __except(EXCEPTION_EXECUTE_HANDLER){}
}
static void __cdecl CorrectWorldUpdate(unsigned char* context,float* matrix,unsigned char* frame){
    __try {
        if(!correctionEnabled||!context||!matrix||!frame||!ballistic_aim::enabled)return;
        //639480 unconditionally calls639750 at6396B9. Authenticate that exact
        // immediate creation update, then reuse the original4799CC->7008C6
        // source/fire provenance. Later updater calls never acquire ownership.
        if(*reinterpret_cast<DWORD*>(frame+4)==0x6396be){
            auto* creationFrame=*reinterpret_cast<unsigned char**>(frame);
            CorrectWorldMuzzle(context,matrix,creationFrame);
            const auto created=FindMuzzle(reinterpret_cast<DWORD>(context));
            if(enabled&&created.context&&InterlockedIncrement(&worldUpdated)<=logShotLimit)
                Log("Fiery world update creation context=%08lx actor=%08lx identity=%08lx origin=(%.6f,%.6f,%.6f) target=(%.6f,%.6f,%.6f) basis=(%.6f,%.6f,%.6f;%.6f,%.6f,%.6f;%.6f,%.6f,%.6f)",created.context,created.actor,created.identity,matrix[12],matrix[13],matrix[14],created.target[0],created.target[1],created.target[2],matrix[0],matrix[1],matrix[2],matrix[4],matrix[5],matrix[6],matrix[8],matrix[9],matrix[10]);
            return;
        }
        const auto own=FindMuzzle(reinterpret_cast<DWORD>(context));
        if(!own.context){if(enabled){const auto s=Snapshot();if(s.weapon&&GetTickCount()-s.tick<=100&&
            *reinterpret_cast<DWORD*>(context+0x98)==s.weapon&&InterlockedIncrement(&worldUnowned)<=logShotLimit)
            Log("Fiery world update unowned context=%08lx actor=%08lx identity=%08lx return=%08lx parentReturn=%08lx origin=(%.6f,%.6f,%.6f)",reinterpret_cast<DWORD>(context),*reinterpret_cast<DWORD*>(context+0xbc),*reinterpret_cast<DWORD*>(context+0xb8),*reinterpret_cast<DWORD*>(frame+4),*reinterpret_cast<DWORD*>(frame)?*reinterpret_cast<DWORD*>(*reinterpret_cast<DWORD*>(frame)+4):0,matrix[12],matrix[13],matrix[14]);}return;}
        // The exact FX weak reference, source actor and current gun behavior
        // authenticate every update. Pool reuse, scene changes and other FX
        // therefore retain their original matrices.
        if(own.hero!=*reinterpret_cast<DWORD*>(0xfd61ec)||
           *reinterpret_cast<DWORD*>(context+0x98)!=own.weapon||
           *reinterpret_cast<DWORD*>(context+0xbc)!=own.actor||
           *reinterpret_cast<DWORD*>(context+0xb8)!=own.identity||
           *reinterpret_cast<DWORD*>(own.actor+0xf8)!=own.identity||
           *reinterpret_cast<DWORD*>(own.weapon+0x25c)!=own.behavior||
           *reinterpret_cast<DWORD*>(own.behavior+0x20)!=own.weapon){ForgetMuzzle(own.context);return;}
        // This zero-speed authored volume represents the fired bullet, not
        // subsequent gun motion. Keep its actual shot pose for this weak actor's
        // lifetime; the ownership checks above still invalidate pool/hero reuse.
        if(own.fixedShotPose){memcpy(matrix,own.shotPose,sizeof(own.shotPose));return;}
        float revised[16],nativeForward[3];memcpy(revised,matrix,sizeof(revised));
        if(!FrameWorldDirection(matrix,own.localForward,nativeForward)||!AimEmitterFromDirection(revised,nativeForward,own.target))return;
        memcpy(matrix,revised,sizeof(revised));
        if(enabled&&InterlockedIncrement(&worldUpdated)<=logShotLimit)Log("Fiery world update context=%08lx actor=%08lx identity=%08lx origin=(%.6f,%.6f,%.6f) nativeForward=(%.6f,%.6f,%.6f) target=(%.6f,%.6f,%.6f) basis=(%.6f,%.6f,%.6f;%.6f,%.6f,%.6f;%.6f,%.6f,%.6f)",own.context,own.actor,own.identity,matrix[12],matrix[13],matrix[14],nativeForward[0],nativeForward[1],nativeForward[2],own.target[0],own.target[1],own.target[2],matrix[0],matrix[1],matrix[2],matrix[4],matrix[5],matrix[6],matrix[8],matrix[9],matrix[10]);
    }__except(EXCEPTION_EXECUTE_HANDLER){}
}
static __declspec(naked) void UpdateAdapter(){
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        // Native639750 EBX=context, ESP+20=final world matrix after638660.
        // Preserve its original EBP for the exact constructor return address.
        mov eax,[ebp]
        push eax
        lea eax,[ebp+24h]
        push eax
        push ebx
        call CorrectWorldUpdate
        add esp,12
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        movss xmm0,dword ptr [ebx+0c0h]
        jmp dword ptr [updateContinuation]
    }
}
static __declspec(naked) void SpawnAdapter(){
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        // At6395A7 nativeESI=FXcontext and nativeESP+80=final WORLD matrix.
        // Preserve the original639480 EBP for exact creation provenance.
        mov eax,[ebp]
        push eax
        lea eax,[ebp+84h]
        push eax
        push esi
        call CorrectStaticWorldMuzzle
        add esp,12
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        movss xmm0,dword ptr [esi+0c0h]
        jmp dword ptr [spawnContinuation]
    }
}
static __declspec(naked) void ShotAdapter(){
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        push edi
        lea eax,[ebp+24h]
        push eax
        lea eax,[ebp+34h]
        push eax
        push ebx
        call CaptureShot
        add esp,16
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        fld dword ptr [ebx+988h]
        fstp dword ptr [ebx+0a08h]
        jmp dword ptr [shotContinuation]
    }
}
static __declspec(naked) void EmitterAdapter(){
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        push edi
        push esi
        call CaptureEmitter
        add esp,8
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        jmp dword ptr [nativeEmitter]
    }
}
static __declspec(naked) void MuzzleAdapter(){
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        push ebx
        call CaptureMuzzle
        add esp,4
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        mov esi,[ebx+300h]
        jmp dword ptr [muzzleContinuation]
    }
}
static bool Patch(BYTE* address,const void* adapter,unsigned bytes,BYTE opcode){
    DWORD old=0;if(!VirtualProtect(address,bytes,PAGE_EXECUTE_READWRITE,&old))return false;
    const DWORD offset=static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(adapter)-reinterpret_cast<ULONG_PTR>(address+5));
    memcpy(address+1,&offset,4);for(unsigned i=5;i<bytes;++i)address[i]=0x90;address[0]=opcode;
    FlushInstructionCache(GetCurrentProcess(),address,bytes);DWORD ignored=0;VirtualProtect(address,bytes,old,&ignored);return true;
}
}
static void ConfigureFieryTrail(const wchar_t* config,bool supported){
    fiery_trail::correctionEnabled=supported&&ballistic_aim::enabled&&ConfigFlag(L"Controls",L"CorrectFieryVisualDirection",true,config);
    fiery_trail::enabled=supported&&ballistic_aim::enabled&&ConfigFlag(L"Diagnostics",L"FieryTrailGeometry",false,config);
}
static void InstallFieryTrail(){
    using namespace fiery_trail;
    if((!fiery_trail::enabled&&!fiery_trail::correctionEnabled)||InterlockedCompareExchange(&installation,1,0))return;
    const BYTE shot[]={0xd9,0x83,0x88,0x09,0,0,0xd9,0x9b,0x08,0x0a,0,0};
    const BYTE emitter[]={0xe8,0xd8,0xfc,0xff,0xff};
    const BYTE spawn[]={0xf3,0x0f,0x10,0x86,0xc0,0,0,0};
    const BYTE update[]={0xf3,0x0f,0x10,0x83,0xc0,0,0,0};
    const BYTE muzzle[]={0x8b,0xb3,0,0x03,0,0};
    auto* a=reinterpret_cast<BYTE*>(0x6ff5df);auto* b=reinterpret_cast<BYTE*>(0x701a63);
    auto* c=reinterpret_cast<BYTE*>(0x6fe898);auto* d=reinterpret_cast<BYTE*>(0x6395a7);auto* e=reinterpret_cast<BYTE*>(0x639800);
    if(memcmp(a,shot,sizeof(shot))||memcmp(b,emitter,sizeof(emitter))||memcmp(c,muzzle,sizeof(muzzle))||memcmp(d,spawn,sizeof(spawn))||memcmp(e,update,sizeof(update))){fiery_trail::enabled=false;fiery_trail::correctionEnabled=false;Log("Fiery trail diagnostic signature mismatch; native geometry untouched");return;}
    if(!Patch(a,reinterpret_cast<const void*>(&ShotAdapter),sizeof(shot),0xe9)||
       !Patch(b,reinterpret_cast<const void*>(&EmitterAdapter),sizeof(emitter),0xe8)||
       !Patch(c,reinterpret_cast<const void*>(&MuzzleAdapter),sizeof(muzzle),0xe9)||
       !Patch(d,reinterpret_cast<const void*>(&SpawnAdapter),sizeof(spawn),0xe9)||
       !Patch(e,reinterpret_cast<const void*>(&UpdateAdapter),sizeof(update),0xe9)){
        fiery_trail::enabled=false;fiery_trail::correctionEnabled=false;Log("Fiery trail diagnostic install incomplete; callbacks disabled; native instructions retained");return;
    }
    Log("Fiery trail hooks installed: exact Glock authored PRT line on fixed shot ray; other owned fire WORLD marker alignment; physical trajectory untouched");
}
