#pragma once
struct FavoriteSlotView {DWORD valid,icons[3];};
struct FavoritesStateV1 {
 DWORD size,version,flags,tick,mode,hover,selected,status;
 float cursorX,cursorY;
 DWORD current[3];FavoriteSlotView slots[6];
};
static_assert(sizeof(FavoritesStateV1)==148,"Favorites snapshot ABI");
using FavoritesStateGetter=BOOL(WINAPI*)(FavoritesStateV1*,DWORD);
