#pragma once
#include <windows.h>
struct IDirect3DTexture9;
// Original game pixels. Successful caller owns one COM reference; no native
// resource pointer crosses this API. Row/epoch are bound to the copied roster.
using PhonePortraitGetter=HRESULT(WINAPI*)(DWORD row,DWORD epoch,IDirect3DTexture9**);
