// Native stable-vehicle camera only. Ordinary on-foot orbit ownership is separate.
#include "ui_native_status.h"
namespace vehicle_camera {
struct Owners {unsigned char *hero{},*vehicle{},*camera{},*component{},*saved{};DWORD savedId{},heroId{},vehicleId{},cameraId{};};
struct Pose {Owners owner{};unsigned char* frame{};float yaw{},pitch{},radius{},distance{},pivot[4]{};DWORD tick{};bool active{};};
static Pose pose{};
static volatile LONG firstPersonWeightReports=0,firstPersonRestartReports=0,thirdPersonReports=0,installAttempt=0;
static bool CachedPair(const Owners& previous,unsigned char* hero,unsigned char* vehicle,unsigned char* camera,unsigned char* component,unsigned char** saved,DWORD* identity){
 // This token is optional. An expired/unmapped cached actor cannot invalidate
 // the current genuine selected follower or its native saved weakref.
 __try {
  if(previous.hero!=hero||previous.vehicle!=vehicle||previous.camera!=camera||previous.component!=component||
   !previous.heroId||!previous.vehicleId||!previous.cameraId||previous.heroId!=*reinterpret_cast<DWORD*>(hero+0xc)||
   previous.vehicleId!=*reinterpret_cast<DWORD*>(vehicle+0xc)||previous.cameraId!=*reinterpret_cast<DWORD*>(camera+0xc)||
   !previous.saved||previous.saved==camera||!previous.savedId||*reinterpret_cast<DWORD*>(previous.saved)!=0xc664b0||
   *reinterpret_cast<DWORD*>(previous.saved+0xc)!=previous.savedId)return false;
  const auto* definition=*reinterpret_cast<unsigned char**>(previous.saved+0x28);
  const auto* settings=*reinterpret_cast<unsigned char**>(previous.saved+0x38c);
  const auto* timeline=*reinterpret_cast<unsigned char**>(previous.saved+0x280);
  if(!definition||(*reinterpret_cast<const DWORD*>(definition+0x20)&8)||!settings||
   *reinterpret_cast<const DWORD*>(settings+0xc)!=8||*reinterpret_cast<unsigned char* const*>(settings+0x40)!=hero||
   (timeline&&timeline[0x204]))return false;
  *saved=previous.saved;*identity=previous.savedId;return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool Context(bool firstPerson,Owners* output,DWORD* failure=nullptr){
 __try {
  if(failure)*failure=1;
  if(!freeThirdPerson||!orbitSettingsValid||!output||*reinterpret_cast<DWORD*>(0xff4290)||
   ui_native_status::BlockingLoad(*reinterpret_cast<DWORD*>(0xff4390))||*reinterpret_cast<BYTE*>(0xff437c)||*reinterpret_cast<DWORD*>(0xff5508)!=35||
   (*reinterpret_cast<DWORD*>(0xfd8bd4)&15)||(*reinterpret_cast<DWORD*>(0xfda5c8)&0x38)!=8||
   *reinterpret_cast<BYTE*>(0xff4b7f))return false;
  auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);
  if(failure)*failure=2;
  if(!hero||*reinterpret_cast<DWORD*>(hero)!=0xc834c0||hero[0xc19]||
   (((*reinterpret_cast<DWORD*>(hero+0x968)&1)!=0)!=firstPerson)||OrbitNativeMenu(hero))return false;
  const auto* cinematic=*reinterpret_cast<unsigned char**>(hero+0x280);
  const auto* controller=*reinterpret_cast<unsigned char**>(hero+0x95c);
  auto* handler=*reinterpret_cast<unsigned char**>(hero+0x25c);
  if(failure)*failure=3;
  if((cinematic&&cinematic[0x204])||!controller||(*reinterpret_cast<const DWORD*>(controller+4)&6)!=6||
   !handler||*reinterpret_cast<DWORD*>(handler)!=0xc84b68||*reinterpret_cast<unsigned char**>(handler+0x20)!=hero)return false;
  const DWORD state=*reinterpret_cast<DWORD*>(handler+0xb0);
  auto* vehicle=*reinterpret_cast<unsigned char**>(handler+0x8e8);
  if(failure)*failure=4;
  if((state!=5&&state!=15)||!vehicle||*reinterpret_cast<DWORD*>(vehicle)!=0xc9adf8||
   *reinterpret_cast<unsigned char**>(hero+0x28c)!=vehicle)return false;
  DWORD pid{};const HWND window=GetForegroundWindow();GetWindowThreadProcessId(window,&pid);
  if(failure)*failure=5;
  if(pid!=GetCurrentProcessId()||IsIconic(window))return false;
  output->hero=hero;output->vehicle=vehicle;output->heroId=*reinterpret_cast<DWORD*>(hero+0xc);output->vehicleId=*reinterpret_cast<DWORD*>(vehicle+0xc);
  if(firstPerson){
   if(failure)*failure=6;
   const auto* subjective=*reinterpret_cast<unsigned char**>(0xfd5d90);
   const auto* top=*reinterpret_cast<unsigned char***>(0xfda8f0);
   const bool valid=subjective&&*reinterpret_cast<const DWORD*>(subjective)==0xc67048&&top&&*top==subjective&&
    (reinterpret_cast<const unsigned char*>(top)[0xc]&1);
   if(valid&&failure)*failure=0;return valid;
  }
  auto* camera=*reinterpret_cast<unsigned char**>(0xfd5d98);
  auto* component=camera?*reinterpret_cast<unsigned char**>(camera+0x25c):nullptr;
  auto* saved=*reinterpret_cast<unsigned char**>(hero+0xf10);
  DWORD id=*reinterpret_cast<DWORD*>(hero+0xf14);
  if(failure)*failure=6;
  if(!camera||*reinterpret_cast<DWORD*>(camera)!=0xc67670||
   *reinterpret_cast<unsigned char**>(camera+0x3e8)!=vehicle||!component||
   *reinterpret_cast<DWORD*>(component)!=0xc68228||*reinterpret_cast<unsigned char**>(component+0x20)!=camera||
   (*reinterpret_cast<DWORD*>(component+0xb0)>2)||
   (*reinterpret_cast<DWORD*>(component+0x438)&1))return false;
  // Actual v169 driving log reaches Prepare in CGFO mode2 on this same
  // authenticated vehicle follower. DescriptorF516BC tableF516F0 has native
  // mode0/1/2 flags1/2/4: each uses the normal pose/collision path, without
  //468EC0 focus bit10. Higher modes and forced-camera flags stay native.
  // This is vehicle-only; on-foot mode2/equipment policy is unchanged.
  const auto* top=*reinterpret_cast<unsigned char***>(0xfda8f0);
  if(failure)*failure=7;
  //650890 saves the CURRENT TP camera before entering FP. Once our genuine
  // follower is selected, that valid native weakref therefore names C67670,
  // not the original KIC. Never replace or write that native saved reference.
  const bool savedFollower=saved==camera&&id&&*reinterpret_cast<DWORD*>(camera+0xc)==id;
  const bool initialFollower=!saved&&!id&&top&&*top==camera;
  if(savedFollower||initialFollower){
   if(!CachedPair(pose.owner,hero,vehicle,camera,component,&saved,&id)){saved=camera;id=*reinterpret_cast<DWORD*>(camera+0xc);}
  }
  if(!saved||!id||*reinterpret_cast<DWORD*>(saved+0xc)!=id)return false;
  if(saved!=camera){
   if(*reinterpret_cast<DWORD*>(saved)!=0xc664b0)return false;
   const auto* definition=*reinterpret_cast<unsigned char**>(saved+0x28);
   const auto* settings=*reinterpret_cast<unsigned char**>(saved+0x38c);
   const auto* timeline=*reinterpret_cast<unsigned char**>(saved+0x280);
   if(failure)*failure=8;
   // Cached or native saved KIC must still be interactive and owned by this hero.
   if(!definition||(*reinterpret_cast<const DWORD*>(definition+0x20)&8)||!settings||
    *reinterpret_cast<const DWORD*>(settings+0xc)!=8||*reinterpret_cast<unsigned char* const*>(settings+0x40)!=hero||
    (timeline&&timeline[0x204]))return false;
  }
  if(failure)*failure=9;
  if(!top||(*top!=saved&&*top!=camera))return false;
  output->camera=camera;output->cameraId=*reinterpret_cast<DWORD*>(camera+0xc);output->component=component;output->saved=saved;output->savedId=id;if(failure)*failure=0;return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){if(failure)*failure=99;return false;}
}
static void ReportThirdPersonRejection(DWORD reason){
 __try {
  auto* hero=*reinterpret_cast<unsigned char**>(0xfd61ec);
  auto* handler=hero?*reinterpret_cast<unsigned char**>(hero+0x25c):nullptr;
  if(!hero||!handler||*reinterpret_cast<DWORD*>(handler)!=0xc84b68||
   (*reinterpret_cast<DWORD*>(hero+0x968)&1))return;
  const DWORD state=*reinterpret_cast<DWORD*>(handler+0xb0);if(state!=5&&state!=15)return;
  static volatile LONG reports[13]{};const DWORD index=reason<13?reason:12;
  if(InterlockedIncrement(&reports[index])>3)return;
  auto* camera=*reinterpret_cast<unsigned char**>(0xfd5d98);
  auto* component=camera?*reinterpret_cast<unsigned char**>(camera+0x25c):nullptr;
  auto* saved=*reinterpret_cast<unsigned char**>(hero+0xf10);
  const auto* top=*reinterpret_cast<unsigned char***>(0xfda8f0);
  Log("Vehicle TP rejected stage=%lu state=%lu component=%p mode=%lu target=%p vehicle=%p saved=%p/%08lx actualSavedId=%08lx top=%p menu=%lu load=%lu context=%08lx (1=world,2=hero/view/menu,3=input/script,4=vehicle,5=foreground,6=camera/mode,7=savedweak,8=savedowner/script,9=top,10=hookframe,11=radius,12=data)",
   reason,state,component,component?*reinterpret_cast<DWORD*>(component+0xb0):~0u,
   camera?*reinterpret_cast<unsigned char**>(camera+0x3e8):nullptr,*reinterpret_cast<unsigned char**>(handler+0x8e8),
   saved,*reinterpret_cast<DWORD*>(hero+0xf14),saved?*reinterpret_cast<DWORD*>(saved+0xc):0,
   top?*top:nullptr,*reinterpret_cast<DWORD*>(0xff4290),*reinterpret_cast<DWORD*>(0xff4390),*reinterpret_cast<DWORD*>(0xfd8bd4));
 }__except(EXCEPTION_EXECUTE_HANDLER){}
}
static unsigned char* Select(unsigned char* start,unsigned char* original){
 Owners owner{};if(!Context(false,&owner)||!original)return nullptr;
 __try {
  auto* winner=*reinterpret_cast<unsigned char**>(original);
  if(winner!=owner.saved&&winner!=owner.camera)return nullptr;
  unsigned char* follower=nullptr;unsigned count=0;
  for(auto* node=start;node;node=*reinterpret_cast<unsigned char**>(node+0x18)){
   if(++count>32)return nullptr;
   if(!(node[0xc]&1))continue;
   auto* actor=*reinterpret_cast<unsigned char**>(node);
   if(actor==owner.camera)follower=node;
   else if(actor!=owner.saved)return nullptr; // Every other active camera wins natively.
  }
  return follower; // Already active node only; never activate or edit a native list.
 }__except(EXCEPTION_EXECUTE_HANDLER){return nullptr;}
}
static bool Prepare(unsigned char* component,unsigned char* frame,unsigned char* fx){
 Owners owner{};DWORD failure{};
 if(!Context(false,&owner,&failure)){ReportThirdPersonRejection(failure);pose.active=false;return false;}
 if(!frame||!fx||component!=owner.component||orbitPivotComponent!=component||orbitPivotFrame!=frame){ReportThirdPersonRejection(10);pose.active=false;return false;}
 __try {
  const DWORD now=GetTickCount();const float radius=OrbitRead(component,0x43c);
  if(!std::isfinite(radius)||radius<1||radius>30){ReportThirdPersonRejection(11);pose.active=false;return false;}
  const bool fresh=!pose.active||pose.owner.hero!=owner.hero||pose.owner.vehicle!=owner.vehicle||
   pose.owner.component!=component||pose.owner.saved!=owner.saved||pose.owner.savedId!=owner.savedId||
   pose.owner.heroId!=owner.heroId||pose.owner.vehicleId!=owner.vehicleId||pose.owner.cameraId!=owner.cameraId||now-pose.tick>1500;
  if(fresh){
   const float x=OrbitRead(frame,0xe0),y=OrbitRead(frame,0xe4),z=OrbitRead(frame,0xe8);
   if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)){pose.active=false;return false;}
   pose.yaw=atan2f(x,z);pose.pitch=asinf(std::clamp(y,-1.f,1.f));pose.distance=radius;OrbitDiscardMouse();
  }
  const LONG dx=InterlockedExchange(&orbitRawX,0),dy=InterlockedExchange(&orbitRawY,0);
  if(now-orbitLastMouse<100){pose.yaw=OrbitWrap(pose.yaw-static_cast<float>(std::clamp(dx,-2048L,2048L))*orbitGainX);
   pose.pitch=std::clamp(pose.pitch-static_cast<float>(std::clamp(dy,-2048L,2048L))*orbitGainY*(orbitInvertY?-1.f:1.f),-1.22173048f,1.13446401f);}
  for(unsigned i=0;i<4;++i){if(!std::isfinite(orbitNativePivot[i])){pose.active=false;return false;}pose.pivot[i]=orbitNativePivot[i];}
  pose.owner=owner;pose.frame=frame;pose.radius=radius;pose.tick=now;pose.active=true;
  // Same C68228 native pose path and collision consumer; separate vehicle state
  // retains its measured5.9m profile radius and target vehicle exclusions.
  *reinterpret_cast<float*>(frame+0x5c)=0;*reinterpret_cast<float*>(frame+0x10)=0;
  *reinterpret_cast<float*>(fx+160+4*16)=0;
  if(fresh&&InterlockedIncrement(&thirdPersonReports)<=3)Log("Vehicle orbit begin hero=%p vehicle=%p component=%p saved=%p/%08lx radius=%.4f; native active-camera ranking/collision retained",owner.hero,owner.vehicle,component,owner.saved,owner.savedId,radius);
  return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){pose.active=false;return false;}
}
static bool Finish(unsigned char* component,unsigned char* frame){
 if(!pose.active||pose.owner.component!=component||pose.frame!=frame)return false;
 Owners owner{};
 if(!Context(false,&owner)||owner.hero!=pose.owner.hero||owner.vehicle!=pose.owner.vehicle||
  owner.saved!=pose.owner.saved||owner.savedId!=pose.owner.savedId||owner.heroId!=pose.owner.heroId||
  owner.vehicleId!=pose.owner.vehicleId||owner.cameraId!=pose.owner.cameraId){pose.active=false;return false;}
 __try {
  const float cp=cosf(pose.pitch),sp=sinf(pose.pitch),sy=sinf(pose.yaw),cy=cosf(pose.yaw);
  const float forward[4]={sy*cp,sp,cy*cp,0},right[3]={-cy,0,sy},up[4]={-sy*sp,cp,-cy*sp,0};
  float eye[4]={pose.pivot[0]-forward[0]*pose.radius,pose.pivot[1]-forward[1]*pose.radius,pose.pivot[2]-forward[2]*pose.radius,1};
  float safe=pose.radius;
  const float corners[5][2]={{0,0},{-.18f,-.12f},{.18f,-.12f},{-.18f,.12f},{.18f,.12f}};
  for(const auto& corner:corners){
   float direction[4]{};float squared=0;
   for(unsigned i=0;i<3;++i){direction[i]=eye[i]+corner[0]*right[i]+corner[1]*up[i]-pose.pivot[i];squared+=direction[i]*direction[i];}
   const float length=sqrtf(squared);if(!std::isfinite(length)||length<.001f){pose.active=false;return false;}
   for(unsigned i=0;i<3;++i)direction[i]/=length;
   const float query=length+1.f,result=OrbitRay(component,pose.pivot,direction,query);
   if(!std::isfinite(result)){pose.active=false;return false;}
   if(result<query-.0001f)safe=std::min(safe,std::max(.05f,std::clamp(result/query,0.f,1.f)*pose.radius-.20f));
  }
  float dt=*reinterpret_cast<float*>(0xf503c8);if(!std::isfinite(dt)||dt<0||dt>.1f)dt=0;
  pose.distance=safe<pose.distance?safe:std::min(safe,pose.distance+(safe-pose.distance)*(1.f-expf(-7.f*dt)));
  for(unsigned i=0;i<3;++i)eye[i]=pose.pivot[i]-forward[i]*pose.distance;
  const float worldUp[4]={0,1,0,0};OrbitLook(reinterpret_cast<float*>(frame+0xc0),pose.pivot,eye,worldUp);return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){pose.active=false;return false;}
}
static bool FirstPersonOwned(unsigned char* look,Owners* output){
 __try {
  Owners owner{};if(!look||!Context(true,&owner)||*reinterpret_cast<DWORD*>(look)!=0xc8caf8||
   *reinterpret_cast<unsigned char**>(owner.hero+0xf74)!=look||*reinterpret_cast<unsigned char**>(look+0x20)!=owner.hero||
   *reinterpret_cast<DWORD*>(look+0xb0)!=10||*reinterpret_cast<DWORD*>(look+0x4a0)!=0||
   *reinterpret_cast<DWORD*>(look+0x3f0)!=0)return false;
  if(output)*output=owner;return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool __cdecl KeepFirstPersonWeight(unsigned char* look,float factor){
 __try {
  Owners owner{};if(!std::isfinite(factor)||factor<0||factor>=1||!FirstPersonOwned(look,&owner))return false;
  if(InterlockedIncrement(&firstPersonWeightReports)<=3)Log("Vehicle FP look weight retained hero=%p vehicle=%p look=%p nativeFactor=%.7f angles=(%.6f,%.6f) timer=(%lu,%.6f,%.6f); only look2A0 consumer scalar=1, native angles/timer/free-look predicate unchanged",owner.hero,owner.vehicle,look,factor,OrbitRead(look,0x220),OrbitRead(look,0x224),*reinterpret_cast<DWORD*>(look+0x2a0),OrbitRead(look,0x2a4),OrbitRead(look,0x2a8));
  return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
// Native restart event6AD6C0 attenuates stored angles by2A0 BEFORE setting
// kind1. Since this car view stayed visible during idle, use the same weight1
// at those two logical consumers. ESI (not EBX) is the native HLTO here.
static bool __cdecl KeepFirstPersonRestartWeight(unsigned char* look,float factor){
 __try {
  Owners owner{};if(!std::isfinite(factor)||factor<0||factor>=1||!FirstPersonOwned(look,&owner))return false;
  if(InterlockedIncrement(&firstPersonRestartReports)<=3)Log("Vehicle FP restart logical weight retained hero=%p vehicle=%p look=%p nativeFactor=%.7f angles=(%.6f,%.6f) timer=(%lu,%.6f,%.6f); native restart event/timer/integration retained",owner.hero,owner.vehicle,look,factor,OrbitRead(look,0x220),OrbitRead(look,0x224),*reinterpret_cast<DWORD*>(look+0x2a0),OrbitRead(look,0x2a4),OrbitRead(look,0x2a8));
  return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static void* nativeRestartPitchContinue=reinterpret_cast<void*>(0x6ad749);
static void* nativeRestartYawContinue=reinterpret_cast<void*>(0x6ad79d);
static __declspec(naked) void FirstPersonRestartPitchAdapter(){__asm {
 push ebp
 mov ebp,esp
 pushfd
 pushad
 sub esp,528
 and esp,0fffffff0h
 fxsave [esp]
 push dword ptr [esp+160]
 push esi
 call KeepFirstPersonRestartWeight
 add esp,8
 test al,al
 jz vehicleKeepNativeRestartPitch
 mov dword ptr [esp+160],3f800000h
vehicleKeepNativeRestartPitch:
 fxrstor [esp]
 lea esp,[ebp-36]
 popad
 popfd
 mov esp,ebp
 pop ebp
 mulss xmm3,xmm0
 movss dword ptr [esi+220h],xmm3
 jmp dword ptr [nativeRestartPitchContinue]
}}
static __declspec(naked) void FirstPersonRestartYawAdapter(){__asm {
 push ebp
 mov ebp,esp
 pushfd
 pushad
 sub esp,528
 and esp,0fffffff0h
 fxsave [esp]
 push dword ptr [esp+160]
 push esi
 call KeepFirstPersonRestartWeight
 add esp,8
 test al,al
 jz vehicleKeepNativeRestartYaw
 mov dword ptr [esp+160],3f800000h
vehicleKeepNativeRestartYaw:
 fxrstor [esp]
 lea esp,[ebp-36]
 popad
 popfd
 mov esp,ebp
 pop ebp
 mulss xmm2,xmm0
 movss dword ptr [esi+224h],xmm2
 jmp dword ptr [nativeRestartYawContinue]
}}
static void* nativeInterpolator=reinterpret_cast<void*>(0x63af50);
//6AB8EC/8F4/926/92E read look+2A0, then multiply pitch/yaw by factor squared
// before producing the displayed native rotation. Actual idle capture proved
// factor decays1->0 while stored yaw remains1.22173. Keep that consumer weight,
// not the unrelated3A8 timer or6ABF50 predicate. Original call remains once.
static __declspec(naked) void FirstPersonWeightAdapter(){__asm {
 call dword ptr [nativeInterpolator]
 push ebp
 mov ebp,esp
 pushfd
 pushad
 sub esp,528
 and esp,0fffffff0h
 fxsave [esp]
 mov eax,[esp+160]
 push eax
 push ebx
 call KeepFirstPersonWeight
 add esp,8
 test al,al
 jz vehicleKeepNativeWeight
 mov dword ptr [esp+160],3f800000h // XMM0.low32=1; upper96 and every other lane unchanged.
vehicleKeepNativeWeight:
 fxrstor [esp]
 lea esp,[ebp-36]
 popad
 popfd
 mov esp,ebp
 pop ebp
 ret
}}
static void Install(){
 if(!freeThirdPerson||!orbitSettingsValid||InterlockedCompareExchange(&installAttempt,1,0))return;
 const DWORD sites[6]={0x6ab8ec,0x6ab8f4,0x6ab926,0x6ab92e,0x6ad73d,0x6ad791};
 const unsigned lengths[6]={5,5,5,5,12,12};DWORD protections[6]{};
 const unsigned char pitch[12]={0xf3,0x0f,0x59,0xd8,0xf3,0x0f,0x11,0x9e,0x20,0x02,0,0};
 const unsigned char yaw[12]={0xf3,0x0f,0x59,0xd0,0xf3,0x0f,0x11,0x96,0x24,0x02,0,0};
 for(unsigned i=0;i<6;++i){const DWORD site=sites[i];const auto* bytes=reinterpret_cast<const unsigned char*>(site);
  const bool valid=i<4?(bytes[0]==0xe8&&site+5+*reinterpret_cast<const LONG*>(bytes+1)==0x63af50):memcmp(bytes,i==4?pitch:yaw,12)==0;
  if(!valid){Log("Vehicle FP camera signature mismatch at%08lx; native callbacks retained",site);return;}}
 unsigned writable=0;for(unsigned i=0;i<6;++i){if(!VirtualProtect(reinterpret_cast<void*>(sites[i]),lengths[i],PAGE_EXECUTE_READWRITE,&protections[i]))break;++writable;}
 if(writable!=6){for(int i=static_cast<int>(writable)-1;i>=0;--i){DWORD ignored;VirtualProtect(reinterpret_cast<void*>(sites[i]),lengths[i],protections[i],&ignored);}return;}
 for(unsigned i=0;i<6;++i){const DWORD site=sites[i];const auto adapter=i<4?&FirstPersonWeightAdapter:(i==4?&FirstPersonRestartPitchAdapter:&FirstPersonRestartYawAdapter);
  *reinterpret_cast<BYTE*>(site)=i<4?0xe8:0xe9;
  *reinterpret_cast<DWORD*>(site+1)=static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(adapter)-site-5);
  for(unsigned j=5;j<lengths[i];++j)*reinterpret_cast<BYTE*>(site+j)=0x90;
  FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(site),lengths[i]);}
 for(int i=5;i>=0;--i){DWORD ignored;VirtualProtect(reinterpret_cast<void*>(sites[i]),lengths[i],protections[i],&ignored);}
 Log("Vehicle FP look2A0 weights installed:4 visual consumers+2 native restart logical consumers; native free-look predicates/event/timers/integration retained, stable HVCL5/15 profile0 only");
}
}
