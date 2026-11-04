#pragma once
namespace phone_roster_content {
enum class Kind:unsigned{None,Bungle,Hartford,Sarah,Tips,Unavailable,Documents};
inline Kind Parse(const wchar_t* text){if(!text)return Kind::None;const wchar_t* routes[]={L"bungle",L"hartford",L"sarah",L"tips",L"no_one",L"documents"};for(unsigned n=0;n<6;++n){unsigned k=0;while(k<32&&text[k]&&routes[n][k]&&text[k]==routes[n][k])++k;if(k<32&&!text[k]&&!routes[n][k])return static_cast<Kind>(n+1);}return Kind::None;}
inline bool HasPortrait(Kind kind){return kind>=Kind::Bungle&&kind<=Kind::Sarah;}
}
