#pragma once
#include <cstdint>
namespace fixed_camera_policy {
struct KeyEdge {
 bool focused=false,previous=false;
 bool Update(bool down,bool allowed,bool modified) noexcept {
  if(!allowed){focused=false;previous=down;return false;}
  if(!focused){focused=true;previous=down;return false;}
  const bool edge=down&&!previous;previous=down;return edge&&!modified;
 }
};
inline bool Gameplay(std::uint32_t menus,std::uint32_t loading,std::uint32_t state,
 std::uint32_t context,std::uint32_t input,std::uint32_t controller,
 bool cinematic,bool phone,bool blocked,bool ordinary) noexcept {
 return !menus&&!loading&&state==35&&!(context&15u)&&(input&0x38u)==8u&&
  (controller&6u)==6u&&!cinematic&&!phone&&!blocked&&ordinary;
}
}
