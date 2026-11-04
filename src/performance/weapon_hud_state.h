#pragma once
#include <windows.h>
// POD copied from the input thread. Renderer never dereferences game objects.
enum WeaponHudFlags:DWORD {WeaponHudVisible=1,WeaponHudFireAmmo=2};
struct WeaponHudStateV1 {
    DWORD size,version,flags,tick;
    DWORD rounds,weaponKind,reserved[2];
};
static_assert(sizeof(WeaponHudStateV1)==32,"Weapon HUD ABI");
using WeaponHudStateGetter=BOOL (WINAPI*)(WeaponHudStateV1*,DWORD);
