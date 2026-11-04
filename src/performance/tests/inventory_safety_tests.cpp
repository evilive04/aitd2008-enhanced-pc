#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstring>
#include <cassert>
#include <limits>
static bool diagnostics=true,enabled=false;
static void Log(const char*,...) {}
static bool ConfigFlag(const wchar_t*,const wchar_t*,bool fallback,const wchar_t*){return fallback;}
#include "../inventory_safety.inl"
extern "C" __declspec(dllexport) void* Before(){return reinterpret_cast<void*>(&inventory_safety::BehaviorGate);}
extern "C" __declspec(dllexport) void* Hit(){return reinterpret_cast<void*>(&inventory_safety::HeroMessageGate);}
extern "C" __declspec(dllexport) void* AnimA(){return reinterpret_cast<void*>(&inventory_safety::AnimationGateA);}
extern "C" __declspec(dllexport) void* AnimB(){return reinterpret_cast<void*>(&inventory_safety::AnimationGateB);}
extern "C" __declspec(dllexport) void Configure(bool supported){ConfigureInventorySafety(L"",supported);}
extern "C" __declspec(dllexport) void Install(){InstallInventorySafety();enabled=true;}
#ifdef TEST_EXE
static BYTE* alloc(SIZE_T n){auto p=static_cast<BYTE*>(VirtualAlloc(nullptr,n,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));assert(p);return p;}
static void ptr(BYTE* p,void* v){*reinterpret_cast<void**>(p)=v;}
int main(){
 using namespace inventory_safety;
 assert(!InventorySafetyContextValue(0)&&!InventorySafetyContextValue(-1)&&InventorySafetyContextValue(.5f)&&InventorySafetyContextValue(2));
 assert(!InventorySafetyContextValue(std::numeric_limits<float>::infinity())&&!InventorySafetyContextValue(std::numeric_limits<float>::quiet_NaN()));
 assert(VirtualAlloc(reinterpret_cast<void*>(0xfd0000),0x10000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
 auto hero=alloc(0x3000),actor=alloc(0x3000),b=alloc(0x800),type=alloc(0x80),parent=alloc(0x80),follow=alloc(0x800),ft=alloc(0x80),entries=alloc(0x100),model=alloc(0x1000),message=alloc(0x100),fx=alloc(0x400);
 ptr(reinterpret_cast<BYTE*>(0xfd61ec),hero);*reinterpret_cast<DWORD*>(hero)=0xc834c0;ptr(b+0x18,type);ptr(type+0x10,parent);*reinterpret_cast<DWORD*>(parent+4)=0x4e504142;ptr(b+0x20,actor);ptr(actor+0x25c,b);ptr(actor+0x3b4,model);*reinterpret_cast<DWORD*>(actor+0xc)=77;
 Configure(true);assert(CombatBehavior(b,hero));assert(!SuspendBehavior(b));
 *reinterpret_cast<float*>(hero+0x1d88)=1;assert(SuspendBehavior(b));
 ptr(actor+0x94,entries);*reinterpret_cast<int*>(actor+0x90)=1;ptr(entries,follow);ptr(follow+0x20,actor);ptr(follow+0x18,ft);*reinterpret_cast<DWORD*>(ft+4)=0x4e504346;ptr(follow+0x240,hero);
 assert(FriendlyFollower(actor,hero)&&!SuspendBehavior(b));*reinterpret_cast<int*>(actor+0x90)=0;assert(SuspendBehavior(b));
 *reinterpret_cast<DWORD*>(message)=0xc651b0;assert(BlockHeroHit(hero,message));assert(!BlockHeroHit(actor,message));*reinterpret_cast<DWORD*>(message)=0;assert(!BlockHeroHit(hero,message));
 *reinterpret_cast<DWORD*>(fx+160)=0x3fc00000;FilterAnimation(model,fx);assert(!*reinterpret_cast<DWORD*>(fx+160));
 *reinterpret_cast<float*>(hero+0x1d88)=0;assert(!SuspendBehavior(b));*reinterpret_cast<DWORD*>(fx+160)=0x3fc00000;FilterAnimation(model,fx);assert(*reinterpret_cast<DWORD*>(fx+160)==0x3fc00000);
 *reinterpret_cast<float*>(hero+0x1e00)=1;assert(SuspendBehavior(b));*reinterpret_cast<DWORD*>(actor+0xc)=78;FilterAnimation(model,fx);assert(*reinterpret_cast<DWORD*>(fx+160)==0x3fc00000);
 assert(SuspendBehavior(b));FilterAnimation(model,fx);assert(!*reinterpret_cast<DWORD*>(fx+160));
 *reinterpret_cast<int*>(actor+0x90)=1;*reinterpret_cast<DWORD*>(fx+160)=0x3fc00000;FilterAnimation(model,fx);assert(*reinterpret_cast<DWORD*>(fx+160)==0x3fc00000);
 Configure(false);assert(!SuspendBehavior(b)&&!BlockHeroHit(hero,message));ptr(reinterpret_cast<BYTE*>(0xfd61ec),reinterpret_cast<void*>(1));assert(!InventorySafetyMenuActive());
 return 0;
}
#endif
