#include "bleeding_ui_state.h"
namespace bleeding_timer{
static bool Parse(const char* text,char suffix,DWORD maximum,DWORD& out){out=0;const auto length=strnlen_s(text,16);if(length<2||length>4||text[length-1]!=suffix)return false;for(size_t n=0;n<length-1;++n){if(text[n]<'0'||text[n]>'9')return false;out=out*10+text[n]-'0';}return out<=maximum;}
static bool Read(BYTE* handle,DWORD root,BleedingUiValue& out){out={};DWORD b[2]{};
 if(!menu_context::Resolve(handle,"/hemo",b)||b[1]!=root||!b[0])return false;const auto node=b[0],resource=phone_context::Word(reinterpret_cast<void*>(node),0x18);if(!resource||phone_context::Word(reinterpret_cast<void*>(resource),8)!=564)return false;
 const auto frame=phone_context::Word(reinterpret_cast<void*>(node),0x60);if(frame<1||frame>123)return false;if(frame==1||!(phone_context::Word(reinterpret_cast<void*>(node),0x6c)&1))return true;
 char minute[16]{},second[16]{};if(!phone_parameters_context::Raw(root,root+200,"HemMin",minute,16)||!phone_parameters_context::Raw(root,root+200,"HemSec",second,16)||!Parse(minute,'\'',999,out.minutes)||!Parse(second,'"',59,out.seconds))return false;
 out.stage=phone_context::Word(reinterpret_cast<void*>(0x10815e4));if(!out.stage||out.stage>4)return false;out.nativeFrame=frame;return true;
}
}
