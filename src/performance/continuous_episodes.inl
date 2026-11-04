// Exact native film request boundaries only. Keep the episode manager's save,
// unlock, continuation and Main Menu paths authoritative.
namespace continuous_episodes {
static bool requested=false,installed=false;
static volatile LONG installAttempt=0;
static unsigned creditsSkipped{},recapsSkipped{},lastCreditsPhase{};
using CreditsFunction=void(__cdecl*)();
using MovieFunction=bool(WINAPI*)(const char*,DWORD);
static CreditsFunction nativeCredits=reinterpret_cast<CreditsFunction>(0x455d50);
static MovieFunction nativeMovie=reinterpret_cast<MovieFunction>(0x559890);
static void Configure(const wchar_t* config,bool supported){
 requested=supported&&ConfigFlag(L"Gameplay",L"ContinuousEpisodes",false,config);
}
static bool Requested(){return requested&&installed;}
static bool RecapPath(const char* path,DWORD flags) noexcept {
 if(!path||flags!=0x2f)return false;
 __try {
  constexpr char prefix[]="Previously\\";
  for(unsigned i=0;i<sizeof(prefix)-1;++i){
   char c=path[i];if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');
   char expected=prefix[i];if(expected>='A'&&expected<='Z')expected=static_cast<char>(expected-'A'+'a');
   if(c!=expected)return false;
  }
  unsigned end=sizeof(prefix)-1;
  for(;end<260&&path[end];++end)if(path[end]=='/'||path[end]=='\\')return false;
  if(end>=260||end<sizeof(prefix)+4)return false;
  return path[end-4]=='.'&&(path[end-3]=='b'||path[end-3]=='B')&&
   (path[end-2]=='i'||path[end-2]=='I')&&(path[end-1]=='k'||path[end-1]=='K');
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static void __cdecl Credits(){
 unsigned phase=0;
 __try{phase=*reinterpret_cast<unsigned*>(0xfda644);}__except(EXCEPTION_EXECUTE_HANDLER){}
 // 454CB0 invokes only Credits1/2 at phase4 (saving) and5 (episode choices).
 // Main-menu CreditsFull uses the independent53EE50 request.
 if(Requested()&&(phase==4||phase==5)){
  if(lastCreditsPhase!=phase){lastCreditsPhase=phase;++creditsSkipped;
   Log("Continuous episodes: skipped inter-episode credits request phase=%u total=%u",phase,creditsSkipped);}
  return;
 }
 lastCreditsPhase=0;nativeCredits();
}
static bool WINAPI PreviousMovie(const char* path,DWORD flags){
 // This replaces only455930's Previously request. False follows its existing
 // missing-recap branch, without entering movie state5 or touching loading.
 if(Requested()&&RecapPath(path,flags)){
  ++recapsSkipped;Log("Continuous episodes: skipped native Previously video total=%u",recapsSkipped);return false;
 }
 return nativeMovie(path,flags);
}
static void Install(){
 if(!requested||InterlockedCompareExchange(&installAttempt,1,0))return;
 struct Patch {DWORD address;unsigned char expected[5];void* replacement;DWORD protection;};
 Patch sites[]={
  {0x4551f5,{0xe8,0x56,0x0b,0,0},reinterpret_cast<void*>(&Credits),0},
  {0x455202,{0xe8,0x49,0x0b,0,0},reinterpret_cast<void*>(&Credits),0},
  {0x455972,{0xe8,0x19,0x3f,0x10,0},reinterpret_cast<void*>(&PreviousMovie),0}};
 for(const auto& s:sites)if(memcmp(reinterpret_cast<void*>(s.address),s.expected,5)){
  requested=false;Log("Continuous episodes signature mismatch at %08lx",s.address);return;
 }
 unsigned writable=0;
 for(auto& s:sites){if(!VirtualProtect(reinterpret_cast<void*>(s.address),5,PAGE_EXECUTE_READWRITE,&s.protection))break;++writable;}
 if(writable!=3){
  for(int i=static_cast<int>(writable)-1;i>=0;--i){DWORD ignored{};VirtualProtect(reinterpret_cast<void*>(sites[i].address),5,sites[i].protection,&ignored);}
  requested=false;Log("Continuous episodes native page access failed");return;
 }
 for(const auto& s:sites){auto* out=reinterpret_cast<unsigned char*>(s.address);out[0]=0xe8;
  *reinterpret_cast<DWORD*>(out+1)=static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(s.replacement)-s.address-5);
  FlushInstructionCache(GetCurrentProcess(),out,5);
 }
 for(int i=2;i>=0;--i){DWORD ignored{};VirtualProtect(reinterpret_cast<void*>(sites[i].address),5,sites[i].protection,&ignored);}
 installed=true;Log("Continuous episodes installed: inter-episode Credits1/2 and Previously only; native saving, choices, main-menu CreditsFull and previews retained");
}
}
