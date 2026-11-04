#pragma once
#include "native_bitmap_identity.h"
#include "phone_roster_content.h"
namespace phone_portrait_identity {
using Reader=native_bitmap_identity::Reader;
using Identity=native_bitmap_identity::Identity;
inline unsigned Route(const wchar_t* text){return static_cast<unsigned>(phone_roster_content::Parse(text));}
inline bool Capture(const Reader& reader,std::uint32_t root,unsigned route,Identity& out,unsigned* stage=nullptr){
 if(stage)*stage=1;out={};if(route<1||route>3)return false;const unsigned ids[]={361,372,375};
 return native_bitmap_identity::Capture(reader,root,{ids[route-1],128,256},out,stage);
}
}
