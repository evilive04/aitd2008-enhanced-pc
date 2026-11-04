#include "skin_material_catalog.h"
#include <cmath>
#include <algorithm>
namespace skin_material_catalog {
namespace {
struct Point {float x,y;};
struct Polygon {unsigned first,count;};
struct Hole {float x,y,rx,ry;};
struct Definition {Profile profile;unsigned polygonFirst,polygonCount,holeFirst,holeCount;float feather,strength;unsigned maskFirst,maskCount;};
struct Entry {Identity identity;unsigned profile;};
#include "skin_material_catalog_data.inl"
const Definition* DefinitionOf(const Profile* profile){if(!profile)return nullptr;for(const auto& d:definitions)if(&d.profile==profile)return &d;return nullptr;}
float Ramp(float distance,float width){const float t=(std::max)(0.f,(std::min)(1.f,distance/(std::max)(width,1e-5f)));return t*t*(3-2*t);}
float PolygonStrength(const Polygon& polygon,float x,float y,float feather){bool inside=false;float distance=1000;for(unsigned i=0,j=polygon.count-1;i<polygon.count;j=i++){
 const auto a=points[polygon.first+j],b=points[polygon.first+i];const float dx=b.x-a.x,dy=b.y-a.y,denominator=dx*dx+dy*dy;const float t=denominator>0?(std::max)(0.f,(std::min)(1.f,((x-a.x)*dx+(y-a.y)*dy)/denominator)):0;const float px=x-a.x-t*dx,py=y-a.y-t*dy;distance=(std::min)(distance,std::sqrt(px*px+py*py));if((a.y>y)!=(b.y>y))if(x<(b.x-a.x)*(y-a.y)/(b.y-a.y)+a.x)inside=!inside;}
 return inside?Ramp(distance,feather):0;
}
}
const Profile* Match(const Identity& value) noexcept {
 if(value.pool!=1||!value.width||!value.height||!value.bytes)return nullptr;
 for(const auto& entry:entries){const auto& q=entry.identity;if(q.width==value.width&&q.height==value.height&&q.format==value.format&&q.pool==value.pool&&q.mip==value.mip&&q.mipWidth==value.mipWidth&&q.mipHeight==value.mipHeight&&q.bytes==value.bytes&&q.fnv1a64==value.fnv1a64){for(const auto& d:definitions)if(d.profile.index==entry.profile)return &d.profile;}}
 return nullptr;
}
float Strength(const Profile* profile,float u,float v) noexcept {
 const auto* d=DefinitionOf(profile);if(!d||!std::isfinite(u)||!std::isfinite(v)||u<0||v<0||u>1||v>1)return 0;if(d->profile.kind==Kind::ProvenWholeSkin)return d->strength;
 const float x=u*256,y=v*256;float result=0;for(unsigned n=0;n<d->polygonCount;++n)result=(std::max)(result,PolygonStrength(polygons[d->polygonFirst+n],x,y,d->feather));if(!result)return 0;
 for(unsigned n=0;n<d->holeCount;++n){const auto h=holes[d->holeFirst+n];const float dx=(x-h.x)/h.rx,dy=(y-h.y)/h.ry;const float distance=(std::sqrt(dx*dx+dy*dy)-1)*(std::min)(h.rx,h.ry);result*=Ramp(distance,d->feather);}return result*d->strength;
}
bool BuildMask(const Profile* profile,unsigned width,unsigned height,unsigned char* output,std::size_t bytes) noexcept {
 const auto* definition=DefinitionOf(profile);if(!definition||!output||!width||!height||width>512||height>512||bytes!=static_cast<std::size_t>(width)*height)return false;
 if(width==128&&height==128){std::size_t at=0;for(unsigned n=0;n<definition->maskCount;++n){const auto run=maskRuns[definition->maskFirst+n];const unsigned count=run>>8;const auto value=static_cast<unsigned char>(run&255);if(!count||count>bytes-at)return false;for(unsigned i=0;i<count;++i)output[at++]=value;}return at==bytes;}
 // Minimum of four sub-texel probes avoids enabling hair/eye/clothing islands
 // just because one pixel center falls inside an authored skin polygon.
 for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){float strength=1;for(float dy:{.25f,.75f})for(float dx:{.25f,.75f})strength=(std::min)(strength,Strength(profile,(x+dx)/width,(y+dy)/height));output[y*width+x]=static_cast<unsigned char>(std::floor(strength*255));}return true;
}
unsigned ProfileCount() noexcept{return static_cast<unsigned>(sizeof definitions/sizeof definitions[0]);}
unsigned ApprovedIdentityCount() noexcept{return static_cast<unsigned>(sizeof entries/sizeof entries[0]);}
}
