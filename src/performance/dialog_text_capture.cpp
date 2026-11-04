#include "dialog_text_capture.h"
#include <cstring>
#include <limits>
#include <cmath>
#include "ui_native_status.h"
namespace dialog_text {
namespace {
bool Bytes(Memory m, std::uint32_t at, void* out, std::size_t size) {
    return m.read && at >= 0x10000 && size && size <= 0x10000 &&
        size <= (std::numeric_limits<std::uint32_t>::max)() - at && m.read(m.context,at,out,size);
}
bool Word(Memory m, std::uint32_t at, std::uint32_t& out) { return Bytes(m,at,&out,4); }
bool Add(std::uint32_t a, std::uint32_t b, std::uint32_t& out) {
    if(a > (std::numeric_limits<std::uint32_t>::max)()-b)return false;
    out=a+b;return true;
}
unsigned Fold(unsigned c) {return c>='a'&&c<='z'?c-32:c;}
bool Name(Memory m,std::uint32_t entry,const char* expected) {
    char name[32]{};if(!Bytes(m,entry+12,name,sizeof(name)))return false;
    for(unsigned i=0;i<32;++i){
        if(Fold(static_cast<unsigned char>(name[i]))!=Fold(static_cast<unsigned char>(expected[i])))return false;
        if(!name[i])return true;
    }return false;
}
bool String(Memory m,std::uint32_t at,char* text,std::size_t cap){
    for(std::size_t i=0;i<cap;++i){
        if(at > (std::numeric_limits<std::uint32_t>::max)()-static_cast<std::uint32_t>(i) ||
           !Bytes(m,at+static_cast<std::uint32_t>(i),text+i,1))return false;
        if(!text[i])return true;
    }return false;
}
bool Boolean(Memory m,std::uint32_t root,const char* name,Value& out){
    Value value{};if(!ReadVariable(m,root,name,value))return false;
    std::uint32_t number{};
    if((value.kind==Kind::Integer||value.kind==Kind::Number)&&Unsigned(value,1,number)){out={};out.kind=Kind::Integer;out.integer=number;return true;}
    // Actual 91F4B0 integer setters serialize "0"/"1" as inline type4 strings.
    // Confirmed read-only on authenticated r29 PID19192, both visibility flags.
    if(value.kind==Kind::String&&(value.text[0]=='0'||value.text[0]=='1')&&!value.text[1]){
        out={};out.kind=Kind::Integer;out.integer=value.text[0]=='1'?1u:0u;return true;
    }return false;
}
}
bool Unsigned(const Value& value,std::uint32_t maximum,std::uint32_t& output) noexcept {
    output=0;
    if(value.kind==Kind::Integer){if(value.integer>maximum)return false;output=value.integer;return true;}
    if(value.kind==Kind::Number){const double number=value.number;if(!std::isfinite(number)||number<0||number>maximum||std::trunc(number)!=number)return false;output=static_cast<std::uint32_t>(number);return true;}
    if(value.kind!=Kind::String||!value.text[0])return false;
    std::uint32_t number=0;
    for(std::size_t i=0;i<TextCapacity;++i){const auto c=value.text[i];if(!c){output=number;return true;}if(c<'0'||c>'9')return false;const auto digit=static_cast<std::uint32_t>(c-'0');if(digit>maximum||number>(maximum-digit)/10)return false;number=number*10+digit;}
    return false;
}
bool ReadOptionalVariable(Memory m,std::uint32_t root,const char* name,Value& value){
    value={};if(!name||!name[0]||std::strlen(name)>31)return false;
    std::uint32_t address{},buckets{},scope{},table{},array{};
    if(!Add(root,0x1a8,address)||!Word(m,address,scope)||!Word(m,root,buckets)||!buckets||buckets>65536 ||
       !Add(root,0x120,address)||!Word(m,address,table)||!Word(m,table,array))return false;
    std::uint32_t hash=scope,packed=0;
    for(std::size_t i=0;name[i];){
        // Native 921430 intentionally retains the preceding word's high bytes
        // in a partial final group. Zero-padding each group gives WRONG buckets.
        for(unsigned j=0;j<4&&name[i];++j,++i){
            const unsigned shift=8*j;packed=(packed&~(0xffu<<shift))|
                (static_cast<std::uint32_t>(static_cast<unsigned char>(name[i]))<<shift);
        }
        packed&=0xdfdfdfdfu;hash+=packed;
    }
    std::uint32_t entry{};if(!Add(array,(hash%buckets)*0x4c,entry))return false;
    // Bound malformed cycles without allocating or trusting a native allocator.
    std::uint32_t visited[128]{};
    for(unsigned depth=0;depth<128;++depth){
        if(entry<0x10000||entry>0xffffffb3u)return false;
        for(unsigned i=0;i<depth;++i)if(visited[i]==entry)return false;
        visited[depth]=entry;
        std::uint32_t type{},entryScope{},next{};
        if(!Word(m,entry,type))return false;
        if(!type)return true;
        if(!Word(m,entry+4,entryScope)||!Word(m,entry+8,next))return false;
        if(entryScope==scope&&Name(m,entry,name)){
            Value result{};
            if(type==1){float number{};if(!Bytes(m,entry+0x2c,&number,4))return false;result.kind=Kind::Number;result.number=number;}
            else if(type==2||type==5){
                std::uint32_t pointer{};if(!Word(m,entry+0x2c,pointer)||!String(m,pointer,result.text,TextCapacity))return false;
                result.kind=Kind::String;
            }else if(type==4){if(!String(m,entry+0x2c,result.text,32))return false;result.kind=Kind::String;}
            else return false;
            value=result;return true;
        }
        if(!next)return true;entry=next;
    }return false;
}
bool ReadVariable(Memory m,std::uint32_t root,const char* name,Value& value){
    return ReadOptionalVariable(m,root,name,value)&&value.kind!=Kind::Missing;
}
bool Capture(Memory m,NativeSnapshot& snapshot){
    snapshot={};NativeSnapshot s{};std::uint32_t movie{},callback{},handle{},active{},wait{},vt{};
    unsigned char activeByte{};
    if(!Bytes(m,0xff437c,&activeByte,1)||activeByte!=1||!Word(m,0xff4390,wait)||ui_native_status::BlockingLoad(wait) ||
       !Word(m,0xfe14e8,vt)||vt!=0xc77b34||!Word(m,0xfe14e0,movie)||movie>0xffffff9au ||
       !Word(m,movie+12,callback)||callback!=0xfe14e8||!Word(m,movie+0x24,handle)||
       !Word(m,0xff4378,s.handle)||s.handle!=handle||!Word(m,handle,s.root)||s.root>0xfffffe57u ||
       !Word(m,0xff438c,s.owner)||s.owner<0x10000||!Word(m,0xff4384,s.userArgument)||
       !Word(m,0xff4380,s.pendingResult)||s.pendingResult>2||!Word(m,0xfda5e0,s.language))return false;
    if(!ReadVariable(m,s.root,"m_AvertText",s.body)||s.body.kind!=Kind::String ||
       !ReadVariable(m,s.root,"m_ok",s.ok)||s.ok.kind!=Kind::String ||
       !ReadVariable(m,s.root,"m_Cancel",s.cancel)||s.cancel.kind!=Kind::String ||
       !ReadVariable(m,s.root,"m_no",s.no)||s.no.kind!=Kind::String ||
       !ReadVariable(m,s.root,"m_yes",s.yes)||s.yes.kind!=Kind::String ||
       !Boolean(m,s.root,"IsAvertValid",s.validEnabled)||!Boolean(m,s.root,"IsAvertCancel",s.cancelEnabled))return false;
    // A close/change during copying invalidates the complete owned snapshot.
    if(!Word(m,0xff438c,active)||active!=s.owner||!Bytes(m,0xff437c,&activeByte,1)||activeByte!=1 ||
       !Word(m,0xff4378,active)||active!=s.handle)return false;
    snapshot=s;return true;
}
}
