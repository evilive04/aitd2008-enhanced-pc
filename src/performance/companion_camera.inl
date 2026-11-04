// Track native active followers; no actor search, relocation or mission-state edits.
struct CameraFollower { unsigned char* behavior; unsigned char* actor; DWORD tick; };
static CameraFollower cameraFollowers[32]{};
static SRWLOCK cameraFollowersLock=SRWLOCK_INIT;
static bool CameraFollowerValid(unsigned char* behavior,unsigned char* actor,unsigned char* hero){
    return behavior&&actor&&hero&&actor!=hero&&
        *reinterpret_cast<DWORD*>(behavior)==0xca8ab8&&
        *reinterpret_cast<unsigned char**>(behavior+0x20)==actor&&
        *reinterpret_cast<unsigned char**>(behavior+0x240)==hero&&
        *reinterpret_cast<unsigned char**>(actor+0x25c)==behavior;
}
static void RecordCameraFollower(unsigned char* behavior){
    __try {
        auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);
        auto* actor=*reinterpret_cast<unsigned char**>(behavior+0x20);
        if(!CameraFollowerValid(behavior,actor,hero))return;
        const DWORD now=GetTickCount();
        AcquireSRWLockExclusive(&cameraFollowersLock);
        unsigned slot=0;DWORD oldest=0;
        for(unsigned i=0;i<32;++i){
            if(cameraFollowers[i].behavior==behavior){slot=i;break;}
            const DWORD age=now-cameraFollowers[i].tick;
            if(!cameraFollowers[i].behavior||age>oldest){slot=i;oldest=age;}
        }
        cameraFollowers[slot]={behavior,actor,now};
        ReleaseSRWLockExclusive(&cameraFollowersLock);
    } __except(EXCEPTION_EXECUTE_HANDLER){}
}
static void* cameraNativeFollowerUpdate=reinterpret_cast<void*>(0x79db00);
static __declspec(naked) void CameraFollowerUpdate(){
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        push ecx
        call RecordCameraFollower
        add esp,4
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        jmp dword ptr [cameraNativeFollowerUpdate]
    }
}
static bool OrdinaryCompanionContext(unsigned char* hero){
    if((!freeThirdPerson&&!companionFollowNormally)||!hero||*reinterpret_cast<DWORD*>(hero)!=0xc834c0||hero[0xc19])return false;
    auto* behavior=*reinterpret_cast<unsigned char**>(hero+0x25c);
    auto* controller=*reinterpret_cast<unsigned char**>(hero+0x95c);
    return behavior&&*reinterpret_cast<DWORD*>(behavior)==0xc857b8&&
        *reinterpret_cast<unsigned*>(behavior+0xb0)<=7&&controller&&
        controller==*reinterpret_cast<unsigned char**>(0xfda7c4)&&
        (*reinterpret_cast<DWORD*>(controller+4)&6)==6&&
        (*reinterpret_cast<DWORD*>(0xfda5c8)&0x38)==8;
}
static bool __cdecl CompanionCanRecover(unsigned char* actor,DWORD caller,DWORD argument){
    __try {
        if(!companionFollowNormally||caller!=0x79dcd2||argument||!actor)return true;
        auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);
        if(!OrdinaryCompanionContext(hero))return true;
        auto* behavior=*reinterpret_cast<unsigned char**>(actor+0x25c);
        if(!CameraFollowerValid(behavior,actor,hero))return true;
        const unsigned state=*reinterpret_cast<unsigned*>(behavior+0xb0);
        if(state!=29)return true; // Only proven ordinary follow; inherited scripted states stay native.
        const float nativeDistance=OrbitRead(behavior,0x25c);
        const float distance=std::isfinite(nativeDistance)?std::max(companionCatchUpDistance,nativeDistance):companionCatchUpDistance;
        float squared=0;
        for(unsigned i=0;i<3;++i){const float d=OrbitRead(actor,0x220+i*4)-OrbitRead(hero,0x220+i*4);if(!std::isfinite(d))return false;squared+=d*d;}
        // A navigation failure timer must not override nearby ordinary walking.
        return squared>=distance*distance;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static __declspec(naked) void CompanionRecoveryGate(){
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        push [ebp+8]
        push [ebp+4]
        push ecx
        call CompanionCanRecover
        add esp,12
        test al,al
        jz companionRecoveryWait
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        mov eax,[ecx]
        mov eax,[eax+104h]
        jmp eax
    companionRecoveryWait:
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        xor eax,eax
        mov esp,ebp
        pop ebp
        ret 4
    }
}
static __declspec(naked) void CompanionRecoverySelector(){
    __asm {
        mov eax,offset CompanionRecoveryGate
        ret
    }
}
static bool CompanionCandidateOffscreen(unsigned char* camera,const float* matrix){
    if(!camera)return false;
    float forward[3],length2=0,projection=0;
    for(unsigned i=0;i<3;++i){
        forward[i]=OrbitRead(camera,0x210+i*4);
        const float d=matrix[12+i]-OrbitRead(camera,0x220+i*4);
        if(!std::isfinite(forward[i])||!std::isfinite(d))return false;
        length2+=forward[i]*forward[i];projection+=d*forward[i];
    }
    if(length2<.5f||length2>1.5f)return false;
    // Entire human-sized capsule must lie behind the actual camera plane.
    const float furthest=projection+std::max(forward[1]*.2f,forward[1]*2.4f)+.65f*sqrtf(length2);
    return furthest<0;
}
static bool __cdecl AllowCompanionPlacement(unsigned char* actor,const float* matrix,const DWORD* nativeFrame){
    __try {
        if(!nativeFrame||nativeFrame[1]!=0x79dcd2||nativeFrame[2]!=0)return true;
        auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);
        if(!actor||!matrix||!OrdinaryCompanionContext(hero))return true;
        auto* behavior=*reinterpret_cast<unsigned char**>(actor+0x25c);
        if(!CameraFollowerValid(behavior,actor,hero))return true;
        // Only the engine's automatic camera-relative recovery reaches these hooks.
        // Reject the candidate before native collision validation/application. The
        // native routine then tries its other ground-tested directions or waits.
        const float x=matrix[12],y=matrix[13],z=matrix[14];
        if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z))return false;
        const float dx=x-OrbitRead(hero,0x220),dz=z-OrbitRead(hero,0x228);
        bool allow=dx*dx+dz*dz>=2.75f*2.75f;
        const auto top=*reinterpret_cast<unsigned char***>(0xfda8f0);
        if(top&&*top){
            auto* camera=*top;
            const float cx=x-OrbitRead(camera,0x220),cz=z-OrbitRead(camera,0x228);
            const float above=OrbitRead(camera,0x224)-y;
            if(above>-.5f&&above<2.5f&&cx*cx+cz*cz<1.15f*1.15f)allow=false;
            if(companionFollowNormally&&(!CompanionCandidateOffscreen(camera,matrix)||dx*dx+dz*dz>12.0f*12.0f))allow=false;
        }
        else if(companionFollowNormally)allow=false;
        if(!allow){
            static DWORD lastLog=0;const DWORD now=GetTickCount();
            if(now-lastLog>2000){lastLog=now;Log("Companion recovery candidate rejected: actor=%p hero_distance=%.3f",actor,sqrtf(dx*dx+dz*dz));}
        }
        return allow;
    } __except(EXCEPTION_EXECUTE_HANDLER){return true;}
}
// Replaces push EAX / mov ECX,EBX / call EDX at automatic recovery only.
// The untouched EDX virtual method owns native collision and placement.
static DWORD companionApplyReturn=0;
static __declspec(naked) void CompanionPlacementAdapter(){
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        push [ebp]
        push eax
        push ebx
        call AllowCompanionPlacement
        add esp,12
        test al,al // MSVC bool returns AL; upper EAX is unspecified.
        jz companionReject
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov companionApplyReturn,offset companionNativeReturned
        push eax
        mov ecx,ebx
        call edx
    companionNativeReturned:
        mov esp,ebp
        pop ebp
        ret
    companionReject:
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        xor eax,eax
        mov esp,ebp
        pop ebp
        ret
    }
}
static bool __cdecl AllowFinalCompanionPlacement(unsigned char* actor,const float* matrix,const DWORD* collisionFrame){
    __try {
        // Native collision can change the candidate matrix. Validate that final
        // position, and prove this call is nested in our automatic recovery seam.
        if(!collisionFrame||collisionFrame[1]!=companionApplyReturn||!companionApplyReturn)return true;
        const auto* adapterFrame=reinterpret_cast<const DWORD*>(collisionFrame[0]);
        if(!adapterFrame||(adapterFrame[1]!=0x477937&&adapterFrame[1]!=0x4779a1&&adapterFrame[1]!=0x477a00))return true;
        return AllowCompanionPlacement(actor,matrix,reinterpret_cast<const DWORD*>(adapterFrame[0]));
    } __except(EXCEPTION_EXECUTE_HANDLER){return true;}
}
static __declspec(naked) void CompanionFinalPlacementAdapter(){
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        push [ebp]
        push edi
        push esi
        call AllowFinalCompanionPlacement
        add esp,12
        test al,al // Preserve the one-byte bool return contract here as well.
        jz companionFinalReject
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        push edi
        mov ecx,esi
        call eax
        mov esp,ebp
        pop ebp
        ret
    companionFinalReject:
        // Resume the native collision-failure epilogue, not its success return.
        mov dword ptr [ebp+4],47742dh
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        ret
    }
}
static void AppendCameraFollowers(unsigned char* component,unsigned char** items,int* count){
    CameraFollower snapshot[32];
    AcquireSRWLockShared(&cameraFollowersLock);
    memcpy(snapshot,cameraFollowers,sizeof(snapshot));
    ReleaseSRWLockShared(&cameraFollowersLock);
    __try {
        auto* camera=*reinterpret_cast<unsigned char**>(0xfd5d98);
        auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);
        if((!freeThirdPerson&&!companionFollowNormally)||!component||!camera||!hero||!items||!count||*count<0||*count>40||
            *reinterpret_cast<unsigned char**>(component+0x20)!=camera||
            *reinterpret_cast<unsigned char**>(camera+0x3e8)!=hero)return;
        const DWORD now=GetTickCount();
        for(const auto& entry:snapshot){
            if(*count>=40)break;
            if(now-entry.tick>1000||!CameraFollowerValid(entry.behavior,entry.actor,hero))continue;
            bool duplicate=false;
            for(int j=0;j<*count;++j)if(items[j]==entry.actor){duplicate=true;break;}
            if(duplicate)continue;
            // Native bounded recursion includes the actor's attached collision entities.
            reinterpret_cast<void(__thiscall*)(unsigned char*,unsigned char**,int*,int)>(0x4794c0)(entry.actor,items,count,40);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER){}
}
static void* cameraNativeExclusions=reinterpret_cast<void*>(0x46cf90);
// Native helper uses EAX=component, ESI=count, one callee-cleaned stack argument.
static __declspec(naked) void CameraExclusionsAdapter(){
    __asm {
        push ebp
        mov ebp,esp
        push eax
        push [ebp+8]
        call dword ptr [cameraNativeExclusions]
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        push esi
        push [ebp+8]
        push [ebp-4]
        call AppendCameraFollowers
        add esp,12
        fxrstor [esp]
        lea esp,[ebp-40]
        popad
        popfd
        mov esp,ebp
        pop ebp
        ret 4
    }
}
