// Included within phone_content_context. Original map render proxies/matrices
// remain untouched. Original textured proxy321 and source-bound authored markers
// may pass paint. Proxy321 must never be replaced with a solid decorative glyph.
#include "phone_map_content.h"
struct MapShapeCache{DWORD root{},dictionary{},shape[8]{},next[8]{};};static MapShapeCache mapCache{};
static bool MapShapes(DWORD root,DWORD(&shapes)[8]){
 const auto dictionary=Word(reinterpret_cast<void*>(root),0x74);if(!dictionary)return false;
 if(mapCache.root!=root||mapCache.dictionary!=dictionary)mapCache={root,dictionary,{},{}};
 for(unsigned n=0;n<8;++n){const auto& descriptor=phone_map_content::descriptors[n];const auto shape=Word(reinterpret_cast<void*>(dictionary),descriptor.id*4);
  if(!shape||Word(reinterpret_cast<void*>(shape),8)!=descriptor.id)return false;
  const auto next=Word(reinterpret_cast<void*>(shape),4);if(next<shape||next-shape!=descriptor.bytes)return false;
  if(mapCache.shape[n]!=shape||mapCache.next[n]!=next){
   DWORD fnv=2166136261u;const auto* bytes=reinterpret_cast<const BYTE*>(shape);
   for(DWORD at=8;at<descriptor.bytes;++at)fnv=(fnv^bytes[at])*16777619u;
   if(fnv!=descriptor.tailFnv)return false;mapCache.shape[n]=shape;mapCache.next[n]=next;
  }
  shapes[n]=shape;
 }return true;
}
static bool CaptureMap(unsigned char* movie,PhoneContentStateV1& out,Owner& owner){
 auto* handle=reinterpret_cast<unsigned char*>(Word(movie,0x24));const auto root=Word(handle),scope=root+200;DWORD panel{},platform{},released{},objective{};
 if(!phone_context::OptionalZero(root,scope,"fake")||!phone_context::Number(root,scope,"WhatPlatform",platform,1)||platform!=1||
    !phone_context::Number(root,scope,"release_button",released,1)||!phone_context::Equal(root,scope,"whatpage","gps")||
    !phone_context::Resolve(handle,"/menu_gps",559,panel)||!phone_timeline_policy::MapPaint(Word(reinterpret_cast<void*>(panel),0x60))||
    !phone_context::Number(root,scope,"is_objective",objective,1))return false;
 auto& v=out.view;v.kind=Kind::Gps;
 if(!phone_parameters_context::Text(root,scope,"m_gps",v.title,96)||!phone_parameters_context::Text(root,scope,"m_Back",v.back,64)||
    !phone_parameters_context::Text(root,scope,"m_zoom",v.open,64)||!phone_parameters_context::Text(root,scope,"m_move",v.rows[0].title,256)||
    (objective&&(!phone_parameters_context::Text(root,scope,"m_objective_title",v.subtitle,256)||!phone_parameters_context::Text(root,scope,"m_objective",v.body,4096))))return false;
 owner={reinterpret_cast<DWORD>(movie),reinterpret_cast<DWORD>(handle),root,panel,static_cast<DWORD>(v.kind),{}};
 if(!MapShapes(root,owner.mapShapes))return false;
 out.flags=PhoneContentVisible|((!phone_timeline_policy::MapInteractive(Word(reinterpret_cast<void*>(panel),0x60))||released||Word(reinterpret_cast<void*>(panel),0x64)!=2||*reinterpret_cast<BYTE*>(panel+0x68))?PhoneContentBusy:0);
 return true;
}
