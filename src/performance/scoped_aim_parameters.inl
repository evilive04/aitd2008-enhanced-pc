// Native actor-owned float variables. Included inside scoped_gun_aim.
struct AimVariable {const char* name;float target;float neutral;unsigned char* pointer{};float previous{};bool captured{},existed{},foreign{};};
struct AimParameters {unsigned char* hero{};unsigned char* animation{};ItemKey identity{};AimVariable variables[4]={{"$_RightBlendInAutoLock",.5f,.5f},{"$_RightBlendOutAutoLockDown",0.f,0.f},{"$_RightBlendOutAutoLockMiddle",1.f,1.f},{"$_RightBlendOutAutoLockUp",0.f,0.f}};};
static AimParameters aimParameters{};
static bool SameOwnedFloat(float value,float owned){return std::isfinite(value)&&fabsf(value-owned)<=.00001f;}
static unsigned char* FindParameter(unsigned char* animation,const char* name){
 auto* entry=Pointer(animation,0x8d8);for(unsigned n=0;entry&&n<256;++n,entry=Pointer(entry,0xd4))if(!strncmp(reinterpret_cast<const char*>(entry),name,32))return entry;return nullptr;
}
#ifndef AITD_SCOPED_PARAMETER_TEST
static __declspec(noinline) void NativeFloat(unsigned char** list,const char* name,float value){
 __asm {
  mov eax,name
  mov ecx,list
  push value
  mov edx,606c80h
  call edx
 }
}
#else
static void NativeFloat(unsigned char** list,const char* name,float value){TestNativeFloat(list,name,value);}
#endif
static bool ParameterOwner(unsigned char* hero,unsigned char* animation){return hero&&Word(hero,0)==0xc834c0&&Pointer(hero,0x3b4)==animation&&animation&&Word(animation,0xc)==1;}
static bool PrepareWeightsUnsafe(unsigned char* hero,unsigned char* animation){
 if(!ParameterOwner(hero,animation)||(Word(hero,0x968)&1))return false;const auto identity=OwnedItemKey(hero);
 if(aimParameters.hero!=hero||aimParameters.animation!=animation||!(aimParameters.identity==identity)){aimParameters=AimParameters{};aimParameters.hero=hero;aimParameters.animation=animation;aimParameters.identity=identity;}
 for(auto& variable:aimParameters.variables){
  auto* current=FindParameter(animation,variable.name);
  if(variable.captured){
   if(!current||Word(current,0x20)!=3)return false;
   if(current!=variable.pointer||!SameOwnedFloat(*reinterpret_cast<float*>(current+0x28),variable.target))variable.foreign=true;
   continue; // Foreign native/script changes win; no per-frame rewrite.
  }
  if(current&&Word(current,0x20)!=3)return false;variable.existed=current!=nullptr;if(current)variable.previous=*reinterpret_cast<float*>(current+0x28);
  if(!ParameterOwner(hero,animation)||!(OwnedItemKey(hero)==identity))return false;
  NativeFloat(reinterpret_cast<unsigned char**>(animation+0x8d8),variable.name,variable.target);
  current=FindParameter(animation,variable.name);if(!current||Word(current,0x20)!=3||*reinterpret_cast<float*>(current+0x28)!=variable.target)return false;
  variable.pointer=current;variable.captured=true;
 }
 return true;
}
static __declspec(noinline) bool PrepareWeights(unsigned char* hero,unsigned char* animation){__try{return PrepareWeightsUnsafe(hero,animation);}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static void RestoreWeightsUnsafe(unsigned char* hero,unsigned char* animation){
 if(aimParameters.hero!=hero||aimParameters.animation!=animation||!ParameterOwner(hero,animation)||!(aimParameters.identity==OwnedItemKey(hero))){aimParameters=AimParameters{};return;}
 for(auto& variable:aimParameters.variables){
  if(!variable.captured||variable.foreign)continue;auto* current=FindParameter(animation,variable.name);
  if(current!=variable.pointer||Word(current,0x20)!=3||!SameOwnedFloat(*reinterpret_cast<float*>(current+0x28),variable.target))continue;
  if(ParameterOwner(hero,animation)&&aimParameters.identity==OwnedItemKey(hero))NativeFloat(reinterpret_cast<unsigned char**>(animation+0x8d8),variable.name,variable.existed?variable.previous:variable.neutral);
  // Newly created entries remain owned by the native animation list until its
  // normal teardown. Old graph/snapshot evaluators may still reference them.
 }
 aimParameters=AimParameters{};
}
static void RestoreWeights(unsigned char* hero,unsigned char* animation){__try{RestoreWeightsUnsafe(hero,animation);}__except(EXCEPTION_EXECUTE_HANDLER){aimParameters=AimParameters{};}}

// Called only after the caller validates its current owned camera aim ray.
// target remains the last value we actually wrote, never a pending desired value.
static bool UpdatePitchUnsafe(unsigned char* hero,unsigned char* animation,float pitch){
 if(!ParameterOwner(hero,animation)||(Word(hero,0x968)&1)||!std::isfinite(pitch)||aimParameters.hero!=hero||aimParameters.animation!=animation||!(aimParameters.identity==OwnedItemKey(hero)))return false;
 for(auto& variable:aimParameters.variables){
  auto* current=FindParameter(animation,variable.name);
  if(!variable.captured||variable.foreign||current!=variable.pointer||!current||Word(current,0x20)!=3)return false;
  const float value=*reinterpret_cast<float*>(current+0x28);
  if(!std::isfinite(value)||fabsf(value-variable.target)>.00001f){variable.foreign=true;return false;}
 }
 const float normalized=std::clamp(pitch/1.57079632679f,-1.f,1.f);
 const float desired[4]={.5f+.5f*normalized,std::max(0.f,-normalized),1.f-fabsf(normalized),std::max(0.f,normalized)};
 for(unsigned i=0;i<4;++i){auto& variable=aimParameters.variables[i];if(fabsf(variable.target-desired[i])<=.00001f)continue;
  if(!ParameterOwner(hero,animation)||!(aimParameters.identity==OwnedItemKey(hero)))return false;
  NativeFloat(reinterpret_cast<unsigned char**>(animation+0x8d8),variable.name,desired[i]);
  auto* current=FindParameter(animation,variable.name);if(current!=variable.pointer||Word(current,0x20)!=3||*reinterpret_cast<float*>(current+0x28)!=desired[i])return false;variable.target=desired[i];
 }
 return true;
}
[[maybe_unused]] static __declspec(noinline) bool UpdatePitch(unsigned char* hero,unsigned char* animation,float pitch){__try{return UpdatePitchUnsafe(hero,animation,pitch);}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
