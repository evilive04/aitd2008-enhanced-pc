#pragma once
#include <cstddef>
#include <cstdint>
#include <cmath>
namespace chapter_preview_identity {
struct Reader{bool(*read)(void*,std::uint32_t,void*,std::size_t);void* context;};
template<class T>bool Read(const Reader& r,std::uint32_t address,T& out){return r.read&&address>=65536&&r.read(r.context,address,&out,sizeof out);}
//747F0 folds uppercase ASCII into eight independent byte lanes; overflow in one
// lane never carries into the next. Non-ASCII names require separate evidence.
inline bool FoldName(const char* name,std::size_t length,std::uint64_t& result){
 result=0;if(!name||!length||length>512)return false;
 for(std::size_t n=0;n<length;++n){const auto c=static_cast<unsigned char>(name[n]);if(!c||c>127)return false;const auto value=c>='a'&&c<='z'?c-32:c;const auto shift=(n%8)*8;const auto lane=((result>>shift)+value)&255u;result=(result&~(std::uint64_t{255}<<shift))|(lane<<shift);}return true;
}
inline bool Name(const Reader& r,std::uint32_t pointer,std::uint64_t& result){
 char bytes[512]{};for(unsigned n=0;n<sizeof bytes;++n){if(!Read(r,pointer+n,bytes[n]))return false;if(!bytes[n])return FoldName(bytes,n,result);}return false;
}
inline std::uint32_t Entry(const Reader& r,std::uint32_t list,unsigned index){
 if(!index||index>4096)return 0;std::uint32_t seen[128]{};unsigned used=0,current=1;
 while(list&&used<128){for(unsigned n=0;n<used;++n)if(seen[n]==list)return 0;seen[used++]=list;std::uint32_t descriptor{},next{};std::uint8_t flags{};
  if(!Read(r,list,descriptor)||!Read(r,list+4,next)||!descriptor||!Read(r,descriptor+0x2ec,flags))return 0;
  if(!(flags&6)){if(current==index)return descriptor;++current;}list=next;
 }return 0;
}
inline bool DescriptorAlias(const Reader& r,std::uint32_t descriptor,bool unlocked,std::int32_t progress,float fraction,std::uint64_t& alias){
 if(!descriptor||!std::isfinite(fraction))return false;
 if(!unlocked){std::uint32_t seen[128]{};unsigned used=0;auto node=descriptor;while(node){
   for(unsigned n=0;n<used;++n)if(seen[n]==node)return false;if(used==128)return false;seen[used++]=node;
   std::int32_t required{};float part{};std::uint32_t next{};if(!Read(r,node+0x378,required)||!Read(r,node+0x37c,part)||!Read(r,node+0x2e0,next)||!std::isfinite(part))return false;
   if(progress<required||fraction<part)return FoldName("locked",6,alias);node=next;
  }}
 std::uint32_t name{};return Read(r,descriptor+8,name)&&Name(r,name,alias);
}
struct Selection{unsigned chapter{},chapters{},sequence{},slot{};};
struct Identity{std::uint32_t chapter{},material{},texture{},width{},height{},deviceTexture{};std::uint64_t alias{},textureName{};};
inline bool Capture(const Reader& r,const Selection& s,std::uint32_t material,Identity& out){
 out={};if(!s.chapter||s.chapter>s.chapters||s.chapters>64||s.slot>4||!s.sequence||s.sequence>4096||!material)return false;
 std::uint32_t type{},list{};std::uint64_t alias{};if(!Read(r,material+4,type)||type!=0x2054414d||!Read(r,material+0x60,alias)||!Read(r,0xfdd3f0,list))return false;
 const auto chapter=(s.chapter+s.chapters+s.slot-2)%s.chapters+1;const auto descriptor=Entry(r,list,chapter);if(!descriptor)return false;
 std::uint8_t unlocked{};std::int32_t progress{};float fraction{};std::uint64_t expected{};
 if(!Read(r,0xfaf215,unlocked)||!Read(r,0xfd91e0,progress)||!Read(r,0xfd9694,fraction)||!DescriptorAlias(r,descriptor,unlocked!=0,progress,fraction,expected))return false;
 if(alias!=expected){std::uint32_t scenes{};if(!Read(r,descriptor+0x380,scenes)||!DescriptorAlias(r,Entry(r,scenes,s.slot==1?s.sequence:1),unlocked!=0,progress,fraction,expected)||alias!=expected)return false;}
 std::uint32_t node{},seen[64]{};unsigned used=0;if(!Read(r,material+0x78,node))return false;
 while(node&&node!=0x121bff8&&used<64){for(unsigned n=0;n<used;++n)if(seen[n]==node)return false;seen[used++]=node;std::uint32_t next{};if(!Read(r,node+0x14,next))return false;
  if(next==0x121bff8){Identity image{};image.chapter=chapter;image.material=material;image.alias=alias;std::uint32_t vtable{},kind{},dimension{},format{};
   if(!Read(r,node+0xc,image.texture)||!image.texture||!Read(r,image.texture,vtable)||vtable!=0xcc74f8||!Read(r,image.texture+4,kind)||kind!=0x54584554||
      !Read(r,image.texture+0x10,image.textureName)||!Read(r,image.texture+0x20,image.width)||!Read(r,image.texture+0x24,image.height)||!image.width||!image.height||image.width>4096||image.height>4096||
      !Read(r,image.texture+0x30,format)||(format!=11&&format!=15)||!Read(r,image.texture+0x34,dimension)||dimension!=1||!Read(r,image.texture+0x48,image.deviceTexture)||!image.deviceTexture)return false;
   out=image;return true;
  }node=next;
 }return false;
}
}
