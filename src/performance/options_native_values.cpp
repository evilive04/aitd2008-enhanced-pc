#include "options_native_values.h"
#include <cmath>
#include <cstring>
namespace options_native_values {
namespace {
struct ScopeRead {dialog_text::Memory original;std::uint32_t address,scope;};
bool WithScope(void* user,std::uint32_t at,void* out,std::size_t size){auto& s=*static_cast<ScopeRead*>(user);if(at==s.address&&size==4){std::memcpy(out,&s.scope,4);return true;}return s.original.read&&s.original.read(s.original.context,at,out,size);}
}
bool Scoped(dialog_text::Memory memory,std::uint32_t root,std::uint32_t instance,const char* name,dialog_text::Value& value,bool optional){
 value={};if(!memory.read||root<0x10000||root>0xfffffe57u||instance<0x10000||instance>0xffffff1fu)return false;
 std::uint32_t scope{},after{};if(!memory.read(memory.context,instance+0xe0,&scope,4))return false;
 ScopeRead input{memory,root+0x1a8,scope};dialog_text::Value result{};const bool found=optional?dialog_text::ReadOptionalVariable({WithScope,&input},root,name,result):dialog_text::ReadVariable({WithScope,&input},root,name,result);if(!found||!memory.read(memory.context,instance+0xe0,&after,4)||scope!=after)return false;value=result;return true;
}
bool ReadGauge(dialog_text::Memory memory,std::uint32_t widget,Gauge& output){
 output={};if(!memory.read||widget<0x10000||widget>0xffffffe7u)return false;Gauge result{};
 if(!memory.read(memory.context,widget+4,&result.current,4)||!memory.read(memory.context,widget+0xc,&result.maximum,4)||!memory.read(memory.context,widget+0x10,&result.minimum,4)||!memory.read(memory.context,widget+0x14,&result.steps,4)||!std::isfinite(result.current)||!std::isfinite(result.minimum)||!std::isfinite(result.maximum)||!std::isfinite(result.steps)||result.minimum>=result.maximum||result.current<result.minimum||result.current>result.maximum||result.steps<=0||result.steps>65535)return false;output=result;return true;
}
}
