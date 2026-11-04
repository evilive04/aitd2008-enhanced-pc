#pragma once
#include <cwchar>
#include <cstddef>
namespace qte_ui_content{
//54A8A0's actual PC binding wrapper, already supported by the shared rich
// keycap painter. This proves a displayed PC key label; it never invents
// QTE_A for native Back/LT, whose original action index is deliberately -1.
inline bool HasNativePcKey(const wchar_t* text,std::size_t capacity)noexcept{
 if(!text||!capacity)return false;const auto length=wcsnlen_s(text,capacity);if(!length||length==capacity)return false;
 constexpr wchar_t prefix[]=L"#color:F2E6C9FF#[",suffix[]=L"]#nocolor#";
 constexpr std::size_t head=sizeof prefix/sizeof(wchar_t)-1,tail=sizeof suffix/sizeof(wchar_t)-1;
 for(std::size_t at=0;at+head+tail<length;++at){
  if(wcsncmp(text+at,prefix,head))continue;const auto* end=wcsstr(text+at+head,suffix);if(!end)continue;
  const auto count=static_cast<std::size_t>(end-text-at-head);if(!count||count>=64)continue;bool valid=true;
  for(std::size_t n=0;n<count;++n){const auto c=text[at+head+n];if(c<L' '||c==L'['||c==L']'||c==L'#'){valid=false;break;}}
  if(valid)return true;
 }return false;
}
}
