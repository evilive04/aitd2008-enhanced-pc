#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <cstring>
#include <cstdio>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <stdexcept>
static bool GammaEnabled=true;
static HMODULE selfModule=reinterpret_cast<HMODULE>(1);
static HWND testWindow=reinterpret_cast<HWND>(0x100);
static HWND testForeground=testWindow;
static bool hookFailure=false;
static DWORD testWindowThread=1234;
static HOOKPROC installedObserver=nullptr;
static DWORD WINAPI FakeWindowThread(HWND,LPDWORD process) {*process=GetCurrentProcessId();return testWindowThread;}
static BOOL WINAPI FakeIsWindow(HWND w) {return w!=nullptr;}
static BOOL WINAPI FakeIsIconic(HWND) {return FALSE;}
static HWND WINAPI FakeForeground() {return testForeground;}
static HWND WINAPI FakeAncestor(HWND w,UINT) {return w;}
static HHOOK WINAPI FakeInstallHook(int,HOOKPROC p,HINSTANCE,DWORD) {installedObserver=p;return hookFailure ? nullptr : reinterpret_cast<HHOOK>(1);}
static LRESULT WINAPI FakeNext(HHOOK,int,WPARAM,LPARAM) {return 0;}
static std::atomic<unsigned> applications{0};
static std::function<void(const D3DGAMMARAMP*)> action;
static void STDMETHODCALLTYPE FakeGamma(IDirect3DDevice9*,UINT,DWORD,const D3DGAMMARAMP* ramp) {++applications;const auto callback=action;if(callback)callback(ramp);}
static void* Original(void*,unsigned slot) {if(slot!=21)throw std::runtime_error("slot");return reinterpret_cast<void*>(&FakeGamma);}
#define GetWindowThreadProcessId FakeWindowThread
#define IsWindow FakeIsWindow
#define IsIconic FakeIsIconic
#define GetForegroundWindow FakeForeground
#define GetAncestor FakeAncestor
#define SetWindowsHookExW FakeInstallHook
#define CallNextHookEx FakeNext
#include "gamma_cache.inl"
#undef GetWindowThreadProcessId
#undef IsWindow
#undef IsIconic
#undef GetForegroundWindow
#undef GetAncestor
#undef SetWindowsHookExW
#undef CallNextHookEx
static void Check(bool ok,const char* text) {if(!ok)throw std::runtime_error(text);}
static void Event(UINT id) {CWPSTRUCT event{};event.hwnd=testWindow;event.message=id;Check(installedObserver!=nullptr,"observer installed");installedObserver(HC_ACTION,0,reinterpret_cast<LPARAM>(&event));}
static IDirect3DDevice9* Device(unsigned id) {return reinterpret_cast<IDirect3DDevice9*>(static_cast<UINT_PTR>(0x1000+id*16));}
static void Call(IDirect3DDevice9* device,const D3DGAMMARAMP& ramp,UINT swap=0,DWORD flags=0) {ObservedGamma(device,swap,flags,&ramp);}
int main() {
 try {
  auto* device=Device(0);D3DGAMMARAMP a{},b{};a.red[0]=1;b.red[0]=2;
  Check(GammaRegister(device,testWindow),"register");
  Call(device,a);Call(device,a);Check(applications==1,"identical skip");
  Call(device,b);Call(device,b);Check(applications==2,"changed LUT");
  Call(device,b,0,1);Check(applications==3,"flags");
  Call(device,b,1);Call(device,b,0,1);Check(applications==5,"unsupported swap invalidates");
  ObservedGamma(device,0,0,nullptr);Call(device,b,0,1);Check(applications==7,"null invalidates");
  GammaInvalidate(device);Call(device,a);const unsigned beforeFocus=applications;
  Event(WM_ACTIVATEAPP);Event(WM_ACTIVATEAPP);Call(device,a);Call(device,a);Check(applications==beforeFocus+1,"unobserved focus cycle invalidates");
  Event(WM_DISPLAYCHANGE);Call(device,a);const unsigned beforeReset=applications;
  action=[&](const D3DGAMMARAMP*){action=nullptr;GammaInvalidate(device);};
  GammaInvalidate(device);Call(device,a);Call(device,a);Check(applications==beforeReset+2,"invalidate during COM prevents commit");
  GammaInvalidate(device);bool nested=false;
  action=[&](const D3DGAMMARAMP*){if(!nested){nested=true;Call(device,b);}};
  const unsigned beforeReentrant=applications;Call(device,a);action=nullptr;Call(device,b);Call(device,b);
  Check(applications==beforeReentrant+3,"reentrant overlap invalidates both");
  // Real native threads, deliberately exercise both overlapping completion orders.
  for(bool firstFinishesFirst:{false,true}) {
   GammaInvalidate(device);std::mutex mutex;std::condition_variable cv;unsigned entered=0;bool releaseA=false,releaseB=false;
   action=[&](const D3DGAMMARAMP* ramp){std::unique_lock<std::mutex> lock(mutex);++entered;cv.notify_all();cv.wait(lock,[&]{return ramp->red[0]==1 ? releaseA : releaseB;});};
   const unsigned before=applications;
   std::thread first([&]{Call(device,a);});
   {std::unique_lock<std::mutex> lock(mutex);cv.wait(lock,[&]{return entered==1;});}
   std::thread second([&]{Call(device,b);});
   {std::unique_lock<std::mutex> lock(mutex);cv.wait(lock,[&]{return entered==2;});if(firstFinishesFirst)releaseA=true;else releaseB=true;cv.notify_all();}
   if(firstFinishesFirst)first.join();else second.join();
   {std::lock_guard<std::mutex> lock(mutex);releaseA=releaseB=true;cv.notify_all();}
   if(firstFinishesFirst)second.join();else first.join();
   action=nullptr;Call(device,b);Call(device,b);Check(applications==before+3,"concurrent overlap leaves cache invalid");
  }
  const auto pendingOldLifetime=GammaGeneration(device);
  Check(GammaRegister(device,testWindow),"same pointer re-register before old Forget");
  const auto freshLifetime=GammaGeneration(device);Check(freshLifetime!=pendingOldLifetime,"registration always renews lifetime");
  GammaForget(device,pendingOldLifetime);Check(GammaGeneration(device)==freshLifetime,"late old Forget preserves newly registered same pointer");
  const auto oldLifetime=GammaGeneration(device);GammaForget(device,oldLifetime);Check(GammaRegister(device,testWindow),"reuse register");GammaForget(device,oldLifetime);Check(GammaGeneration(device)!=0,"late old Release cannot forget new lifetime");const unsigned beforeReuse=applications;Call(device,b);Call(device,b);Check(applications==beforeReuse+1,"reused address fresh cache");
  Event(WM_NCDESTROY);const unsigned beforeDestroyed=applications;Call(device,b);Call(device,b);Check(applications==beforeDestroyed+2,"destroyed observer fails open");
  Check(GammaRegister(device,testWindow),"window re-register");
  testForeground=nullptr;const unsigned beforeInactive=applications;Call(device,b);Call(device,b);Check(applications==beforeInactive+2,"inactive fails open");testForeground=testWindow;
  GammaEnabled=false;const unsigned beforeDisabled=applications;Call(device,b);Call(device,b);Check(applications==beforeDisabled+2,"disabled forwards");GammaEnabled=true;
  // Fill bounded cache; ninth device must always forward.
  for(unsigned id=1;id<8;++id)Check(GammaRegister(Device(id),testWindow),"bounded registration");
  Check(!GammaRegister(Device(8),testWindow),"capacity bounded");const unsigned beforeFull=applications;Call(Device(8),b);Call(Device(8),b);Check(applications==beforeFull+2,"capacity fail open");
  testWindow=reinterpret_cast<HWND>(0x200);testForeground=testWindow;++testWindowThread;hookFailure=true;
  Check(!GammaRegister(device,testWindow),"observer failure reported");const unsigned beforeFailure=applications;Call(device,b);Call(device,b);Check(applications==beforeFailure+2,"observer failure forwards");
  std::puts("PASS: identical/changed/flags/swap/null; actual focus events; reset invalidation; reentrant and both concurrent completion orders; reused address; destroyed/inactive window; disabled/capacity fail-open.");
  return 0;
 } catch(const std::exception& error) {std::printf("FAIL: %s\n",error.what());return 1;}
}
