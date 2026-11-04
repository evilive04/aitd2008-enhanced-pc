// Included inside cloth_rigidity_rate. The native threaded cloth solver lives
// in its own task DLL, not in Alone.exe's serial4E8410 implementation.
static SRWLOCK asyncInstallLock=SRWLOCK_INIT;
static HMODULE asyncPreparedModule=nullptr,asyncBlockedExistingModule=nullptr;
static void* asyncFactoryResume=reinterpret_cast<void*>(0xb5e5b2);
static void AsyncEntryAdapter();
static bool AsyncPathAndDigest(HMODULE module){
 wchar_t path[MAX_PATH]{},exe[MAX_PATH]{};
 const DWORD n=GetModuleFileNameW(module,path,MAX_PATH),m=GetModuleFileNameW(nullptr,exe,MAX_PATH);
 if(!n||n>=MAX_PATH||!m||m>=MAX_PATH)return false;
 const auto* name=wcsrchr(path,L'\\');const auto* exeName=wcsrchr(exe,L'\\');
 if(!name||!exeName||_wcsicmp(name+1,L"edPhysClothTask_Master.dll")||
    name-path!=exeName-exe||_wcsnicmp(path,exe,static_cast<size_t>(name-path)))return false;
 HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
 if(file==INVALID_HANDLE_VALUE)return false;
 BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
 BYTE digest[32]{};bool ok=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0;
 if(ok)ok=BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0;
 BYTE buffer[16384];DWORD size=0;
 while(ok){
  if(!ReadFile(file,buffer,sizeof buffer,&size,nullptr)){ok=false;break;}
  if(!size)break;
  ok=BCryptHashData(hash,buffer,size,0)>=0;
 }
 if(ok)ok=BCryptFinishHash(hash,digest,sizeof digest,0)>=0;
 if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);
 CloseHandle(file);
 const BYTE expected[]={0x6f,0x6e,0x53,0xd6,0xa4,0xda,0x2a,0x44,0x3c,0xcd,0xfa,0xaa,0x29,0x56,0x32,0x0f,
  0xf2,0xcb,0xf8,0xe8,0xa8,0x2f,0x54,0x66,0x4c,0x15,0xda,0x02,0x8c,0xdb,0x33,0x90};
 return ok&&!memcmp(digest,expected,sizeof expected);
}
static bool AsyncHeader(BYTE* base,FARPROC entry){
 const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
 if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<0||dos->e_lfanew>0x1000)return false;
 const auto* pe=reinterpret_cast<const IMAGE_NT_HEADERS32*>(base+dos->e_lfanew);
 if(pe->Signature!=IMAGE_NT_SIGNATURE||pe->FileHeader.Machine!=IMAGE_FILE_MACHINE_I386||
    pe->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC||pe->OptionalHeader.SizeOfImage!=0x9000||
    entry!=reinterpret_cast<FARPROC>(base+0x38b0))return false;
 const BYTE entryPrefix[]={0x6a,0x30,0xe8,0xe9,0x02,0x00,0x00,0x83,0xc4,0x04,0x85,0xc0};
 const BYTE worker[]={0x8b,0x41,0x20,0x50,0xe8,0x67,0xf1,0xff,0xff,0xc3};
 BYTE producer[]={0x0f,0x28,0xcc,0xf3,0x0f,0x5e,0x4b,0x40,0xf3,0x0f,0x11,0x4c,0x24,0x10,
  0xf3,0x0f,0x59,0x0d,0,0,0,0};
 const DWORD sixty=reinterpret_cast<DWORD>(base+0x5270);memcpy(producer+18,&sixty,4);
 BYTE oneLoad[]={0xf3,0x0f,0x10,0x25,0,0,0,0};
 const DWORD one=reinterpret_cast<DWORD>(base+0x5238);memcpy(oneLoad+4,&one,4);
 return !memcmp(base+0x38b0,entryPrefix,sizeof entryPrefix)&&!memcmp(base+0x3870,worker,sizeof worker)&&
  !memcmp(base+0x29ec,oneLoad,sizeof oneLoad)&&
  !memcmp(base+0x2a06,producer,sizeof producer)&&*reinterpret_cast<const DWORD*>(base+0x5270)==0x42700000u&&
  *reinterpret_cast<const DWORD*>(base+0x5238)==0x3f800000u;
}
static bool AsyncOwnsSites(BYTE* base){
 const DWORD rvas[]={0x2aca,0x2b02,0x2b20,0x2b51};
 for(unsigned n=0;n<_countof(rvas);++n){
  const auto* code=base+rvas[n];DWORD displacement=0;memcpy(&displacement,code+1,4);
  const auto* target=n?reinterpret_cast<const BYTE*>(&ScaleRigid3):reinterpret_cast<const BYTE*>(&ScaleRigid0);
  if(code[0]!=0xe8||code[5]!=0x90||code[6]!=0x90||
     reinterpret_cast<DWORD>(code)+5+displacement!=reinterpret_cast<DWORD>(target))return false;
 }
 return true;
}
static void PrepareAsyncModule(HMODULE module,FARPROC entry){
 if(!module||module==asyncBlockedExistingModule)return;
 auto* base=reinterpret_cast<BYTE*>(module);
 // Other native task types share the factory callsite; leave them untouched.
 if(entry!=reinterpret_cast<FARPROC>(base+0x38b0)||GetProcAddress(module,"GetEntryPoint")!=entry)return;
 if(asyncPreparedModule==module){
  if(!AsyncOwnsSites(base))Log("Cloth async correction skipped: previously installed task code changed");
  return;
 }
 if(!AsyncHeader(base,entry)||!AsyncPathAndDigest(module)){
  Log("Cloth async correction skipped: task module identity/signatures differ");return;
 }
 struct Site{DWORD rva;DWORD protection;};
 Site sites[]={{0x2aca,0},{0x2b02,0},{0x2b20,0},{0x2b51,0}};
 const BYTE rigid0[]={0xf3,0x0f,0x59,0xc1,0x0f,0x28,0xd4};
 const BYTE rigid3[]={0xf3,0x0f,0x59,0xd9,0x0f,0x28,0xd4};
 for(unsigned n=0;n<_countof(sites);++n)if(memcmp(base+sites[n].rva,n?rigid3:rigid0,sizeof rigid0)){
  Log("Cloth async correction skipped: geometric site signature differs");return;
 }
 // Pin the authenticated module for this process: installed call targets must
 // never become stale through a later task unload/reload at the same address.
 HMODULE pinned=nullptr;
 if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
      reinterpret_cast<LPCWSTR>(module),&pinned)||pinned!=module){
  Log("Cloth async correction skipped: task lifetime could not be retained");return;
 }
 unsigned writable=0;
 for(auto& site:sites){
  if(!VirtualProtect(base+site.rva,sizeof rigid0,PAGE_EXECUTE_READWRITE,&site.protection))break;
  ++writable;
 }
 if(writable!=_countof(sites)){
  for(int n=static_cast<int>(writable)-1;n>=0;--n){DWORD ignored;VirtualProtect(base+sites[n].rva,sizeof rigid0,sites[n].protection,&ignored);}
  Log("Cloth async correction skipped: native page access failed");return;
 }
 for(unsigned n=0;n<_countof(sites);++n){
  auto* code=base+sites[n].rva;const auto* adapter=n?reinterpret_cast<const BYTE*>(&ScaleRigid3):reinterpret_cast<const BYTE*>(&ScaleRigid0);
  code[0]=0xe8;const DWORD displacement=reinterpret_cast<DWORD>(adapter)-reinterpret_cast<DWORD>(code)-5;
  memcpy(code+1,&displacement,4);code[5]=0x90;code[6]=0x90;
  FlushInstructionCache(GetCurrentProcess(),code,sizeof rigid0);
 }
 bool restored=true;
 for(int n=static_cast<int>(_countof(sites))-1;n>=0;--n){DWORD ignored;
  if(!VirtualProtect(base+sites[n].rva,sizeof rigid0,sites[n].protection,&ignored))restored=false;
 }
 asyncPreparedModule=module;
 Log("Cloth async rigidity correction installed before task entry: module=%p sites=4 page_protection=%d",module,restored);
}
static void __cdecl PrepareAsyncTask(void* task,FARPROC entry){
 const DWORD savedError=GetLastError();
 AcquireSRWLockExclusive(&asyncInstallLock);
 __try{
  if(requested&&task)PrepareAsyncModule(*reinterpret_cast<HMODULE*>(static_cast<BYTE*>(task)+0xe0),entry);
 }__except(EXCEPTION_EXECUTE_HANDLER){Log("Cloth async correction skipped: task identity read failed");}
 ReleaseSRWLockExclusive(&asyncInstallLock);SetLastError(savedError);
}
// Replaces only CALL EAX / MOV[EDI+D0],EAX. JMP entry means there is no extra
// return address when the original GetEntryPoint executes; its stack is exact.
static __declspec(naked) void AsyncEntryAdapter(){
 __asm {
  push ebp
  mov ebp,esp
  pushfd
  pushad
  sub esp,528
  and esp,0fffffff0h
  fxsave [esp]
  push eax
  push edi
  call PrepareAsyncTask
  add esp,8
  fxrstor [esp]
  lea esp,[ebp-36]
  popad
  popfd
  mov esp,ebp
  pop ebp
  call eax
  mov [edi+0d0h],eax
  jmp dword ptr [asyncFactoryResume]
 }
}
static void InstallAsync(){
 const BYTE expected[]={0xff,0xd0,0x89,0x87,0xd0,0x00,0x00,0x00};
 auto* code=reinterpret_cast<BYTE*>(0xb5e5aa);
 if(memcmp(code,expected,sizeof expected)){Log("Cloth async factory hook skipped: signature mismatch");return;}
 const DWORD savedError=GetLastError();
 AcquireSRWLockExclusive(&asyncInstallLock);
 HMODULE existing=GetModuleHandleW(L"edPhysClothTask_Master.dll");
 // Existing modules may already have queued or running jobs. A menu/no-hero
 // observation does not prove otherwise, so never patch an existing module.
 if(existing)asyncBlockedExistingModule=existing;
 DWORD protection=0;
 if(!VirtualProtect(code,sizeof expected,PAGE_EXECUTE_READWRITE,&protection)){
  ReleaseSRWLockExclusive(&asyncInstallLock);SetLastError(savedError);
  Log("Cloth async factory hook skipped: native page access failed");return;
 }
 code[0]=0xe9;const DWORD displacement=reinterpret_cast<DWORD>(&AsyncEntryAdapter)-0xb5e5aa-5;
 memcpy(code+1,&displacement,4);memset(code+5,0x90,3);FlushInstructionCache(GetCurrentProcess(),code,sizeof expected);
 DWORD ignored;const bool restored=VirtualProtect(code,sizeof expected,protection,&ignored)!=FALSE;
 if(existing)Log("Cloth async correction deferred: task DLL predates factory hook; earlier bootstrap required");
 Log("Cloth async factory hook installed: initial_module=%p page_protection=%d",existing,restored);
 ReleaseSRWLockExclusive(&asyncInstallLock);SetLastError(savedError);
}
