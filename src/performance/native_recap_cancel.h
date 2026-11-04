#pragma once
#include <windows.h>
// Private UI-thread provenance shared by the recap and PC pause presenters.
// Original action5 No leaves UserSys30 through skip_stop and icon_stop; this
// does not authorize unrelated state30 loads or change any native engine state.
namespace native_recap_cancel {
static DWORD movie{},handle{},root{};
inline void Clear()noexcept{movie=handle=root=0;}
inline void Bind(DWORD ownerMovie,DWORD ownerHandle,DWORD ownerRoot)noexcept{movie=ownerMovie;handle=ownerHandle;root=ownerRoot;}
inline bool Matches(DWORD ownerMovie,DWORD ownerHandle,DWORD ownerRoot)noexcept{return movie&&movie==ownerMovie&&handle==ownerHandle&&root==ownerRoot;}
// Flags1 is the original in-game base surface during Options close (118..119).
// Retaining origin here grants no presenter: each still checks its own live flags.
inline void Reconcile()noexcept{if(!movie)return;__try{
 const DWORD flags=*reinterpret_cast<const DWORD*>(0xff4290),system=*reinterpret_cast<const DWORD*>(0xff5508)&0x7fffffff;
 if((flags!=1&&flags!=9&&flags!=5&&flags!=6)||(system!=30&&system!=34&&system!=35)||*reinterpret_cast<const DWORD*>(0xfe1518)!=movie||
    *reinterpret_cast<const DWORD*>(movie+0x24)!=handle||*reinterpret_cast<const DWORD*>(handle)!=root||!*reinterpret_cast<const BYTE*>(movie+0x65))Clear();
 }__except(EXCEPTION_EXECUTE_HANDLER){Clear();}}
inline bool ReadyMenuState(DWORD system)noexcept{Reconcile();return system==35||(system==30&&movie!=0);}
}
