#pragma once
#include <windows.h>
#include "bleeding_ui_state.h"
enum InventoryUiFlags:DWORD {InventoryUiVisible=1,InventoryUiItem=2,InventoryUiCombining=4,InventoryUiFireAmmo=8,InventoryUiPowerFull=16,InventoryUiPowerKnown=32,InventoryUiTip=64,InventoryUiFavorites=128,InventoryUiHandsKnown=0x4000,InventoryUiAmmoKnown=0x8000,InventoryUiGunItem=0x10000,InventoryUiSelectMouseLeft=0x20000,InventoryUiBatteryItem=0x40000,InventoryUiBulletItem=0x80000,InventoryUiNativeEquipment=0x100000};
// Inventory equipment categories occupy two three-bit fields in the existing
// flags word; the copied ABI and its version remain byte-for-byte unchanged.
constexpr DWORD InventoryUiLeftShift=8,InventoryUiRightShift=11,InventoryUiHandMask=7;
struct InventoryUiPrompt {wchar_t key[256]{},label[128]{};};
struct InventoryUiImage {DWORD bitmap{},width{},height{};float x{},y{},w{},h{};};
struct InventoryUiStateV1 {
 DWORD size=sizeof(InventoryUiStateV1),version=1,tick{},flags{},generation{},nativeCase{},promptCount{},liquidKind{};
 wchar_t item[128]{},quantity[64]{},bullets[64]{},liquid[64]{},boxes[64]{},instruction[512]{};
 wchar_t closeKey[256]{},closeLabel[128]{};
 InventoryUiPrompt prompts[4]{};
 BleedingUiValue bleeding{};
 DWORD ammo{};
 InventoryUiImage images[3]{}; // Selected detail, actual left hand, actual right hand.
};
static_assert(sizeof(InventoryUiStateV1)==5768,"Copied inventory UI ABI");
using InventoryUiGetter=BOOL(WINAPI*)(void*,DWORD);
using InventoryUiReady=void(WINAPI*)(BOOL,DWORD);
struct IDirect3DTexture9;
using InventoryUiImageGetter=HRESULT(WINAPI*)(DWORD,DWORD,IDirect3DTexture9**);
