#include "weapon_hud_state.h"
#include <cmath>
namespace weapon_hud {
static bool enabled=false;
static SRWLOCK lock=SRWLOCK_INIT;
static WeaponHudStateV1 published{sizeof(WeaponHudStateV1),1,0,0,0,0,{0,0}};
static void Configure(const wchar_t* config,bool supported){
    enabled=supported&&ConfigFlag(L"HUD",L"Ammunition",true,config);
}
static WeaponHudStateV1 ReadSnapshot(bool focused,DWORD now){
    WeaponHudStateV1 next{sizeof(WeaponHudStateV1),1,0,now,0,0,{0,0}};
    if(!enabled)return next;
    __try {
        using namespace modern_combat;
        auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);
        if(focused&&hero&&Word(hero,0)==0xc834c0&&
            (Word(reinterpret_cast<void*>(0xfd8bd4),0)&15)==0&&
            (Word(reinterpret_cast<void*>(0xfda5c8),0)&0x38)==8&&
            Pointer(hero,0x95c)&&(Word(Pointer(hero,0x95c),4)&6)==6){
            auto* inventory=Pointer(hero,0x810);auto* hands=Pointer(hero,0xf9c);
            const auto* cinematic=Pointer(hero,0x280);
            // Native context index39 is decimal (HHEL), at1E00; index0x39 is unrelated.
            const float healing=*reinterpret_cast<const float*>(hero+0x1e00);
            const float inventoryTime=*reinterpret_cast<const float*>(hero+0x1d64+9*4);
            if(!hero[0xc19]&&(!cinematic||!cinematic[0x204])&&std::isfinite(healing)&&healing<=0&&
                std::isfinite(inventoryTime)&&inventoryTime<=0&&inventory&&Pointer(inventory,0x754)==hero&&
                !Word(inventory,0x81c)&&hands&&Pointer(hands,0)==hero){
                // r29 AOWP firearm in the authentic right hand; no guessed reserve count.
                auto* weapon=Pointer(hands,0x14);
                if(IsPistol(weapon)&&NativePistolBehavior(weapon)){
                    auto* manager=Pointer(weapon,0x510);
                    const DWORD count=manager?Word(manager,0xc):0;auto* entries=manager?Pointer(manager,0x10):nullptr;
                    if(entries&&count<=256)for(DWORD n=0;n<count;++n){
                        auto* component=Pointer(entries,n*12+4);
                        if(component&&*reinterpret_cast<const short*>(component+0x34)==1){
                            const DWORD rounds=Word(component,0x38);
                            if(rounds<=9999){
                                next.flags=WeaponHudVisible;next.rounds=rounds;next.weaponKind=1;
                                // Native6FF3D0/6FE700 select the LAST type1 ammo
                                // record for the next shot, not any coated round
                                // elsewhere in the stack or the FX configuration.
                                const DWORD loaded=Word(component,0x44);
                                const auto* ammo=Pointer(component,0x48);
                                if(rounds&&loaded&&loaded<=9999&&ammo&&Word(ammo,(loaded-1)*8)==1)
                                    next.flags|=WeaponHudFireAmmo;
                            }
                            break;
                        }
                    }
                }
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER){next.flags=0;}
    return next;
}
static void Capture(){
    if(!enabled)return;
    const HWND foreground=GetForegroundWindow();DWORD process=0;
    GetWindowThreadProcessId(foreground,&process);
    const auto next=ReadSnapshot(process==GetCurrentProcessId()&&!IsIconic(foreground),GetTickCount());
    AcquireSRWLockExclusive(&lock);published=next;ReleaseSRWLockExclusive(&lock);
}
}
extern "C" BOOL WINAPI AITDGetWeaponHudState(WeaponHudStateV1* output,DWORD bytes){
    if(!output||bytes<sizeof(WeaponHudStateV1)||!weapon_hud::enabled)return FALSE;
    if(!TryAcquireSRWLockShared(&weapon_hud::lock))return FALSE;
    *output=weapon_hud::published;ReleaseSRWLockShared(&weapon_hud::lock);return TRUE;
}
