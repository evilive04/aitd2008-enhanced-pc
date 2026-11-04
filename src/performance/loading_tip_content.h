#pragma once
#include "native_bitmap_identity.h"
#include <cstring>
namespace loading_tip_content{
struct Picture{const char* alias;unsigned frame;native_bitmap_identity::Spec image;};
// Exact General173 frame labels, original bitmaps and full authored source
// rectangles. These are actual game instruction pictures, not UI chrome.
inline constexpr Picture pictures[]={
 {"TL_11",5,{163,512,256}},{"TL_13",12,{165,512,256}},
 {"TL_15",19,{167,512,256}},{"TL_16",26,{169,256,256}},
 {"TL_20",33,{171,256,256}}
};
inline unsigned Select(const char* alias,unsigned frame)noexcept{
 if(!alias)return 0;for(unsigned n=0;n<5;++n)if(pictures[n].frame==frame&&!std::strcmp(alias,pictures[n].alias))return n+1;return 0;
}
}
