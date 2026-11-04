// Native-only third-person pistol aim. No external animation files, private
// DATA allocations, prototype substitutions or locomotion/carry overrides.
#include "native_pistol_grip.h"
namespace scoped_gun_aim {
#include "scoped_aim_parameters.inl"
struct NativeClip {const wchar_t* hash;DWORD bytes;};
static constexpr NativeClip original[3]={
 {L"d53107ef493118eac054cd72235f75b30de4e87146963deeb686603df149ac35",5120},
 {L"215805fbf85492953c9a2e6c040dac463d125a7e6aa91047ca1e79a52cd5448b",3228},
 {L"b10450e197aabb23c7393f63bd8ac68fbe61250fec299aaeb690e1f112189fb5",4916}};
static bool Hash(const unsigned char* bytes,DWORD length,const wchar_t* expected){
 if(wcslen(expected)!=64)return false;BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;unsigned char result[32]{};
 bool ok=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0;
 if(ok)ok=BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0;
 if(ok)ok=BCryptHashData(hash,const_cast<PUCHAR>(bytes),length,0)>=0;
 if(ok)ok=BCryptFinishHash(hash,result,32,0)>=0;
 if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);
 constexpr wchar_t digits[]=L"0123456789abcdef";
 if(ok)for(unsigned i=0;i<32;++i)if(expected[i*2]!=digits[result[i]>>4]||expected[i*2+1]!=digits[result[i]&15])return false;
 return ok;
}

static bool Owner(unsigned char* hero,unsigned char* animation){return ParameterOwner(hero,animation)&&(Word(hero,0x968)&1)==0;}
static bool OriginalPrototype(unsigned char* prototype){
 if(!prototype||Word(prototype,4)!=1||Word(prototype,8)!=244||Word(prototype,0x54)!=3)return false;
 for(unsigned i=0;i<3;++i){auto* data=Pointer(prototype,0x58+i*16);if(!data||!Hash(data,original[i].bytes,original[i].hash))return false;}
 return true;
}
static bool BoundUnsafe(unsigned char* hero,unsigned char* animation){
 if(!Owner(hero,animation))return false;
 auto* context=animation+0x790;const DWORD count=Word(context,0x118);if(count>64)return false;
 for(DWORD i=0;i<count;++i){auto* node=Pointer(context,0x18+i*4);if(!node||Word(node,0)!=0xc81548||Word(node,0x34)!=3)continue;
  auto* prototype=Pointer(node,4);if(OriginalPrototype(prototype)&&Owner(hero,animation)&&Pointer(context,0x18+i*4)==node&&Pointer(node,4)==prototype)return true;
 }
 return false;
}
static bool Bound(unsigned char* hero,unsigned char* animation){__try{return BoundUnsafe(hero,animation);}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
// An authenticated original125E graph already supplies its authored body/grip.
// It must not receive the separate forced first-person trunk postprocess.
static bool NativeAimBound(unsigned char* hero,unsigned char* animation){return Bound(hero,animation);}
static bool Bind(unsigned char* hero,unsigned char* animation){return Bound(hero,animation);}
static unsigned gripApplied=0,gripRejected=0;
static void* nativeGripRebuild=reinterpret_cast<void*>(0x604630);
static void RebuildGrip(unsigned char* animation,const DWORD* mask){
 __asm {
  push animation
  mov ecx,mask
  call dword ptr [nativeGripRebuild]
 }
}
static bool ApplyGrip(unsigned char* hero,unsigned char* animation){
 __try{
  if(!NativeAimBound(hero,animation))return false;
  auto* hands=Pointer(hero,0xf9c);if(!hands||Pointer(hands,0)!=hero||Pointer(hands,4)||Pointer(hands,0x24)||!IsPistol(Pointer(hands,0x14)))return false;
  auto* lookup=Pointer(animation,0x10);auto* pose=Pointer(animation,0x18);if(!lookup||!pose||!Pointer(animation,0x1c)||*reinterpret_cast<const WORD*>(lookup)!=199)return false;
  // Verify every participating native bone hash/index against the actual
  // skeleton lookup. No remembered buffer/node address survives this call.
  for(unsigned i=0;i<15;++i){const int parent=native_pistol_grip::parents[i];const unsigned expected=parent<0?~0u:native_pistol_grip::bones[parent];if(Word(lookup,4+native_pistol_grip::bones[i]*4)!=expected)return false;bool found=false;for(unsigned n=0;n<199;++n)
   if(Word(lookup,4+199*4+n*4)==native_pistol_grip::hashes[i]){if(Word(lookup,4+199*8+n*4)!=native_pistol_grip::bones[i])return false;found=true;break;}
   if(!found)return false;
  }
  native_pistol_grip::Local local[15]{},before[15]{};
  for(unsigned i=0;i<15;++i)memcpy(&local[i],pose+48*native_pistol_grip::bones[i],48);memcpy(before,local,sizeof(local));
  native_pistol_grip::Result result{};if(!native_pistol_grip::Solve(local,result)){++gripRejected;return false;}
  if(!Owner(hero,animation)||Pointer(animation,0x18)!=pose||Pointer(animation,0x10)!=lookup||Pointer(hero,0xf9c)!=hands||Pointer(hands,4)||Pointer(hands,0x24))return false;
  // Do not commit if the engine has published a different sample meanwhile.
  for(unsigned i=0;i<15;++i)if(memcmp(&before[i],pose+48*native_pistol_grip::bones[i],48)!=0)return false;
  for(unsigned i=11;i<14;++i)memcpy(pose+48*native_pistol_grip::bones[i],&local[i].q,16);
  // Original selective local→model builder updates the real cached render
  // matrices and all left-arm descendants.605C90 is the opposite direction.
  const DWORD changed[16]={0,0,0,0xc0000000u,1u};RebuildGrip(animation,changed);
  if(gripApplied++<4)Log("native_grip applied before=%g after=%g correction=%g/%g/%g",result.before,result.after,result.angle[0],result.angle[1],result.angle[2]);return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){++gripRejected;return false;}
}
} // namespace scoped_gun_aim
