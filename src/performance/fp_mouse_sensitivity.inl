// Native mouse-only FP angular consumers. Include after combat_controls.inl.
namespace fp_mouse {
static bool enabled=false;
static float gainX=.0015f,gainY=.0015f;
static bool invertY=false;
static volatile LONG rawX=0,rawY=0,horizontalUpdates=0,verticalUpdates=0;
static unsigned char* sampleHero=nullptr;
static DWORD sampleIdA=0,sampleIdB=0,sampleTick=0;
static volatile LONG attempted=0;
// Bounded diagnostics: signed AND absolute counts distinguish cancellation,
// injected mouse counts and subsequent native integration. No behavior change.
static volatile LONG capturedX=0,capturedY=0,capturedAbsX=0,capturedAbsY=0;
static volatile LONG consumedX=0,consumedY=0,consumedAbsX=0,consumedAbsY=0;
static LONG lastRawX=0,lastRawY=0;
static float lastRateX=0,lastRateY=0,lastDtX=0,lastDtY=0;
static DWORD lastNonzeroTickX=0,lastNonzeroTickY=0;
static volatile LONG ropeYawUpdates=0,ropePitchUpdates=0;
static volatile LONG lighterYawUpdates=0,lighterPitchUpdates=0;
static __declspec(noinline) bool RopeContext(unsigned char* hero){
 using namespace modern_combat;
 unsigned char* owned=nullptr;
 if(!hero||!(Word(hero,0x968)&1))return false;
 auto* main=Pointer(hero,0x25c);auto* look=Pointer(hero,0xf74);auto* inv=Pointer(hero,0x810);auto* hands=Pointer(hero,0xf9c);
 // Actual FP rope snapshot5864: HRPB/state10 and HLTO/state10, mode1.
 // Factories6E2298/6AE0A4 establish these exact native classes. Other rope
 // transitions and modes retain their original input and physics.
 if(!main||Word(main,0)!=0xc8f108||Pointer(main,0x20)!=hero||Word(main,0xb0)!=10||
    !HandlerClass(main,0x48525042)||!look||Word(look,0)!=0xc8caf8||Pointer(look,0x20)!=hero||Word(look,0xb0)!=10||
    !inv||Pointer(inv,0x754)!=hero||Word(inv,0x81c)||!hands||Pointer(hands,0)!=hero||Pointer(hands,0x24))return false;
 const DWORD profile=Word(look,0x4a0);
 return profile<=1&&Word(look,0x3f0+profile*80)==1&&WorldInteractionContext(HeroController(),&owned)&&owned==hero;
}
static __declspec(noinline) bool MouseBindings(unsigned char* controller){
 using namespace modern_combat;
 const auto* table=*reinterpret_cast<unsigned char**>(0x1213570);
 const unsigned base=Word(controller,8);if(!table||base>4096)return false;
 for(unsigned i=0;i<4;++i){
  const auto* action=table+(base+47+i)*60;
  if(Word(action,0x14)!=1)return false;
  const auto* binding=Pointer(action,0x18);const auto* value=binding?Pointer(binding,0):nullptr;
  if(!value||Word(binding,4)!=0||Word(value,0)!=1||Word(value,4)!=(i<2?4u:0u)||
     // Low nibble is immutable axis/polarity. Native97610B/97633A sets
     // activity/edge/history bits10/20/40/80 while this axis is moving.
     (Word(value,8)&0x0f)!=(i%2?5u:1u))return false;
  const auto* device=Pointer(value,0x50);
  if(!device||Word(device,0)!=0xcd3d68)return false;
 }
 return true;
}
static __declspec(noinline) bool Context(unsigned char* hero){
 __try {
  using namespace modern_combat;
  unsigned char *owned=nullptr,*inventory=nullptr;
  if(!fp_mouse::enabled||!hero)return false;
  if(!NativeContext(HeroController(),&owned,&inventory)){
   if(!RopeContext(hero))return false;owned=hero;inventory=Pointer(hero,0x810);
  }
  if(owned!=hero||
     !(Word(hero,0x968)&1)||Word(inventory,0x81c)||!MouseBindings(Pointer(hero,0x95c)))return false;
  auto* hands=Pointer(hero,0xf9c);auto* look=Pointer(hero,0xf74);
  auto* pistol=hands?Pointer(hands,0x14):nullptr;
  if(!hands||Pointer(hands,0)!=hero||!look||Pointer(look,0x20)!=hero||Word(look,0xb0)!=10)return false;
  if(pistol)return (IsPistol(pistol)&&NativePistolBehavior(pistol))||
      (Word(look,0)==0xc8caf8&&(OwnedLighter(hero,pistol,1)||SprayLighterInHands(hero,pistol)));
  // Native6A9D80 explicitly selects the empty-hand subjective contexts+470/
  // +47C before the same6AB010 pitch consumer; ordinary HMOV66EA26 calls
  // the same66AC20 subjective turn. No separate unarmed gain is needed.
  // Authenticate the native look class (constructor6AE0A4) and exclude a
  // two-hand object rather than admitting every non-pistol weapon.
  return Word(look,0)==0xc8caf8&&!Pointer(hands,0x24);
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
// Native6AB010 mode1 consumes the same axis-angle call sites as rope look.
// Lighter use may select that mode through the native held-item profile; retain
// its original direction/cone construction and replace only mouse angular gain.
static bool LighterAngularContext(unsigned char* hero){
 using namespace modern_combat;
 auto* hands=Pointer(hero,0xf9c);auto* look=Pointer(hero,0xf74);
 if(!hands||!look||Word(look,0)!=0xc8caf8||Pointer(look,0x20)!=hero||Word(look,0xb0)!=10)return false;
 const DWORD profile=Word(look,0x4a0);
 return profile<=1&&Word(look,0x3f0+profile*80)==1&&
     (OwnedLighter(hero,Pointer(hands,4),0)||OwnedLighter(hero,Pointer(hands,0x14),1));
}
static void Capture(const void* data,DWORD size){
 if(!data||(size!=sizeof(DIMOUSESTATE)&&size!=sizeof(DIMOUSESTATE2)))return;
 __try {
  auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);
  if(!Context(hero)){InterlockedExchange(&rawX,0);InterlockedExchange(&rawY,0);sampleHero=nullptr;return;}
  const DWORD a=modern_combat::Word(hero,0x10),b=modern_combat::Word(hero,0x14);
  if(sampleHero!=hero||sampleIdA!=a||sampleIdB!=b){InterlockedExchange(&rawX,0);InterlockedExchange(&rawY,0);}
  const auto* mouse=static_cast<const DIMOUSESTATE*>(data);
  const LONG x=std::clamp(mouse->lX,-4096L,4096L),y=std::clamp(mouse->lY,-4096L,4096L);
  InterlockedExchangeAdd(&rawX,x);InterlockedExchangeAdd(&rawY,y);
  InterlockedExchangeAdd(&capturedX,x);InterlockedExchangeAdd(&capturedY,y);
  InterlockedExchangeAdd(&capturedAbsX,x<0?-x:x);InterlockedExchangeAdd(&capturedAbsY,y<0?-y:y);
  sampleHero=hero;sampleIdA=a;sampleIdB=b;sampleTick=GetTickCount();
 }__except(EXCEPTION_EXECUTE_HANDLER){InterlockedExchange(&rawX,0);InterlockedExchange(&rawY,0);sampleHero=nullptr;}
}
static __declspec(noinline) bool Ready(unsigned char* hero){
 return Context(hero)&&sampleHero==hero&&modern_combat::Word(hero,0x10)==sampleIdA&&
     modern_combat::Word(hero,0x14)==sampleIdB&&GetTickCount()-sampleTick<=100;
}
static __declspec(noinline) bool Rate(LONG counts,float gain,float dt,float* out){
 if(!out||!std::isfinite(gain)||gain<=0||!std::isfinite(dt)||dt<=0||dt>.1f)return false;
 *out=static_cast<float>(std::clamp(counts,-2048L,2048L))*gain/dt;
 return std::isfinite(*out);
}
static __declspec(noinline) void Horizontal(unsigned char* hero,DWORD subjective,float* speed,float* direction){
 __try {
  if(!subjective||!speed||!direction||!Ready(hero)||RopeContext(hero))return;
  float rate=0;const float dt=*reinterpret_cast<float*>(0xf503c8);
  if(!Rate(0,gainX,dt,&rate))return;
  const float x=*reinterpret_cast<float*>(hero+0x210),z=*reinterpret_cast<float*>(hero+0x218);
  const float n=x*x+z*z;if(!std::isfinite(n)||n<.5f||n>1.5f)return;
  const LONG counts=std::clamp(InterlockedExchange(&rawX,0),-2048L,2048L);
  if(!Rate(counts,gainX,dt,&rate))return;
  InterlockedExchangeAdd(&consumedX,counts);InterlockedExchangeAdd(&consumedAbsX,counts<0?-counts:counts);
  if(counts){lastRawX=counts;lastRateX=rate;lastDtX=dt;lastNonzeroTickX=GetTickCount();}
  *speed=std::fabs(rate);
  // r29 matrix forward is +210; +mouseX means decreasing atan2(X,Z),
  // matching the orbit camera. Use a 90deg native turn target so its ordinary
  // one-degree completion tolerance cannot erase sub-degree mouse input.
  if(rate!=0){const float sign=rate>0?-1.f:1.f,inv=1.f/std::sqrt(n);
   direction[0]=sign*z*inv;direction[1]=0;direction[2]=-sign*x*inv;direction[3]=0;}
  InterlockedIncrement(&horizontalUpdates);
 }__except(EXCEPTION_EXECUTE_HANDLER){}
}
static __declspec(noinline) void Vertical(unsigned char* look,unsigned char* frame,unsigned char* fx){
 __try {
  using namespace modern_combat;
  auto* hero=look?Pointer(look,0x20):nullptr;
  if(!frame||!fx||!Ready(hero)||Pointer(hero,0xf74)!=look||RopeContext(hero))return;
  float rate=0;const float dt=*reinterpret_cast<float*>(0xf503c8);
  if(!Rate(0,gainY,dt,&rate))return;
  const LONG counts=std::clamp(InterlockedExchange(&rawY,0),-2048L,2048L);
  if(!Rate(counts,gainY,dt,&rate))return;
  // Native6AB91F applies -look+220 to its X-rotation; its forward.Y is
  // +sin(look+220). Thus mouse up must increment +220. Keep native/game invert.
  const bool nativeInvert=(*reinterpret_cast<DWORD*>(0xfda5c8)&1)||*reinterpret_cast<BYTE*>(0xfaf22e);
  rate*=((nativeInvert!=invertY)?1.f:-1.f);
  InterlockedExchangeAdd(&consumedY,counts);InterlockedExchangeAdd(&consumedAbsY,counts<0?-counts:counts);
  if(counts){lastRawY=counts;lastRateY=rate;lastDtY=dt;lastNonzeroTickY=GetTickCount();}
  *reinterpret_cast<float*>(frame+0x1c)=rate; // Native sign test at6AB53E.
  *reinterpret_cast<float*>(fx+160+2*16)=std::fabs(rate); // Native pitch angular rate XMM2.
  InterlockedIncrement(&verticalUpdates);
 }__except(EXCEPTION_EXECUTE_HANDLER){}
}
static __declspec(noinline) void RopeAngular(unsigned char* look,DWORD axis,float* angle){
 __try {
  using namespace modern_combat;auto* hero=look?Pointer(look,0x20):nullptr;
  if(axis>1||!angle||!Ready(hero)||Pointer(hero,0xf74)!=look)return;
  const bool rope=RopeContext(hero);if(!rope&&!LighterAngularContext(hero))return;
  const float dt=*reinterpret_cast<float*>(0xf503c8),gain=axis?gainY:gainX;float rate{};
  if(!Rate(0,gain,dt,&rate))return;
  const LONG counts=std::clamp(InterlockedExchange(axis?&rawY:&rawX,0),-2048L,2048L);
  if(!Rate(counts,gain,dt,&rate))return;
  // Mode1 constructs yaw/pitch axis-angle matrices directly at6AB13C/171.
  // Override only their per-frame angle, retaining the original camera axes,
  // complete look-direction update and native cone limit6AB202..6AB2FD.
  const bool nativeInvert=(*reinterpret_cast<DWORD*>(0xfda5c8)&1)||*reinterpret_cast<BYTE*>(0xfaf22e);
  *angle=rate*dt*(axis?((nativeInvert!=invertY)?-1.f:1.f):-1.f);
  if(axis){InterlockedExchangeAdd(&consumedY,counts);InterlockedExchangeAdd(&consumedAbsY,counts<0?-counts:counts);
   if(counts){lastRawY=counts;lastRateY=rate;lastDtY=dt;lastNonzeroTickY=GetTickCount();}
   InterlockedIncrement(&verticalUpdates);InterlockedIncrement(rope?&ropePitchUpdates:&lighterPitchUpdates);
  }else{InterlockedExchangeAdd(&consumedX,counts);InterlockedExchangeAdd(&consumedAbsX,counts<0?-counts:counts);
   if(counts){lastRawX=counts;lastRateX=rate;lastDtX=dt;lastNonzeroTickX=GetTickCount();}
   InterlockedIncrement(&horizontalUpdates);InterlockedIncrement(rope?&ropeYawUpdates:&lighterYawUpdates);
  }
 }__except(EXCEPTION_EXECUTE_HANDLER){}
}
static void* nativeTurn=reinterpret_cast<void*>(0x571ef0);
static void* nativeAxisAngle=reinterpret_cast<void*>(0xb5ee00);
static DWORD pitchNext=0x6ab517;
static __declspec(naked) void HorizontalAdapter(){
 __asm {
  push ebp
  mov ebp,esp
  pushfd
  pushad
  sub esp,528
  and esp,0fffffff0h
  fxsave [esp]
  mov eax,[ebp]
  push edi
  lea ecx,[ebp+8]
  push ecx
  push [eax+0ch]
  push [eax+8]
  call Horizontal
  add esp,16
  fxrstor [esp]
  lea esp,[ebp-36]
  popad
  popfd
  mov esp,ebp
  pop ebp
  jmp dword ptr [nativeTurn]
 }
}
static __declspec(naked) void VerticalAdapter(){
 __asm {
  push ebp
  mov ebp,esp
  pushfd
  pushad
  sub esp,528
  and esp,0fffffff0h
  fxsave [esp]
  push esp
  lea eax,[ebp+4]
  push eax
  push ebx
  call Vertical
  add esp,12
  fxrstor [esp]
  lea esp,[ebp-36]
  popad
  popfd
  mov esp,ebp
  pop ebp
  mov eax,[ebx+4a0h]
  jmp dword ptr [pitchNext]
 }
}
static __declspec(naked) void RopeYawAdapter(){
 __asm {
  push ebp
  mov ebp,esp
  pushfd
  pushad
  sub esp,528
  and esp,0fffffff0h
  fxsave [esp]
  lea eax,[ebp+8]
  push eax
  push 0
  push ebx
  call RopeAngular
  add esp,12
  fxrstor [esp]
  lea esp,[ebp-36]
  popad
  popfd
  mov esp,ebp
  pop ebp
  jmp dword ptr [nativeAxisAngle]
 }
}
static __declspec(naked) void RopePitchAdapter(){
 __asm {
  push ebp
  mov ebp,esp
  pushfd
  pushad
  sub esp,528
  and esp,0fffffff0h
  fxsave [esp]
  lea eax,[ebp+8]
  push eax
  push 1
  push ebx
  call RopeAngular
  add esp,12
  fxrstor [esp]
  lea esp,[ebp-36]
  popad
  popfd
  mov esp,ebp
  pop ebp
  jmp dword ptr [nativeAxisAngle]
 }
}
static void Configure(const wchar_t* config,bool supported){
 enabled=supported&&ConfigFlag(L"Camera",L"ConsistentFirstPersonMouse",true,config);
 invertY=ConfigFlag(L"Camera",L"InvertY",false,config);
 wchar_t text[64]{};wchar_t* end=nullptr;
 GetPrivateProfileStringW(L"Camera",L"SensitivityX",L"1.0",text,64,config);const double x=wcstod(text,&end);
 if(end==text||*end||!std::isfinite(x)||x<.1||x>4){enabled=false;return;}
 GetPrivateProfileStringW(L"Camera",L"SensitivityY",L"1.0",text,64,config);const double y=wcstod(text,&end);
 if(end==text||*end||!std::isfinite(y)||y<.1||y>4){enabled=false;return;}
 gainX=.0015f*static_cast<float>(x);gainY=.0015f*static_cast<float>(y);
}
static void Install(){
 if(!enabled||InterlockedCompareExchange(&attempted,1,0))return;
 struct Patch{DWORD address;unsigned size;BYTE signature[6];void* replacement;BYTE opcode;DWORD protection;};
 Patch patches[]={
  {0x66aff1,5,{0xe8,0xfa,0x6e,0xf0,0xff},reinterpret_cast<void*>(&HorizontalAdapter),0xe8,0},
  {0x6ab511,6,{0x8b,0x83,0xa0,0x04,0,0},reinterpret_cast<void*>(&VerticalAdapter),0xe9,0},
  {0x6ab13c,5,{0xe8,0xbf,0x3c,0x4b,0},reinterpret_cast<void*>(&RopeYawAdapter),0xe8,0},
  {0x6ab171,5,{0xe8,0x8a,0x3c,0x4b,0},reinterpret_cast<void*>(&RopePitchAdapter),0xe8,0}};
 for(const auto& p:patches)if(memcmp(reinterpret_cast<void*>(p.address),p.signature,p.size)){enabled=false;Log("FP mouse angular signature mismatch");return;}
 unsigned writable=0;for(auto& p:patches){if(!VirtualProtect(reinterpret_cast<void*>(p.address),p.size,PAGE_EXECUTE_READWRITE,&p.protection))break;++writable;}
 if(writable!=_countof(patches)){for(int n=static_cast<int>(writable)-1;n>=0;--n){DWORD ignored;VirtualProtect(reinterpret_cast<void*>(patches[n].address),patches[n].size,patches[n].protection,&ignored);}enabled=false;return;}
 for(auto& p:patches){auto* at=reinterpret_cast<BYTE*>(p.address);at[0]=p.opcode;*reinterpret_cast<DWORD*>(at+1)=static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(p.replacement)-p.address-5);memset(at+5,0x90,p.size-5);FlushInstructionCache(GetCurrentProcess(),at,p.size);}
 for(auto& p:patches){DWORD ignored;VirtualProtect(reinterpret_cast<void*>(p.address),p.size,p.protection,&ignored);}
 Log("Native FP mouse angular routing installed; rad/count X=%.6f Y=%.6f",gainX,gainY);
}
}
