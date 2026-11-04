#pragma once
#include <windows.h>
enum RecapFlags:DWORD{RecapVisible=1,RecapBusy=2,RecapLocked=4};
enum RecapAction:DWORD{RecapBack=1,RecapPreviousSequence,RecapPreviousChapter,RecapPlay,RecapNextChapter,RecapNextSequence};
struct RecapStateV1{
 DWORD size{},version{},tick{},flags{},generation{},episode{},episodes{},sequence{},sequences{},chapter{},chapters{},videoLoaded{},dispatches{},rejected{};
 wchar_t title[128]{},episodeTitle[128]{},sequenceTitle[128]{},summary[768]{},back[64]{},play[64]{};
};
static_assert(sizeof(RecapStateV1)==2616,"Recap copied ABI");
using RecapGetter=BOOL(WINAPI*)(void*,DWORD);
using RecapSender=BOOL(WINAPI*)(DWORD,DWORD);
using RecapReady=void(WINAPI*)(BOOL,DWORD);
