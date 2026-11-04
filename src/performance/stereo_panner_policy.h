#pragma once
#include <cmath>
#include <cwctype>
namespace stereo_audio {
inline bool ParseOutputChannels(const wchar_t* text,unsigned& channels) noexcept {
    channels=0;if(!text)return false;while(std::iswspace(*text))++text;
    if(*text!=L'0'&&*text!=L'2')return false;
    const unsigned value=static_cast<unsigned>(*text++-L'0');
    while(std::iswspace(*text))++text;if(*text)return false;
    channels=value;return true;
}
// The original two speakers both face +Z. Its clamped dot-product panner
// therefore drops the rear hemisphere. Fold only its panning direction;
// original distance, source directivity, obstruction and gains stay native.
inline bool FoldDirection(const float* source,unsigned channels,unsigned speakerMask,float* copy) noexcept {
    if(!source||!copy||channels!=2||speakerMask!=3)return false;
    for(unsigned n=0;n<4;++n)if(!std::isfinite(source[n]))return false;
    if(source[2]>=0)return false;
    for(unsigned n=0;n<4;++n)copy[n]=source[n];
    copy[2]=-copy[2];return true;
}
}
