// Exact-r29 FSCommand broadcast.5499E0 uses EDI=movie, three stack arguments
// (name low/high, text), RET12. Like the original AVM path it notifies the movie
// callback AND all four subscribed game listeners. Calling just the movie's
// callback drops inventory commands handled by its other listeners.
namespace native_ui_command {
static DWORD dispatcherSite=0x5499e0;
static bool Name(const char* text,DWORD& low,DWORD& high){
 if(!text)return false;ULONGLONG value{};unsigned n=0;
 while(n<8&&text[n]){const unsigned char c=static_cast<unsigned char>(text[n]);if((c<'a'||c>'z')&&(c<'0'||c>'9')&&c!='_')return false;value=(value<<8)|c;++n;}
 if(!n||text[n])return false;low=static_cast<DWORD>(value);high=static_cast<DWORD>(value>>32);return true;
}
static __declspec(noinline) bool Send(unsigned char* movie,const char* text){
 DWORD nameLow{},nameHigh{};if(!movie||!Name(text,nameLow,nameHigh))return false;
 const BYTE expected[]={0x8b,0x4f,0x0c,0x8b,0x54,0x24,0x04};
 const auto* site=reinterpret_cast<const volatile BYTE*>(dispatcherSite);for(unsigned n=0;n<sizeof expected;++n)if(site[n]!=expected[n])return false;
 __asm{
  push edi
  mov edi,movie
  push text
  push nameHigh
  push nameLow
  call dword ptr [dispatcherSite]
  pop edi
 }
 return true;
}
}
