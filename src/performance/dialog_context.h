#pragma once
#include <windows.h>
#include <cstdint>
// Cross-DLL transport: owned UTF16 only, no game pointers or callback addresses.
struct DialogStateV1 {
    DWORD size,version,tick,flags,generation,mode,action,pendingResult,language,codePage;
    DWORD firstResult,secondResult;
    WCHAR body[4096],firstLabel[256],secondLabel[256];
};
static_assert(sizeof(DialogStateV1)==9264,"DialogStateV1 transport ABI changed");
enum DialogFlags : DWORD { DialogReady=1,DialogFirstEnabled=2,DialogSecondEnabled=4,DialogChoice=8,DialogClosing=16,DialogPhoneOrigin=32 };
enum DialogSlot : DWORD { DialogFirst=1,DialogSecond=2 };
using DialogStateGetter=BOOL (WINAPI*)(void*,DWORD);
using DialogActionSender=BOOL (WINAPI*)(DWORD,DWORD);
namespace dialog_context {
// Internal same-DLL UI-thread notification, before the closing movie can clear
// native owner/result fields. Not an exported transport/ABI.
using AcceptedChoiceObserver=void(*)(DWORD generation,DWORD mode,DWORD action,DWORD result,DWORD nativeOwner)noexcept;
void SetChoiceObserver(AcceptedChoiceObserver observer)noexcept;
// Call from the authenticated exact-r29 UI thread only. Explicit codePage is
// required for non-ASCII (0 means reject it). This module never guesses locales.
void Initialize(bool exactR29Authenticated,UINT codePage=0);
// Integration point: after the original 54AF80 returned AL=1, before returning
// to its caller. Pass the unchanged original mode. Failure must not notify.
void ObserveOpen(DWORD mode);
void Tick();
BOOL GetState(void* out,DWORD bytes);
bool OwnsNativeRoot(std::uintptr_t root,DWORD expectedGeneration) noexcept;
// Slots refer to displayed labels. Choice first=No, second=Yes. Caller's exact
// snapshot generation is mandatory; timeout/close/reopen invalidates old clicks.
BOOL QueueAction(DWORD slot,DWORD snapshotGeneration);
}
