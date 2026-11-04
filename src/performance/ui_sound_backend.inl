// Game-local D3D component. Lazy initialization outside DllMain; no OS-volume writes.
#include "ui_sound.h"
#include <mmsystem.h>
#include <dsound.h>
#include <wrl/client.h>
#include <vector>
#include <cmath>
#include <algorithm>
namespace ui_sound_backend {
using Microsoft::WRL::ComPtr;
static bool enabled=false;static wchar_t directory[MAX_PATH]{};
static SRWLOCK lock=SRWLOCK_INIT;
struct Clip{WAVEFORMATEX format{};std::vector<BYTE> pcm;ComPtr<IDirectSoundBuffer> voices[3];};
struct State{ComPtr<IDirectSound8> device;Clip clips[3];HWND window{};DWORD retry{},lastTick{};ui_sound::Cue last{};float appliedGain=-1;};
static State* state=nullptr; // Process lifetime, pinned; never release DirectSound in DllMain.
static unsigned plays=0,coalesced=0,failures=0,volumeUpdates=0;static float lastGain=0;
static DWORD Word(const BYTE* p){DWORD v{};memcpy(&v,p,4);return v;}
static bool Decode(const std::vector<BYTE>& bytes,Clip& out){
 if(bytes.size()<44||memcmp(bytes.data(),"RIFF",4)||memcmp(bytes.data()+8,"WAVE",4)||Word(bytes.data()+4)!=bytes.size()-8)return false;
 bool format=false,data=false;WAVEFORMATEX fmt{};std::vector<BYTE> pcm;size_t at=12;
 for(;at+8<=bytes.size();){const DWORD size=Word(bytes.data()+at+4);const size_t begin=at+8;if(size>bytes.size()-begin)return false;
  if(!memcmp(bytes.data()+at,"fmt ",4)){if(format||size<16)return false;memcpy(&fmt,bytes.data()+begin,16);format=true;}
  else if(!memcmp(bytes.data()+at,"data",4)){if(data||!size)return false;pcm.assign(bytes.begin()+begin,bytes.begin()+begin+size);data=true;}
  at=begin+size+(size&1u);if(at>bytes.size())return false;
 }
 if(at!=bytes.size()||!format||!data||fmt.wFormatTag!=WAVE_FORMAT_PCM||(fmt.nChannels!=1&&fmt.nChannels!=2)||fmt.wBitsPerSample!=16||
    (fmt.nSamplesPerSec!=44100&&fmt.nSamplesPerSec!=48000)||fmt.nBlockAlign!=fmt.nChannels*2||
    fmt.nAvgBytesPerSec!=fmt.nSamplesPerSec*fmt.nBlockAlign||pcm.size()%fmt.nBlockAlign||pcm.size()>fmt.nAvgBytesPerSec*2)return false;
 out.format=fmt;out.pcm=std::move(pcm);return true;
}
static bool LoadEmbedded(const wchar_t* name,Clip& out){
 // The optional licensed sound effects are combined with this component as
 // resources. The public source deliberately contains no standalone audio.
 WORD id=0;if(!wcscmp(name,L"hover"))id=301;else if(!wcscmp(name,L"confirm"))id=302;else if(!wcscmp(name,L"back"))id=303;
 if(!id||!selfModule)return false;
 const HRSRC resource=FindResourceW(selfModule,MAKEINTRESOURCEW(id),MAKEINTRESOURCEW(10));if(!resource)return false; // RT_RCDATA
 const DWORD size=SizeofResource(selfModule,resource);if(size<44||size>1024*1024)return false;
 const HGLOBAL loaded=LoadResource(selfModule,resource);if(!loaded)return false;
 const auto* begin=static_cast<const BYTE*>(LockResource(loaded));if(!begin)return false;
 try{const std::vector<BYTE> bytes(begin,begin+size);return Decode(bytes,out);}catch(...){return false;}
}
static bool Load(const wchar_t* name,Clip& out){
 if(LoadEmbedded(name,out))return true;
 // Optional local replacement clips remain available to private/custom installs.
 wchar_t path[MAX_PATH]{};if(swprintf_s(path,L"%s\\%s.wav",directory,name)<0)return false;
 HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)return false;
 LARGE_INTEGER size{};bool good=GetFileSizeEx(file,&size)&&size.QuadPart>=44&&size.QuadPart<=1024*1024;std::vector<BYTE> bytes;
 try{if(good){bytes.resize(static_cast<size_t>(size.QuadPart));DWORD read{};good=ReadFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&read,nullptr)&&read==bytes.size();}}catch(...){good=false;}
 CloseHandle(file);return good&&Decode(bytes,out);
}
static bool BusGain(BYTE** buses,DWORD count,float& gain)noexcept{
 __try{
  if(!buses||!count||count>128)return false;
  for(DWORD i=0;i<count;++i)if(buses[i]&&*reinterpret_cast<DWORD*>(buses[i]+0x2c)==0){
   const float value=*reinterpret_cast<float*>(buses[i]+0x94);if(!std::isfinite(value)||value<0||value>1)return false;gain=value;return true;
  }
 }__except(EXCEPTION_EXECUTE_HANDLER){}return false;
}
static bool Master(float& gain)noexcept{
 // 44A820 writes the effective live master bus gain;5273D0 resolves bus type0.
 // Read that actual bus, including uncommitted Audio-slider changes and Cancel.
 __try{return BusGain(*reinterpret_cast<BYTE***>(0xfe13ac),*reinterpret_cast<DWORD*>(0xfe13b4),gain);}
 __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool Upload(Clip& clip,IDirectSoundBuffer* voice){
 void *a=nullptr,*b=nullptr;DWORD an=0,bn=0;
 if(FAILED(voice->Lock(0,static_cast<DWORD>(clip.pcm.size()),&a,&an,&b,&bn,0)))return false;
 const bool good=an+bn==clip.pcm.size()&&a&&(!bn||b);if(good){memcpy(a,clip.pcm.data(),an);if(bn)memcpy(b,clip.pcm.data()+an,bn);}
 return SUCCEEDED(voice->Unlock(a,an,b,bn))&&good;
}
static bool Prepare(State& s,HWND window){
 if(s.window==window&&s.device)return true;
 const DWORD now=GetTickCount();if(s.retry&&static_cast<LONG>(now-s.retry)<0)return false;s.retry=now+2000;
 for(auto& c:s.clips){for(auto& v:c.voices)v.Reset();c.pcm.clear();}s.device.Reset();s.window=nullptr;
 const wchar_t* names[]={L"hover",L"confirm",L"back"};for(unsigned i=0;i<3;++i)if(!Load(names[i],s.clips[i]))return false;
 ComPtr<IDirectSound8> device;if(FAILED(DirectSoundCreate8(nullptr,&device,nullptr))||FAILED(device->SetCooperativeLevel(window,DSSCL_NORMAL)))return false;
 for(auto& c:s.clips){DSBUFFERDESC desc{};desc.dwSize=sizeof desc;desc.dwFlags=DSBCAPS_CTRLVOLUME|DSBCAPS_GETCURRENTPOSITION2;desc.dwBufferBytes=static_cast<DWORD>(c.pcm.size());desc.lpwfxFormat=&c.format;
  for(auto& v:c.voices)if(FAILED(device->CreateSoundBuffer(&desc,&v,nullptr))||!Upload(c,v.Get()))return false;}
 s.device=std::move(device);s.window=window;s.retry=0;s.appliedGain=-1;Log("Modern UI sounds ready: three PCM clips; live native master bus");return true;
}
static bool ApplyGain(State& s,float gain){
 if(!std::isfinite(gain)||gain<0||gain>1)return false;
 if(s.appliedGain==gain)return true;
 const LONG volume=gain==0?DSBVOLUME_MIN:static_cast<LONG>(std::clamp(2000.f*std::log10(gain),-10000.f,0.f));
 bool good=true;for(auto& c:s.clips)for(auto& v:c.voices)if(v&&FAILED(v->SetVolume(volume)))good=false;
 if(good){s.appliedGain=gain;lastGain=gain;++volumeUpdates;}return good;
}
static void Refresh()noexcept{
 if(!enabled)return;const DWORD error=GetLastError();
 if(TryAcquireSRWLockExclusive(&lock)){
  float gain{};if(state&&Master(gain))ApplyGain(*state,gain);
  ReleaseSRWLockExclusive(&lock);
 }SetLastError(error);
}
static bool Play(ui_sound::Cue cue)noexcept{
 if(!enabled||cue<ui_sound::Cue::Hover||cue>ui_sound::Cue::Back)return false;
 const DWORD error=GetLastError();bool result=false;AcquireSRWLockExclusive(&lock);
 try{
  DWORD pid{};const HWND window=GetForegroundWindow();float gain{};
  if(window&&!IsIconic(window)&&GetWindowThreadProcessId(window,&pid)&&pid==GetCurrentProcessId()&&Master(gain)){
   if(!state){state=new State;HMODULE pinned=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&Play),&pinned);}
   auto& s=*state;if(Prepare(s,window)&&ApplyGain(s,gain)){
    const DWORD now=GetTickCount();lastGain=gain;
    // Own presenter and the corresponding native AVM can describe one action
    // within adjacent frames. Coalesce that pair, and cap rapid hover chatter.
    if(s.last==cue&&now-s.lastTick<(cue==ui_sound::Cue::Hover?55u:90u)){++coalesced;result=true;}
    else if(gain==0){s.last=cue;s.lastTick=now;result=true;}
    else{
     auto& c=s.clips[static_cast<unsigned>(cue)-1];IDirectSoundBuffer* voice=nullptr;
     for(auto& v:c.voices){DWORD status{};if(FAILED(v->GetStatus(&status)))continue;
      if(status&DSBSTATUS_BUFFERLOST){if(FAILED(v->Restore())||!Upload(c,v.Get()))continue;status=0;}
      if(!(status&DSBSTATUS_PLAYING)){voice=v.Get();break;}}
     const LONG volume=static_cast<LONG>(std::clamp(2000.f*std::log10(gain),-10000.f,0.f));
     // Never cut an already playing transient mid-wave; a tiny fixed voice pool
     // absorbs rapid navigation. If saturated, coalesce rather than accumulate.
     const bool good=!voice||(SUCCEEDED(voice->SetCurrentPosition(0))&&SUCCEEDED(voice->SetVolume(volume))&&SUCCEEDED(voice->Play(0,0,0)));
     if(good){s.last=cue;s.lastTick=now;if(voice)++plays;else ++coalesced;result=true;}
    }
   }
  }
 }catch(...){result=false;}
 if(!result)++failures;ReleaseSRWLockExclusive(&lock);SetLastError(error);return result;
}
static void Configure(const wchar_t* config,bool supported){
 enabled=supported&&ConfigFlag(L"UI",L"ModernSounds",false,config);if(!enabled)return;
 wchar_t module[MAX_PATH]{};if(!GetModuleFileNameW(selfModule,module,MAX_PATH)){enabled=false;return;}auto* slash=wcsrchr(module,L'\\');if(!slash){enabled=false;return;}*slash=0;
 if(swprintf_s(directory,L"%s\\enhanced-ui\\audio",module)<0){enabled=false;return;}ui_sound::SetSink(Play);
}
}
extern "C" BOOL WINAPI AITDPlayUiSound(DWORD cue){return ui_sound_backend::Play(static_cast<ui_sound::Cue>(cue))?TRUE:FALSE;}
