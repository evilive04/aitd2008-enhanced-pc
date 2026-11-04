#pragma once
#include <windows.h>
#include "../shared/crash_protocol.h"
#include <dbghelp.h>
#include <tlhelp32.h>
#include <cstdio>
#include <string>
#pragma comment(lib,"dbghelp.lib")
// External writer and normal-process IPC monitor. No debugger port, native
// heap mode changes, registry/WER changes, or in-process dump generation.
namespace crash_monitor {
struct Channel {
 HANDLE mapping{},request{},ack{};crash_protocol::Shared* shared{};
 Channel()=default;Channel(const Channel&)=delete;Channel& operator=(const Channel&)=delete;
 ~Channel(){if(shared)UnmapViewOfFile(shared);if(mapping)CloseHandle(mapping);if(request)CloseHandle(request);if(ack)CloseHandle(ack);}
 bool Create(DWORD pid){
  crash_protocol::Names names{};if(!crash_protocol::ObjectNames(pid,names))return false;
  mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(crash_protocol::Shared),names.mapping);
  if(!mapping||GetLastError()==ERROR_ALREADY_EXISTS)return false;
  shared=static_cast<crash_protocol::Shared*>(MapViewOfFile(mapping,FILE_MAP_READ|FILE_MAP_WRITE,0,0,sizeof(crash_protocol::Shared)));if(!shared)return false;
  request=CreateEventW(nullptr,FALSE,FALSE,names.request);if(!request||GetLastError()==ERROR_ALREADY_EXISTS)return false;
  ack=CreateEventW(nullptr,FALSE,FALSE,names.ack);if(!ack||GetLastError()==ERROR_ALREADY_EXISTS)return false;
  *shared={};shared->magic=crash_protocol::Magic;shared->version=crash_protocol::Version;shared->bytes=sizeof(*shared);shared->pid=pid;return true;
 }
};
inline std::string Utf8(const wchar_t* value){const int n=WideCharToMultiByte(CP_UTF8,0,value,-1,nullptr,0,nullptr,nullptr);if(n<=1)return {};std::string out(static_cast<size_t>(n),0);WideCharToMultiByte(CP_UTF8,0,value,-1,out.data(),n,nullptr,nullptr);out.pop_back();return out;}
inline std::string Quoted(const wchar_t* value){std::string out="\"";for(const auto c:Utf8(value)){if(c=='\\'||c=='\"')out+='\\';if(static_cast<unsigned char>(c)>=32)out+=c;}return out+'\"';}
inline std::wstring Folder(){wchar_t base[32768]{};const auto n=GetEnvironmentVariableW(L"LOCALAPPDATA",base,32768);if(!n||n>=32768)return {};std::wstring p=std::wstring(base)+L"\\AITD2008Enhanced";CreateDirectoryW(p.c_str(),nullptr);p+=L"\\crashes";CreateDirectoryW(p.c_str(),nullptr);return p;}
inline std::wstring Stamp(DWORD pid){SYSTEMTIME t{};GetLocalTime(&t);wchar_t value[96]{};swprintf_s(value,L"%04u-%02u-%02u_%02u-%02u-%02u-%03u_pid%lu",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds,pid);return value;}
// Remote virtual-address accounting, not physical RAM or allocator ownership.
// A 64-bit cursor is essential even though this external launcher is x86.
struct MemorySnapshot {
 ULONGLONG elapsed{},committed{},reserved{},free{},largestFree{},largestFreeBase{},above2gbAllocated{},highestMappedEnd{},walkedUntil{};
 DWORD regions{},queryError{};
};
inline MemorySnapshot SampleMemory(HANDLE process,ULONGLONG elapsed){
 MemorySnapshot out{};out.elapsed=elapsed;constexpr ULONGLONG limit=0x100000000ull,split=0x80000000ull;ULONGLONG cursor=0;
 while(cursor<limit){
  MEMORY_BASIC_INFORMATION m{};const auto address=reinterpret_cast<LPCVOID>(static_cast<ULONG_PTR>(static_cast<DWORD>(cursor)));
  if(VirtualQueryEx(process,address,&m,sizeof m)!=sizeof m){out.queryError=GetLastError();break;}
  const auto base=static_cast<ULONGLONG>(reinterpret_cast<ULONG_PTR>(m.BaseAddress));const auto size=static_cast<ULONGLONG>(m.RegionSize);
  if(!size||base>cursor||base+size<=cursor){out.queryError=ERROR_INVALID_DATA;break;}
  const auto end=base+size<limit?base+size:limit;const auto bytes=end-cursor;++out.regions;
  if(m.State==MEM_COMMIT)out.committed+=bytes;
  else if(m.State==MEM_RESERVE)out.reserved+=bytes;
  else if(m.State==MEM_FREE){out.free+=bytes;if(bytes>out.largestFree){out.largestFree=bytes;out.largestFreeBase=cursor;}}
  if(m.State==MEM_COMMIT||m.State==MEM_RESERVE){out.highestMappedEnd=end;if(end>split)out.above2gbAllocated+=end-(cursor>split?cursor:split);}
  cursor=end;out.walkedUntil=cursor;
 }
 return out;
}
struct MemoryHistory {
 static constexpr unsigned Capacity=12;MemorySnapshot samples[Capacity]{};unsigned stored{},next{};ULONGLONG count{},peakCommitted{},peakReserved{},peakAbove2gb{},highestMappedEnd{},minimumLargestFree=~0ull;
 void Add(const MemorySnapshot& s){samples[next]=s;next=(next+1)%Capacity;if(stored<Capacity)++stored;++count;
  if(s.committed>peakCommitted)peakCommitted=s.committed;if(s.reserved>peakReserved)peakReserved=s.reserved;if(s.above2gbAllocated>peakAbove2gb)peakAbove2gb=s.above2gbAllocated;if(s.highestMappedEnd>highestMappedEnd)highestMappedEnd=s.highestMappedEnd;
  if(s.walkedUntil&&s.largestFree<minimumLargestFree)minimumLargestFree=s.largestFree;
 }
};
inline void WriteMemory(FILE* file,const MemorySnapshot& s){
 std::fprintf(file,"{\"elapsed_ms\":%llu,\"committed_bytes\":%llu,\"reserved_bytes\":%llu,\"free_bytes\":%llu,\"largest_free_bytes\":%llu,\"largest_free_base\":%llu,\"above_2gb_allocated_bytes\":%llu,\"highest_mapped_end_exclusive\":%llu,\"walked_until_exclusive\":%llu,\"regions\":%lu,\"query_error\":%lu}",s.elapsed,s.committed,s.reserved,s.free,s.largestFree,s.largestFreeBase,s.above2gbAllocated,s.highestMappedEnd,s.walkedUntil,s.regions,s.queryError);
}
inline void WriteHistory(FILE* file,const MemoryHistory& h){
 std::fprintf(file,"{\"scope\":\"remote_virtual_address_space_not_physical_ram\",\"sample_interval_ms\":5000,\"sample_limit\":12,\"samples_total\":%llu,\"peak_committed_bytes\":%llu,\"peak_reserved_bytes\":%llu,\"peak_above_2gb_allocated_bytes\":%llu,\"highest_mapped_end_exclusive\":%llu,\"minimum_largest_free_bytes\":%llu,\"last_samples\":[",h.count,h.peakCommitted,h.peakReserved,h.peakAbove2gb,h.highestMappedEnd,h.minimumLargestFree==~0ull?0:h.minimumLargestFree);
 const auto begin=(h.next+MemoryHistory::Capacity-h.stored)%MemoryHistory::Capacity;for(unsigned n=0;n<h.stored;++n){if(n)std::fprintf(file,",");WriteMemory(file,h.samples[(begin+n)%MemoryHistory::Capacity]);}std::fprintf(file,"]}");
}
inline void RecordException(HANDLE process,DWORD pid,const crash_protocol::Shared& fault,const std::wstring& folder,const std::wstring& stem,MemoryHistory* history=nullptr,ULONGLONG elapsed=0){
 const auto memory=SampleMemory(process,elapsed);if(history)history->Add(memory);
 const std::wstring path=folder+L"\\"+stem;CONTEXT context=fault.context;
 EXCEPTION_RECORD record=fault.exception;record.ExceptionRecord=nullptr;EXCEPTION_POINTERS pointers{&record,&context};MINIDUMP_EXCEPTION_INFORMATION info{fault.thread,&pointers,FALSE};
 const auto dumpName=path+L".dmp";HANDLE dump=CreateFileW(dumpName.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);BOOL dumped=FALSE;DWORD error=GetLastError();
 if(dump!=INVALID_HANDLE_VALUE){const auto flags=static_cast<MINIDUMP_TYPE>(MiniDumpWithThreadInfo|MiniDumpWithUnloadedModules|MiniDumpWithIndirectlyReferencedMemory|MiniDumpWithProcessThreadData|MiniDumpWithFullMemoryInfo);dumped=MiniDumpWriteDump(process,pid,dump,flags,&info,nullptr,nullptr);error=dumped?0:GetLastError();FlushFileBuffers(dump);CloseHandle(dump);}
 FILE* log{};_wfopen_s(&log,(path+L".json").c_str(),L"wb");if(!log)return;
 std::fprintf(log,"{\n  \"schema\":2,\"build\":\"v145\",\"pid\":%lu,\"thread\":%lu,\"unhandled\":true,\"exception_code\":\"%08lx\",\"address\":\"%p\",\"dump_written\":%s,\"dump_error\":%lu,\n",pid,fault.thread,record.ExceptionCode,record.ExceptionAddress,dumped?"true":"false",error);
 std::fprintf(log,"  \"memory_at_exception\":");WriteMemory(log,memory);std::fprintf(log,",\n  \"memory_telemetry\":");if(history)WriteHistory(log,*history);else std::fprintf(log,"null");std::fprintf(log,",\n");
 std::fprintf(log,"  \"input_log\":\"input-enhanced-%lu.log\",\"render_log\":\"d3d9-%lu.log\",\n",pid,pid);
#if defined(_M_IX86)
 std::fprintf(log,"  \"registers\":{\"eip\":\"%08lx\",\"esp\":\"%08lx\",\"ebp\":\"%08lx\",\"eax\":\"%08lx\",\"ebx\":\"%08lx\",\"ecx\":\"%08lx\",\"edx\":\"%08lx\",\"esi\":\"%08lx\",\"edi\":\"%08lx\"},\n",context.Eip,context.Esp,context.Ebp,context.Eax,context.Ebx,context.Ecx,context.Edx,context.Esi,context.Edi);
 DWORD stack[64]{};SIZE_T bytes{};ReadProcessMemory(process,reinterpret_cast<void*>(context.Esp),stack,sizeof stack,&bytes);std::fprintf(log,"  \"stack_words\":[");for(size_t n=0;n<bytes/sizeof(DWORD);++n)std::fprintf(log,"%s\"%08lx\"",n?",":"",stack[n]);std::fprintf(log,"],\n");
#endif
 std::fprintf(log,"  \"exception_information\":[");for(DWORD n=0;n<record.NumberParameters&&n<EXCEPTION_MAXIMUM_PARAMETERS;++n)std::fprintf(log,"%s\"%p\"",n?",":"",reinterpret_cast<void*>(record.ExceptionInformation[n]));std::fprintf(log,"],\n  \"modules\":[");
 HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid);MODULEENTRY32W module{};module.dwSize=sizeof module;bool first=true;if(snapshot!=INVALID_HANDLE_VALUE){if(Module32FirstW(snapshot,&module))do{std::fprintf(log,"%s{\"name\":%s,\"path\":%s,\"base\":\"%p\",\"size\":%lu}",first?"":",",Quoted(module.szModule).c_str(),Quoted(module.szExePath).c_str(),module.modBaseAddr,module.modBaseSize);first=false;}while(Module32NextW(snapshot,&module));CloseHandle(snapshot);}std::fprintf(log,"]\n}\n");std::fclose(log);
}
inline DWORD Monitor(PROCESS_INFORMATION& process,Channel& channel,const std::wstring& directory=Folder()){
 const auto stem=Stamp(process.dwProcessId);const auto start=GetTickCount64();DWORD crashes=0,exitCode=~0u;MemoryHistory memory;memory.Add(SampleMemory(process.hProcess,0));
 HANDLE waits[]={process.hProcess,channel.request};const DWORD count=channel.request?2u:1u;
 for(;;){const DWORD result=WaitForMultipleObjects(count,waits,FALSE,5000);
  if(result==WAIT_TIMEOUT){memory.Add(SampleMemory(process.hProcess,GetTickCount64()-start));continue;}
  if(result==WAIT_OBJECT_0){GetExitCodeProcess(process.hProcess,&exitCode);break;}
  if(result!=WAIT_OBJECT_0+1)break;
  const auto* state=channel.shared;
  if(state&&crash_protocol::Valid(*state,process.dwProcessId)&&state->state==2){
   const crash_protocol::Shared fault=*state;
   // Shared data contains copied POD, never remote pointers to dereference.
   if(crash_protocol::Valid(fault,process.dwProcessId)&&fault.thread&&fault.exception.NumberParameters<=EXCEPTION_MAXIMUM_PARAMETERS){
    HANDLE thread=OpenThread(THREAD_QUERY_LIMITED_INFORMATION,FALSE,fault.thread);
    const bool owned=thread&&GetProcessIdOfThread(thread)==process.dwProcessId;if(thread)CloseHandle(thread);
    if(owned){++crashes;if(!directory.empty())RecordException(process.hProcess,process.dwProcessId,fault,directory,stem+L"_crash"+std::to_wstring(crashes),&memory,GetTickCount64()-start);}
   }
   InterlockedExchange(&channel.shared->state,3);
  }
  if(channel.ack)SetEvent(channel.ack);
 }
 if(!directory.empty()){FILE* file{};_wfopen_s(&file,(directory+L"\\"+stem+L"_session.json").c_str(),L"wb");if(file){std::fprintf(file,"{\"schema\":3,\"build\":\"v145\",\"pid\":%lu,\"exit_code\":\"%08lx\",\"unhandled_exceptions\":%lu,\"debugger\":false,\"duration_ms\":%llu,\"input_log\":\"input-enhanced-%lu.log\",\"render_log\":\"d3d9-%lu.log\",\"memory_telemetry\":",process.dwProcessId,exitCode,crashes,GetTickCount64()-start,process.dwProcessId,process.dwProcessId);WriteHistory(file,memory);std::fprintf(file,"}\n");std::fclose(file);}}
 return exitCode;
}
}
