#pragma once
#include <cstdint>
namespace phone_camera_policy {
inline bool Allow(std::uint32_t flags,bool cinematic,bool active,bool validManager,bool validMovie)noexcept{
 return !cinematic&&(!(flags&15u)||((flags&15u)==8u&&active&&validManager&&validMovie));
}
}
