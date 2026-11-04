#pragma once
#include <cstdint>
namespace startup_skip {
// An allowlist at startup-only callers, never a global video filename filter.
// Unknown/custom BootMovie entries are intentionally forwarded unchanged.
inline bool SameMovie(const char* path,const char* name){
    if(!path)return false;
    for(unsigned i=0;i<32;++i){
        char c=path[i];if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');
        if(c!=name[i])return false;
        if(!c)return true;
    }
    return false;
}
inline bool IntroMovie(const char* path,std::uint32_t flags){
    return flags==0xf&&(SameMovie(path,"atari.bik")||SameMovie(path,"eden.bik")||SameMovie(path,"credits0.bik"));
}
}
