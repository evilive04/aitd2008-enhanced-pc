#pragma once
#include <cstddef>
namespace general_countdown_content{
// Native639E90 writes "%02d'" and "%02d\"" into the two text fields.
// Keep presentation read-only; do not interpret an AVM display string as a
// plain integer or calculate a competing timer from wall-clock time.
inline bool Parse(const char* text,char suffix,unsigned maximum,unsigned& out)noexcept{
 out=0;if(!text||(suffix!='\''&&suffix!='"')||maximum>999)return false;
 std::size_t count=0;while(count<5&&text[count])++count;if(count<3||count>4||text[count-1]!=suffix)return false;
 unsigned value=0;for(std::size_t n=0;n<count-1;++n){if(text[n]<'0'||text[n]>'9')return false;value=value*10+static_cast<unsigned>(text[n]-'0');}
 if(value>maximum||(count==4&&value<100))return false;out=value;return true;
}
}
