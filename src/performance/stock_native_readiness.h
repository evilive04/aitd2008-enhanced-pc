#pragma once
#include <windows.h>
#include <bcrypt.h>
#include "../shared/executable_identity.h"
#include <cstdint>
#include <cstring>
// Read-only admission checks on normally loaded native game sections. No
// decryption, protection-section access, activation emulation or EXE writes.
// Each feature's existing exact signatures/ownership checks remain mandatory.
namespace stock_native_readiness {
enum class Result : unsigned {Ready,WrongBase,NativeCodeUnavailable,NativeDataUnavailable,NativeSignaturesUnavailable};
inline bool ReadableImage(std::uintptr_t first,std::uintptr_t last){
 if(first>=last)return false;
 while(first<last){
  MEMORY_BASIC_INFORMATION memory{};
  if(VirtualQuery(reinterpret_cast<const void*>(first),&memory,sizeof memory)!=sizeof memory||
     memory.AllocationBase!=GetModuleHandleW(nullptr)||memory.Type!=MEM_IMAGE||memory.State!=MEM_COMMIT||
     (memory.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
  const DWORD access=memory.Protect&0xff;
  if(access!=PAGE_READONLY&&access!=PAGE_READWRITE&&access!=PAGE_WRITECOPY&&
     access!=PAGE_EXECUTE_READ&&access!=PAGE_EXECUTE_READWRITE&&access!=PAGE_EXECUTE_WRITECOPY)return false;
  const auto next=reinterpret_cast<std::uintptr_t>(memory.BaseAddress)+memory.RegionSize;
  if(next<=first)return false;first=next;
 }
 return true;
}
inline Result Check(){
 if(GetModuleHandleW(nullptr)!=reinterpret_cast<HMODULE>(0x400000))return Result::WrongBase;
 // Only original .text, .rdata and .data; do not inspect the protection ranges.
 if(!ReadableImage(0x401000,0xb84b2b))return Result::NativeCodeUnavailable;
 if(!ReadableImage(0xb85000,0xcf275d)||!ReadableImage(0xcf3000,0x123414c))return Result::NativeDataUnavailable;
 // Stable native timer and frame-wait instructions, checked before our
 // frame-wait hook. Uninitialized bytes never admit native feature hooks.
 const BYTE timer[]={0x8b,0x08,0x83,0xf9,0x03,0x77,0x35,0xff};
 const BYTE frameWait[]={0xd8,0x35,0x8c,0xea,0xfa,0x00};
 __try{
  if(std::memcmp(reinterpret_cast<const void*>(0x63af50),timer,sizeof timer)||
     std::memcmp(reinterpret_cast<const void*>(0x41a1bc),frameWait,sizeof frameWait))return Result::NativeSignaturesUnavailable;
 }__except(EXCEPTION_EXECUTE_HANDLER){return Result::NativeSignaturesUnavailable;}
 return Result::Ready;
}
}

// Exact on-disk identity is independently required before native admission.
// Reports API-stage/error and the computed public game hash for diagnostics;
// unavailable or mismatching bytes are never accepted.
namespace stock_file_identity {
struct Report {bool matched{};unsigned stage{};DWORD error{};unsigned long long totalBytes{};wchar_t imagePath[MAX_PATH]{},openedPath[MAX_PATH]{};char sha256[65]{};};
inline Report Verify(){
 Report report{};wchar_t path[MAX_PATH]{};
 report.stage=1;const DWORD length=GetModuleFileNameW(nullptr,path,MAX_PATH);
 if(!length||length>=MAX_PATH){report.error=GetLastError();return report;}
 memcpy(report.imagePath,path,(length+1)*sizeof(wchar_t));
 report.stage=2;HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
 if(file==INVALID_HANDLE_VALUE){report.error=GetLastError();return report;}
 if(GetFinalPathNameByHandleW(file,report.openedPath,MAX_PATH,FILE_NAME_NORMALIZED)>=MAX_PATH)report.openedPath[0]=0;
 BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;BYTE digest[32]{};BYTE buffer[65536]{};
 report.stage=3;NTSTATUS status=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0);
 bool okay=status>=0;if(!okay)report.error=static_cast<DWORD>(status);
 if(okay){report.stage=4;status=BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0);okay=status>=0;if(!okay)report.error=static_cast<DWORD>(status);}
 while(okay){
  DWORD count{};report.stage=5;if(!ReadFile(file,buffer,sizeof buffer,&count,nullptr)){report.error=GetLastError();okay=false;break;}
  if(!count)break;report.totalBytes+=count;report.stage=6;status=BCryptHashData(hash,buffer,count,0);okay=status>=0;if(!okay)report.error=static_cast<DWORD>(status);
 }
 if(okay){report.stage=7;status=BCryptFinishHash(hash,digest,sizeof digest,0);okay=status>=0;if(!okay)report.error=static_cast<DWORD>(status);}
 if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);CloseHandle(file);
 if(okay){
  constexpr char digits[]="0123456789abcdef";
  for(unsigned n=0;n<32;++n){report.sha256[n*2]=digits[digest[n]>>4];report.sha256[n*2+1]=digits[digest[n]&15];}
  report.matched=executable_identity::SupportedDigest(digest,sizeof digest);report.stage=report.matched?8u:9u;
 }
 return report;
}
}
