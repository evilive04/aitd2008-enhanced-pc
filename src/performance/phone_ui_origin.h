#pragma once
#include "dialog_text_capture.h"
namespace phone_ui_origin {
// Read-only exact-r29 origin classification. Minimap and full phone are distinct.
// This is sampled only on an authenticated successful generic open, not per frame.
struct Identity {std::uint32_t movie{},handle{},root{},mode{};};
inline bool Capture(const dialog_text::Memory& memory)noexcept{
    if(!memory.read)return false;
    const auto word=[&](std::uint32_t at,std::uint32_t& value){return memory.read(memory.context,at,&value,4);};
    const auto byte=[&](std::uint32_t at,unsigned char& value){return memory.read(memory.context,at,&value,1);};
    const auto read=[&](Identity& s){std::uint32_t manager{},callback{},flags{},owner{};unsigned char active{},gameplay{},visible{};
        return word(0xff4780,manager)&&manager==0xc799f8&&word(0xff47a0,callback)&&callback==0xc79974&&
            byte(0xff4b7f,active)&&active==1&&word(0xff4290,flags)&&flags==0&&byte(0xfd8be5,gameplay)&&gameplay!=0&&
            word(0xff4790,s.movie)&&s.movie&&s.movie<=0xffffff00u&&word(s.movie+12,owner)&&owner==0xff47a0&&
            byte(s.movie+0x65,visible)&&visible!=0&&word(s.movie+0x24,s.handle)&&s.handle&&word(s.handle,s.root)&&s.root&&
            word(0xff4b88,s.mode)&&s.mode<=5;
    };
    Identity first,second;return read(first)&&read(second)&&first.movie==second.movie&&first.handle==second.handle&&first.root==second.root&&first.mode==second.mode;
}
}
