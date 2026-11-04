// Include outside modern_combat, after combat_controls.inl.
#include "target_lock_state.h"
#include <cmath>
#include "target_lock_projector.inl"
namespace target_lock_capture {
static SRWLOCK lock=SRWLOCK_INIT;
static TargetLockStateV2 published{sizeof(TargetLockStateV2),2};
static bool Capture(bool effectiveLock,bool eligible){
    TargetLockStateV2 next{sizeof(TargetLockStateV2),2,0,GetTickCount()};
    __try {
        using namespace modern_combat;
        auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);
        // 64BF00 -> 68ED30(hero+FA0); 68F760 is the native active-selection
        // predicate. 68F8A0 selects the node, never a guessed nearest actor.
        if(effectiveLock&&eligible&&hero&&Word(hero,0)==0xc834c0&&hero[0xfa4]&&Pointer(hero,0xfa8)==hero){
            const DWORD count=Word(hero,0xfa0);auto* selected=Pointer(hero,0xfac);
            if(count&&count<=512&&selected&&!selected[8]){
                bool member=false;auto* node=Pointer(hero,0xfbc);
                for(DWORD n=0;node&&n<count;++n,node=Pointer(node,0x18))if(node==selected){member=true;break;}
                auto* actor=member?Pointer(selected,0):nullptr;
                if(actor&&actor!=hero&&!(actor[0x248]&0x80)){
                    next.actor=reinterpret_cast<DWORD>(actor);next.guidLow=Word(actor,0x10);next.guidHigh=Word(actor,0x14);
                    bool finite=true;for(unsigned n=0;n<3;++n){next.world[n]=*reinterpret_cast<const float*>(actor+0x220+n*4);finite=finite&&std::isfinite(next.world[n])&&fabsf(next.world[n])<1000000.f;}
                    if(finite){next.flags=1;if(target_lock_projector::Capture(actor,next))next.flags|=2;}
                }
            }
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){next.flags=0;}
    AcquireSRWLockExclusive(&lock);published=next;ReleaseSRWLockExclusive(&lock);
    return (next.flags&1)!=0;
}
}
extern "C" BOOL WINAPI AITDGetTargetLockState(TargetLockStateV2* output,DWORD bytes){
    if(!output||bytes<sizeof(TargetLockStateV2)||!TryAcquireSRWLockShared(&target_lock_capture::lock))return FALSE;
    BOOL copied=FALSE;
    __try {*output=target_lock_capture::published;copied=TRUE;}
    __except(EXCEPTION_EXECUTE_HANDLER){copied=FALSE;}
    ReleaseSRWLockShared(&target_lock_capture::lock);return copied;
}
