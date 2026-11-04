#pragma once
#include "ui_art.h"
namespace phone_status_art{
// Boolean native connection, not an invented signal-strength or battery meter.
inline void Append(ui_art::Scene& s,bool connected,float x=1755,float y=97){
 const D3DCOLOR ink=connected?0xffe9e5dc:0xff818b92;
 s.Line(x,y+14,x,y+46,1.6f,ink);s.Arc(x,y+9,3,0,6.2831853f,1.5f,ink);
 if(connected){s.Arc(x,y+9,13,-.85f,.85f,1.3f,ink);s.Arc(x,y+9,13,2.29159265f,3.99159265f,1.3f,ink);s.Arc(x,y+9,23,-.85f,.85f,1.1f,ink);s.Arc(x,y+9,23,2.29159265f,3.99159265f,1.1f,ink);}
 else{s.Line(x-19,y-8,x+19,y+34,1.6f,ink);}
}
}
