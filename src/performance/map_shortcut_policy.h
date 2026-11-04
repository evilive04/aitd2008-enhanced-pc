#pragma once
#include <cstdint>
namespace map_shortcut_policy {
enum class Result {None,OpenPhone,OpenMap,CloseMap};
struct State {
 bool previous{},pending{};std::uint32_t started{};
 Result Update(bool down,bool gameplay,bool phoneHomeReady,bool gpsAvailable,bool phoneOther,std::uint32_t now,bool mapOpen=false)noexcept{
  const bool edge=down&&!previous;previous=down;
  if(edge&&mapOpen){pending=false;return Result::CloseMap;}
  if(pending&&(now-started>5000||phoneOther)){pending=false;}
  if(edge&&gameplay){pending=true;started=now;return Result::OpenPhone;}
  if(pending&&phoneHomeReady){pending=false;if(gpsAvailable)return Result::OpenMap;}
  return Result::None;
 }
};
}
