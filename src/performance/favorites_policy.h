#pragma once
#include <cmath>
namespace favorites_policy {
constexpr unsigned None=~0u;
inline unsigned Sector(float nx,float ny,float aspect){
 if(!std::isfinite(nx)||!std::isfinite(ny)||!std::isfinite(aspect)||aspect<=0)return None;
 const float x=(nx-.5f)*aspect,y=ny-.46f,r=std::sqrt(x*x+y*y);
 if(r<.12f||r>.255f)return None;
 float a=std::atan2(y,x)+1.5707963268f+.5235987756f;
 while(a<0)a+=6.2831853072f;while(a>=6.2831853072f)a-=6.2831853072f;
 return static_cast<unsigned>(a/1.0471975512f)%6;
}
inline unsigned Button(float nx,float ny,float aspect){
 const float x=(nx-.5f)*aspect,y=ny-.46f;
 if(y<.31f||y>.36f)return None;
 if(x>=-.30f&&x<=-.12f)return 0;
 if(x>=-.10f&&x<=.10f)return 1;
 if(x>=.12f&&x<=.30f)return 2;return None;
}
struct Gesture {
 bool previous{},blocked{};unsigned began{};
 bool Update(bool down,unsigned now,bool allowed,bool immediate=false){
  if(!allowed){previous=down;blocked=down;return false;}
  if(!down){previous=false;blocked=false;return false;}
  if(!previous){began=now;previous=true;if(immediate){blocked=true;return true;}return false;}
  if(!blocked&&now-began>=250){blocked=true;return true;}return false;
 }
};
}
