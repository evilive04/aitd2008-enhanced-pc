// Initialize from the existing worker/UI entry, never under DllMain's loader lock.
#include "../shared/crash_protocol.h"
namespace crash_client {
static HANDLE mapping{},request{},ack{};
static crash_protocol::Shared* shared{};
static LPTOP_LEVEL_EXCEPTION_FILTER previous{};
static volatile LONG initialized{};
static DWORD processId{};
static LONG WINAPI Filter(EXCEPTION_POINTERS* pointers){
 auto* state=shared;
 // All storage/IPC resources were allocated before registering this callback.
 // No heap, logging, symbol/loader work or recovery runs on the failing thread.
 if(pointers&&pointers->ExceptionRecord&&pointers->ContextRecord&&state&&
    crash_protocol::Valid(*state,processId)&&InterlockedCompareExchange(&state->state,1,0)==0){
  state->thread=GetCurrentThreadId();state->exception=*pointers->ExceptionRecord;
  state->exception.ExceptionRecord=nullptr;state->context=*pointers->ContextRecord;
  InterlockedExchange(&state->state,2);if(SetEvent(request))WaitForSingleObject(ack,5000);
 }
 return previous&&previous!=&Filter?previous(pointers):EXCEPTION_CONTINUE_SEARCH;
}
static bool Initialize(){
 if(InterlockedCompareExchange(&initialized,1,0))return shared!=nullptr;
 processId=GetCurrentProcessId();crash_protocol::Names names{};
 if(!crash_protocol::ObjectNames(processId,names))return false;
 mapping=OpenFileMappingW(FILE_MAP_READ|FILE_MAP_WRITE,FALSE,names.mapping);
 if(mapping)shared=static_cast<crash_protocol::Shared*>(MapViewOfFile(mapping,FILE_MAP_READ|FILE_MAP_WRITE,0,0,sizeof(crash_protocol::Shared)));
 request=OpenEventW(EVENT_MODIFY_STATE,FALSE,names.request);ack=OpenEventW(SYNCHRONIZE,FALSE,names.ack);
 if(!shared||!request||!ack||!crash_protocol::Valid(*shared,processId)||shared->state!=0){
  if(shared)UnmapViewOfFile(shared);shared=nullptr;
  if(mapping)CloseHandle(mapping);if(request)CloseHandle(request);if(ack)CloseHandle(ack);mapping=request=ack=nullptr;return false;
 }
 previous=SetUnhandledExceptionFilter(&Filter);return true;
}
}
