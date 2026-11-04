#pragma once
#include <cstdint>
#include <cstddef>
namespace native_bitmap_identity {
struct Reader{bool(*read)(void*,std::uint32_t,void*,std::size_t);void* context;};
struct Identity{std::uint32_t bitmap{},material{},texture{},width{},height{};std::uint64_t name{};};
template<class T>bool Read(const Reader& r,std::uint32_t p,T& out){return r.read&&p>=65536&&r.read(r.context,p,&out,sizeof out);}
struct Spec{std::uint32_t id{},width{},height{};};
inline bool Capture(const Reader& r,std::uint32_t root,const Spec& spec,Identity& out,unsigned* stage=nullptr){
 if(stage)*stage=1;
 out={};if(!root||!spec.id||spec.id>65535||!spec.width||!spec.height||spec.width>4096||spec.height>4096)return false;std::uint32_t dictionary{},bitmap{},vtable{},next{},id{},width{},height{},material{},kind{};float uv[4]{};
 if(!Read(r,root+0x74,dictionary)||!Read(r,dictionary+spec.id*4,bitmap)||!bitmap||!Read(r,bitmap,vtable)||vtable!=0xccfb88||!Read(r,bitmap+4,next)||next<bitmap||next-bitmap!=40||
    !Read(r,bitmap+8,id)||id!=spec.id||!Read(r,bitmap+12,width)||width!=spec.width||!Read(r,bitmap+16,height)||height!=spec.height||!r.read(r.context,bitmap+20,uv,sizeof uv)||uv[0]!=0||uv[1]!=0||uv[2]!=1||uv[3]!=1||
    !Read(r,bitmap+36,material)||!material||!Read(r,material+4,kind)||kind!=0x2054414d)return false;
 if(stage)*stage=2;
 //92A3D0 passes dictionary[bitmapID]+24h to the native material callback.
 // Callers separately prove that the native material callback preserves it.
 std::uint32_t node{},seen[64]{},texture{},used=0;if(!Read(r,material+0x78,node))return false;
 while(node&&node!=0x121bff8&&used<64){for(unsigned n=0;n<used;++n)if(seen[n]==node)return false;seen[used++]=node;std::uint32_t candidate{},after{};
  if(!Read(r,node+0xc,candidate)||!Read(r,node+0x14,after))return false;
  if(candidate){std::uint32_t magic{};if(!Read(r,candidate+4,magic)||magic!=0x54584554||texture)return false;texture=candidate;}
  node=after;
 }
 if(node!=0x121bff8||!texture)return false;if(stage)*stage=3;Identity image{};image.bitmap=bitmap;image.material=material;image.texture=texture;std::uint32_t type{},dimension{},deviceTexture{};
 if(!Read(r,texture,type)||type!=0xcc74f8||!Read(r,texture+0x10,image.name)||!Read(r,texture+0x20,image.width)||image.width!=width||!Read(r,texture+0x24,image.height)||image.height!=height||
    !Read(r,texture+0x34,dimension)||dimension!=1||!Read(r,texture+0x48,deviceTexture)||!deviceTexture)return false;out=image;if(stage)*stage=100;return true;
}
}
