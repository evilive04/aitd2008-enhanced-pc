#pragma once
// Native inventory base-item bitmap crops, derived from exact source geometry.
#include <cstring>
namespace inventory_native_icons {
struct Entry{const char* name;InventoryUiImage image;};
inline constexpr Entry items[]={
 {"symbol",{38,1024,256,1.700000f,174.150000f,76.750000f,76.750000f}},
 {"d_eagle",{38,1024,256,866.118275f,0.000000f,107.349987f,69.780429f}},
 {"knife",{38,1024,256,343.078341f,130.709438f,43.360925f,120.603917f}},
 {"nothing",{484,128,128,3,5,97,75}},
 {"tissue",{83,64,64,0,0,64,64}},
 {"scotch",{38,1024,256,389,129,101,115}},
 {"batteries",{38,1024,256,528,15,117,66}},
 {"maglight",{428,512,128,0.000000f,0.000000f,311.000000f,98.000000f}},
 {"red_stick",{85,256,64,0.000000f,0.000000f,233.000000f,62.000000f}},
 {"green_stick",{38,1024,256,85.668665f,171.224737f,261.449596f,73.500164f}},
 {"bandages",{79,64,128,0.000000f,0.000000f,49.000000f,72.000000f}},
 {"blood_pack",{81,128,256,0.000000f,0.000000f,111.000000f,153.000000f}},
 {"h_bottle",{320,128,256,0.000000f,0.000000f,128.000000f,256.000000f}},
 {"p_bottle",{318,128,256,0.000000f,0.000000f,128.000000f,256.000000f}},
 {"g_bottle",{322,128,256,0.000000f,0.000000f,127.995620f,256.000000f}},
 {"spray_medical",{94,64,128,0.000000f,0.000000f,63.000000f,128.000000f}},
 {"spray_rust",{38,1024,256,493.591791f,112.133758f,62.949615f,141.499615f}},
 {"spray_mosquito",{96,64,128,0.000000f,0.000000f,63.000000f,128.000000f}},
 {"glock_gun",{490,128,128,0.000000f,0.000000f,94.000000f,73.000000f}},
 {"screwdriver",{493,32,128,0.000000f,0.000000f,20.000000f,100.000000f}},
 {"lighter",{38,1024,256,780.904984f,0.000000f,85.150387f,102.338982f}},
};
inline InventoryUiImage Find(const char* name){if(!name)return {};if(!std::strcmp(name,"flashlight"))name="maglight";else if(!std::strcmp(name,"handkerchieves"))name="tissue";else if(!std::strcmp(name,"cock_knife")||!std::strcmp(name,"flick_knife")||!std::strcmp(name,"hunt_knife"))name="knife";for(const auto& e:items){const auto n=std::strlen(e.name);if(!std::strcmp(name,e.name)||(!std::strncmp(name,e.name,n)&&name[n]=='_'))return e.image;}return {};}
}
