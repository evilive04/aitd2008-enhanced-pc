#include "native_ui_text_hook.h"
#include "native_ui_text_mask.h"
#include <map>
#include <unordered_map>
#include <cstring>
#include <cmath>
namespace native_ui_text_hook {
namespace {
void* textOriginal=nullptr;void* glyphOriginal=nullptr;void* maskedOriginal=nullptr;void* dynamicOriginal=nullptr;Emit emitter=nullptr;bool nativeMode=false,installed=false;
void* layoutOriginal=reinterpret_cast<void*>(0x92dbb0);
void* shapeOriginal=nullptr;
PaintOwner paintOwner=nullptr;
ContentPaint contentPaint=nullptr;
ContentInstancePaint contentInstancePaint=nullptr;
ContentTransform contentTransform=nullptr;
PointerOwner pointerOwner=nullptr;void* pointerOriginal=nullptr;
void __cdecl UpdatePointerOwner()noexcept{if(pointerOwner)pointerOwner();}
volatile LONG paintCounters[3]{};
// First seven entries retain the public Statistics layout. Diagnostic entries
// seven/eight count dynamic scope and completed-layout observer invocations.
volatile LONG counters[9]{};
bool Copy(std::uintptr_t source,void* target,std::size_t size) noexcept {
    if(source<0x10000||!target||size>1024u*1024u||source>0xffffffffu-size)return false;
    __try{std::memcpy(target,reinterpret_cast<const void*>(source),size);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
template<class T>bool Read(std::uintptr_t at,T& value) noexcept{return Copy(at,&value,sizeof value);}
bool __cdecl PaintOwned() noexcept {
    // Global+28=2 is the native non-paint path. +2C builds/queries masks;
    // suppressing either would corrupt hit tests or the next sibling's clipping.
    unsigned root{},mode{};unsigned char mask{};
    if(!paintOwner||!Read(0xfd63e0,root)||!root||!Read(root+0x28,mode)||mode==2||
       !Read(root+0x2c,mask)||mask)return false;
    return paintOwner(root);
}
std::uintptr_t __cdecl TransformOwnedShape(std::uintptr_t shape,std::uintptr_t context,std::uintptr_t instance) noexcept {
    unsigned root{},mode{};
    if(!contentTransform||!Read(0xfd63e0,root)||!root||!Read(root+0x28,mode)||mode==2)return context;
    unsigned char copy[44]{};float matrix[6]{};
    if(!Copy(context,copy,sizeof copy))return context;
    std::memcpy(matrix,copy+0x14,sizeof matrix);
    if(!contentTransform(root,shape,instance,matrix))return context;
    for(float value:matrix)if(!std::isfinite(value))return context;
    //92A200/92A3D0 consume the context synchronously into native vertices;
    // no pointer to it is retained. Separate bounded TLS slots also protect
    // surrounding primitive calls from incidental nested callback activity.
    alignas(16) static thread_local unsigned char copies[8][48]{};
    static thread_local unsigned cursor=0;
    auto* shadow=copies[cursor++%8];std::memcpy(shadow,copy,sizeof copy);
    std::memcpy(shadow+0x14,matrix,sizeof matrix);
    unsigned flags{};std::memcpy(&flags,shadow,sizeof flags);flags|=2u;
    std::memcpy(shadow,&flags,sizeof flags);
    return reinterpret_cast<std::uintptr_t>(shadow);
}
bool __cdecl PaintOwnedShape(std::uintptr_t shape,std::uintptr_t instance) noexcept {
    if(!PaintOwned())return false;unsigned root{};
    if(!Read(0xfd63e0,root))return true;
    if(contentInstancePaint&&contentInstancePaint(root,shape,instance))return false;
    return !contentPaint||!contentPaint(root,shape);
}
struct FontSnapshot {std::uintptr_t address{},texture{};std::vector<std::uint8_t> bytes;};
struct Identity {std::uintptr_t quad{},texture{};unsigned codepoint{},start{},count{};native_ui_text::Matrix expected;};
struct Scope {unsigned depth{},cursor{};std::uintptr_t global{},dictionary{};bool valid=false;std::vector<Identity> glyphs;std::map<unsigned,FontSnapshot> fonts;std::unordered_map<unsigned,native_ui_text::Geometry> cache;std::vector<native_ui_text::Vertex> scratch,prepared;};
thread_local Scope scope;
thread_local const std::vector<native_ui_text::Matrix>* replacementTransforms=nullptr;
thread_local unsigned dynamicDepth=0;thread_local std::uintptr_t dynamicText=0,dynamicContext=0;thread_local bool dynamicScope=false;
bool Geometry(unsigned cp) noexcept;
bool Preflight(unsigned vertices,unsigned glyphs) noexcept;
bool __cdecl NativeEmit(const DrawRequest*) noexcept;
bool Font(void*,std::uint16_t id,native_ui_text::Bytes& result) noexcept {
    try{auto found=scope.fonts.find(id);if(found==scope.fonts.end()){
        std::uint32_t ptr{},next{},vtable{},atlas{},texture{};
        if(!Read(scope.dictionary+id*4u,ptr)||!Read(ptr,vtable)||vtable!=0xccfb6c||!Read(ptr+4,next)||next<ptr||next-ptr<1112||next-ptr>65536||!Read(ptr+12,atlas)||atlas>65535||!Read(scope.dictionary+atlas*4u,texture))return false;
        const unsigned size=next-ptr;FontSnapshot f;f.address=ptr;f.texture=texture;f.bytes.resize(size);if(!Copy(ptr,f.bytes.data(),size))return false;const unsigned kind=10;std::memcpy(f.bytes.data(),&kind,4);std::memcpy(f.bytes.data()+4,&size,4);found=scope.fonts.emplace(id,std::move(f)).first;
    }result={found->second.bytes.data(),found->second.bytes.size()};return true;}catch(...){return false;}
}
bool White(void*,std::uint32_t,std::uint32_t& color) noexcept{color=0xffffffff;return true;}
native_ui_text::Matrix Compose(const native_ui_text::Matrix& a,const native_ui_text::Matrix& b) noexcept{return {a.xx*b.xx+a.xy*b.yx,a.yx*b.xx+a.yy*b.yx,a.xx*b.xy+a.xy*b.yy,a.yx*b.xy+a.yy*b.yy,a.xx*b.tx+a.xy*b.ty+a.tx,a.yx*b.tx+a.yy*b.ty+a.ty};}
void __cdecl Begin(std::uintptr_t text,std::uintptr_t context) noexcept {
    ++scope.depth;if(scope.depth!=1)return;InterlockedIncrement(counters);scope.valid=false;scope.cursor=0;scope.glyphs.clear();scope.fonts.clear();scope.prepared.clear();if(!emitter)return;
    try{
        std::uint32_t vtable{},next{},global{},dictionary{};
        if(!Read(text,vtable)||vtable!=0xccfd20||!Read(text+4,next)||next<text||next-text<56||next-text>1024*1024||!Read(0xfd63e0,global)||!Read(global+0x74,dictionary))return;
        const unsigned size=next-static_cast<unsigned>(text);scope.global=global;scope.dictionary=dictionary;std::vector<std::uint8_t> bytes(size);if(!Copy(text,bytes.data(),size))return;const unsigned kind=11;std::memcpy(bytes.data(),&kind,4);std::memcpy(bytes.data()+4,&size,4);
        unsigned mask{},flags{};if(!Read(global+0x1bc,mask))return;native_ui_text::Matrix parent;native_ui_text_mask::Snapshot maskSnapshot;
        if(nativeMode){if(!Read(context,flags))return;if(flags&2u){float m[6]{};if(!Copy(context+0x14,m,sizeof m))return;parent={m[0],m[2],m[3],m[1],m[4],m[5]};}
            unsigned char disabled{};if(!Read(global+0x2f,disabled))return;if(mask&& !disabled){unsigned pointer{};if(mask>65536||!Read(global+0x1c4,pointer))return;std::vector<float> words(mask);if(!Copy(pointer,words.data(),mask*4u)||FAILED(native_ui_text_mask::Decode(words.data(),mask,false,maskSnapshot)))return;}
        }else if(mask)return;
        native_ui_text::Capture capture;if(FAILED(native_ui_text::Decode({bytes.data(),bytes.size()},Font,nullptr,capture)))return;
        if(replacementTransforms&&replacementTransforms->size()!=capture.glyphs.size())return;unsigned vertices=0,maxGlyph=0,glyphNumber=0;
        for(const auto& glyph:capture.glyphs){const auto found=scope.fonts.find(glyph.font);if(found==scope.fonts.end())return;const auto& font=found->second;std::uint32_t relative{};std::memcpy(&relative,font.bytes.data()+1116u+glyph.nativeIndex*8u,4);Identity identity;identity.quad=font.address+relative+28u;identity.texture=font.texture;identity.codepoint=glyph.codepoint;identity.expected=Compose(parent,glyph.transform);
            if(nativeMode){if(!Geometry(glyph.codepoint)){InterlockedIncrement(counters+5);return;}const auto& source=scope.cache.find(glyph.codepoint)->second.triangles;std::vector<ui_vector_text::Triangle> triangles,clipped;triangles.reserve(source.size()/3);for(std::size_t i=0;i<source.size();i+=3)triangles.push_back({{source[i].x,source[i].y},{source[i+1].x,source[i+1].y},{source[i+2].x,source[i+2].y}});
                if(FAILED(native_ui_text_mask::Clip(triangles,replacementTransforms?Compose(parent,(*replacementTransforms)[glyphNumber]):identity.expected,maskSnapshot,21845,clipped))||clipped.size()*3>65535u-vertices){InterlockedIncrement(counters+4);return;}identity.start=vertices;identity.count=static_cast<unsigned>(clipped.size()*3);vertices+=identity.count;if(identity.count>maxGlyph)maxGlyph=identity.count;for(auto t:clipped)for(auto p:{t.a,t.b,t.c})scope.prepared.push_back({p.x,p.y,0xffffffff});
            }scope.glyphs.push_back(identity);++glyphNumber;
        }
        if(nativeMode){scope.scratch.reserve(maxGlyph);if(!Preflight(vertices,static_cast<unsigned>(scope.glyphs.size()))){InterlockedIncrement(counters+4);return;}}
        scope.valid=true;InterlockedIncrement(counters+1);
    }catch(...){scope.valid=false;}
}
void __cdecl End() noexcept{if(scope.depth)--scope.depth;if(!scope.depth)scope.valid=false;}
void __cdecl DynamicBegin(std::uintptr_t text,std::uintptr_t context) noexcept {InterlockedIncrement(counters+7);++dynamicDepth;if(dynamicDepth!=1||scope.depth)return;dynamicText=text;dynamicContext=context;dynamicScope=false;scope.valid=false;}
void __cdecl DynamicEnd() noexcept {if(dynamicDepth==1&&dynamicScope){End();dynamicScope=false;}if(dynamicDepth)--dynamicDepth;}
void __cdecl CaptureLayout(std::uintptr_t layout,std::uintptr_t text,std::uintptr_t context) noexcept {
    InterlockedIncrement(counters+8);
    if(!nativeMode||dynamicDepth!=1||dynamicScope||scope.depth||text!=dynamicText||context!=dynamicContext||!emitter)return;
    try{
        unsigned vt{},global{},dict{},fontPointer{},layoutFont{},count{},drawable{};std::uint16_t fontId{},fontCount{},sx{},sy{};
        if(!Read(text,vt)||vt!=0xccfd3c||!Read(0xfd63e0,global)||!Read(global+0x74,dict)||!Read(text+0x1c,fontId)||!Read(dict+fontId*4u,fontPointer)||!Read(layout+0x24,layoutFont)||layoutFont!=fontPointer||!Read(fontPointer,vt)||vt!=0xccfb6c||!Read(fontPointer+10,fontCount)||fontCount>256||!Read(layout+0x804c,count)||count>2048||!Read(layout+0x8be8,drawable)||drawable>count||!Read(text+0x2c,sx)||!Read(text+0x1e,sy))return;
        std::vector<native_ui_text::LayoutRecord> records(count);if(count&&!Copy(layout+0x4c,records.data(),count*sizeof(records[0])))return;
        unsigned lineCount{},alignment{};float left{},right{};if(!Read(layout+0x8450,lineCount)||!lineCount||lineCount>128||!Read(layout+0x1c,alignment)||!Read(layout,left)||!Read(layout+4,right))return;
        std::vector<native_ui_text::LayoutLine> lines(lineCount);if(!Copy(layout+0x8050,lines.data(),lineCount*sizeof(lines[0])))return;std::vector<native_ui_text::Matrix> reflow;if(FAILED(native_ui_text::Reflow(records,lines,left,right,sx/1024.f,sy/1024.f,alignment,reflow)))return;
        std::vector<std::uint8_t> bytes(52,0);bytes.reserve(56+count*28u);const auto append=[&](auto value){const auto* p=reinterpret_cast<const std::uint8_t*>(&value);bytes.insert(bytes.end(),p,p+sizeof value);};const auto op=[&](unsigned opcode){append(static_cast<std::uint8_t>(opcode));};unsigned drawn=0;
        for(unsigned i=0;i<count;++i){std::int32_t record[4]{};if(!Copy(layout+0x4c+i*16u,record,sizeof record))return;const auto cp=record[0];if(cp<=0||cp==9||cp==10||cp==13||cp==32)continue;if(record[2]<-32768||record[2]>32767||record[3]<-32768||record[3]>32767)return;
            unsigned glyphIndex=fontCount;for(unsigned k=0;k<fontCount;++k){unsigned nativeCp{};if(!Read(fontPointer+0x458+k*8u,nativeCp))return;if(nativeCp==static_cast<unsigned>(cp)){glyphIndex=k;break;}}if(glyphIndex==fontCount)return;
            op(8);append(std::uint8_t{0});append(fontId);append(std::uint32_t{1});op(16);append(std::uint8_t{0});append(sy);append(sx);op(1);append(std::uint8_t{0});append(static_cast<std::int16_t>(record[2]));op(2);append(std::uint8_t{0});append(static_cast<std::int16_t>(record[3]));op(255);append(static_cast<std::uint8_t>(glyphIndex));append(std::int16_t{0});++drawn;
        }if(drawn!=drawable)return;op(0);while(bytes.size()%4)op(0);
        const unsigned nativeText=0xccfd20,next=static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(bytes.data()+bytes.size()));const float one=1;std::memcpy(bytes.data(),&nativeText,4);std::memcpy(bytes.data()+4,&next,4);std::memcpy(bytes.data()+28,&one,4);std::memcpy(bytes.data()+32,&one,4);
        replacementTransforms=&reflow;Begin(reinterpret_cast<std::uintptr_t>(bytes.data()),context);replacementTransforms=nullptr;dynamicScope=true;
    }catch(...){replacementTransforms=nullptr;scope.valid=false;}
}
bool Geometry(unsigned cp) noexcept {
    if(scope.cache.find(cp)!=scope.cache.end())return true;if(scope.cache.size()>=512)return false;
    try{native_ui_text::Capture input;native_ui_text::Glyph glyph;glyph.codepoint=cp;input.glyphs.push_back(glyph);native_ui_text::Geometry geometry;native_ui_text::RenderContext context;context.rectangularClipProven=context.colorResolved=true;context.clip={-16384,-16384,16384,16384};if(FAILED(native_ui_text::Build(input,context,White,nullptr,geometry)))return false;scope.cache.emplace(cp,std::move(geometry));return true;}catch(...){return false;}
}
struct Budget {unsigned manager{},frame{},vertexCap{},indexCap{},usedVertices{},usedIndices{},jobs{},jobCap{},white{},batch{},flags{};};
bool BudgetNow(Budget& b) noexcept {
    unsigned frameIndex{},display{},whitePointer{};
    if(!Read(0xfd6304,b.manager)||!Read(b.manager+0x54,frameIndex)||frameIndex>1||!Read(b.manager+0x60,b.vertexCap)||!Read(b.manager+0x64,b.indexCap)||!Read(b.manager+8,b.jobCap)||!Read(scope.global+0x50,display)||!Read(display+0x40,b.batch)||!Read(b.batch+0x28,b.flags)||!Read(0xff7218,whitePointer)||!Read(whitePointer,b.white)||!b.white)return false;
    b.frame=b.manager+frameIndex*0x1c;unsigned flags{};
    return Read(b.flags,flags)&&(flags&1u)&&Read(b.frame+0xc,b.usedVertices)&&Read(b.frame+0x14,b.usedIndices)&&Read(b.frame+0x20,b.jobs)&&b.usedVertices<=b.vertexCap&&b.usedIndices<=b.indexCap&&b.jobs<=b.jobCap;
}
bool Preflight(unsigned vertices,unsigned glyphs) noexcept {
    Budget b;if(!BudgetNow(b)||vertices>65535||glyphs>4096)return false;
    // Capacities must have been raised before BOTH CPU and GPU allocation by
    // ui_geometry_budget. Never infer allocated size from post-allocation edits.
    constexpr unsigned requiredV=32u*1024u*1024u,requiredI=8u*1024u*1024u,requiredJobs=8192;
    constexpr unsigned reserveV=24u*1024u*1024u,reserveI=6u*1024u*1024u,reserveJobs=4096;
    if(b.vertexCap<requiredV||b.indexCap<requiredI||b.jobCap<requiredJobs)return false;
    // Compare against actual cumulative frame use, so successive text scopes
    // cannot each claim a fresh reserve. Parent native UI still has this room.
    return vertices*12u<=b.vertexCap-b.usedVertices&&reserveV<=b.vertexCap-b.usedVertices-vertices*12u&&vertices*2u<=b.indexCap-b.usedIndices&&reserveI<=b.indexCap-b.usedIndices-vertices*2u&&glyphs*2u+2u<=b.jobCap-b.jobs&&reserveJobs<=b.jobCap-b.jobs-glyphs*2u-2u;
}
__declspec(noinline)unsigned Queue(unsigned batch) noexcept {
    unsigned result{};__asm{push batch
        mov eax,054be90h
        call eax
        mov result,eax
    }return result;
}
__declspec(noinline)unsigned Start(unsigned batch) noexcept {
    unsigned result{};__asm{mov eax,batch
        mov edx,3
        mov ecx,04171f0h
        call ecx
        mov result,eax
    }return result;
}
__declspec(noinline)void Vertex(unsigned global,const float* xyuv) noexcept {__asm{mov ecx,global
    mov eax,xyuv
    mov edx,054ba70h
    call edx
}}
bool __cdecl NativeEmit(const DrawRequest* r) noexcept {
    if(!r||r->size!=sizeof(*r)||r->vertexCount%3||r->vertexCount>65535)return false;if(!r->vertexCount)return true;
    Budget b;unsigned savedTexture{},topology{};if(!BudgetNow(b)||b.batch!=r->nativeBatch||!Read(b.batch+0x44,savedTexture)||!Read(b.batch+0x40,topology)||topology!=3)return false;
    // Entire text was preflighted before its first glyph. State remains on the
    // same UI thread and only this native triangle appender consumes the budget.
    if(r->vertexCount*12u>b.vertexCap-b.usedVertices||r->vertexCount*2u>b.indexCap-b.usedIndices||b.jobCap-b.jobs<2)return false;
    if(Queue(b.batch))return false;
    *reinterpret_cast<unsigned*>(b.batch+0x44)=b.white;
    if(Start(b.batch)){*reinterpret_cast<unsigned*>(b.batch+0x44)=savedTexture;Start(b.batch);return false;}
    for(unsigned i=0;i<r->vertexCount;++i){const float xyuv[]={r->vertices[i].x,r->vertices[i].y,0,0};Vertex(static_cast<unsigned>(r->nativeGlobal),xyuv);}
    // Native Queue returns failure only when job slots are exhausted. The full
    // definition preflight reserved two slots per glyph before any emission.
    Queue(b.batch);*reinterpret_cast<unsigned*>(b.batch+0x44)=savedTexture;Start(b.batch);return true;
}
bool __cdecl Try(std::uintptr_t quad,std::uintptr_t texture,std::uintptr_t matrix) noexcept {
    if(scope.depth!=1||!scope.valid||!emitter)return false;
    try{
        const Identity* identity=nullptr;if(nativeMode){if(scope.cursor>=scope.glyphs.size())return false;const auto& glyph=scope.glyphs[scope.cursor];if(glyph.quad!=quad||glyph.texture!=texture)return false;identity=&glyph;}else for(const auto& glyph:scope.glyphs)if(glyph.quad==quad&&glyph.texture==texture){identity=&glyph;break;}if(!identity)return false;
        unsigned global{},mode{},mask{},display{},batch{},color{};float m[6]{},viewport[4]{};
        if(!Read(0xfd63e0,global)||global!=scope.global||!Read(global+0x28,mode)||mode==2||!Read(global+0x1bc,mask)||(!nativeMode&&mask)||!Read(global+0x50,display)||!Read(display+0x40,batch)||!Read(batch+0xfc,color)||!Copy(matrix,m,sizeof m)||!Copy(global+0x54,viewport,sizeof viewport))return false;
        for(auto v:m)if(!std::isfinite(v))return false;for(auto v:viewport)if(!std::isfinite(v))return false;if(!nativeMode&&(m[2]!=0||m[3]!=0))return false;
        if(nativeMode){const float expected[]={identity->expected.xx,identity->expected.yy,identity->expected.yx,identity->expected.xy,identity->expected.tx,identity->expected.ty};for(unsigned i=0;i<6;++i)if(std::abs(m[i]-expected[i])>(i<4?.00001f:.01f)){scope.valid=false;InterlockedIncrement(counters+5);return false;}}
        if(!Geometry(identity->codepoint))return false;const auto found=scope.cache.find(identity->codepoint);
        auto& transformed=scope.scratch;transformed.clear();if(nativeMode){for(unsigned i=0;i<identity->count;++i){auto v=scope.prepared[identity->start+i];v.x=v.x*viewport[0]+viewport[2];v.y=v.y*viewport[1]+viewport[3];v.color=color;if(!std::isfinite(v.x)||!std::isfinite(v.y))return false;transformed.push_back(v);}}
        else{transformed.reserve(found->second.triangles.size());for(auto v:found->second.triangles){v.x=(v.x*m[0]+m[4])*viewport[0]+viewport[2];v.y=(v.y*m[1]+m[5])*viewport[1]+viewport[3];v.color=color;if(!std::isfinite(v.x)||!std::isfinite(v.y))return false;transformed.push_back(v);}}
        DrawRequest request;request.vertices=transformed.data();request.vertexCount=static_cast<unsigned>(transformed.size());request.codepoint=identity->codepoint;request.finalArgb=color;request.nativeTexture=texture;request.nativeGlobal=global;request.nativeBatch=batch;const bool success=emitter(&request);if(success){if(nativeMode)++scope.cursor;InterlockedIncrement(counters+2);InterlockedExchangeAdd(counters+3,request.vertexCount);}else InterlockedIncrement(counters+6);return success;
    }catch(...){return false;}
}
__declspec(naked)void TextHook(){__asm{
    pushfd
    pushad
    mov esi,esp
    sub esp,528
    and esp,0fffffff0h
    fxsave [esp]
    call PaintOwned
    test al,al
    jz paintText
    lock inc dword ptr [paintCounters]
    fxrstor [esp]
    mov esp,esi
    popad
    popfd
    ret 12
paintText:
    push dword ptr [esi+40]
    push dword ptr [esi+24]
    call Begin
    add esp,8
    fxrstor [esp]
    mov esp,esi
    popad
    popfd
    push dword ptr [esp+12]
    push dword ptr [esp+12]
    push dword ptr [esp+12]
    call dword ptr [textOriginal]
    pushfd
    pushad
    mov esi,esp
    sub esp,528
    and esp,0fffffff0h
    fxsave [esp]
    call End
    fxrstor [esp]
    mov esp,esi
    popad
    popfd
    ret 12
}}
__declspec(naked)void GlyphHook(){__asm{
    pushfd
    pushad
    mov esi,esp
    sub esp,528
    and esp,0fffffff0h
    fxsave [esp]
    push dword ptr [esi+40]
    push dword ptr [esi+24]
    push dword ptr [esi+28]
    call Try
    add esp,12
    test al,al
    jz fallback
    fxrstor [esp]
    mov esp,esi
    popad
    popfd
    ret 4
fallback:
    fxrstor [esp]
    mov esp,esi
    popad
    popfd
    jmp dword ptr [glyphOriginal]
}}
__declspec(naked)void MaskedHook(){__asm{
    pushfd
    pushad
    mov esi,esp
    sub esp,528
    and esp,0fffffff0h
    fxsave [esp]
    mov eax,dword ptr [esi]
    add eax,014h
    push eax
    push dword ptr [esi+40]
    mov eax,dword ptr [esi+28]
    add eax,16
    push eax
    call Try
    add esp,12
    test al,al
    jz fallback
    fxrstor [esp]
    mov esp,esi
    popad
    popfd
    ret 4
fallback:
    fxrstor [esp]
    mov esp,esi
    popad
    popfd
    jmp dword ptr [maskedOriginal]
}}
__declspec(naked)void DynamicHook(){__asm{
    pushfd
    pushad
    mov esi,esp
    sub esp,528
    and esp,0fffffff0h
    fxsave [esp]
    call PaintOwned
    test al,al
    jz paintDynamic
    lock inc dword ptr [paintCounters+4]
    fxrstor [esp]
    mov esp,esi
    popad
    popfd
    ret 12
paintDynamic:
    push dword ptr [esi+40]
    push dword ptr [esi+24]
    call DynamicBegin
    add esp,8
    fxrstor [esp]
    mov esp,esi
    popad
    popfd
    push dword ptr [esp+12]
    push dword ptr [esp+12]
    push dword ptr [esp+12]
    call dword ptr [dynamicOriginal]
    pushfd
    pushad
    mov esi,esp
    sub esp,528
    and esp,0fffffff0h
    fxsave [esp]
    call DynamicEnd
    fxrstor [esp]
    mov esp,esi
    popad
    popfd
    ret 12
}}
__declspec(naked)void LayoutHook(){__asm{
    call dword ptr [layoutOriginal]
    pushfd
    pushad
    mov esi,esp
    sub esp,528
    and esp,0fffffff0h
    fxsave [esp]
    push dword ptr [esi+08cb0h]
    push dword ptr [esi]
    push dword ptr [esi+4]
    call CaptureLayout
    add esp,12
    fxrstor [esp]
    mov esp,esi
    popad
    popfd
    ret
}}
// Preserve native cursor/button updates, then hide only the old cursor node
// when a fresh replacement owns it. No ShowCursor count or global input changes.
__declspec(naked)void PointerHook(){__asm{
    call dword ptr [pointerOriginal]
    pushfd
    pushad
    mov esi,esp
    sub esp,528
    and esp,0fffffff0h
    fxsave [esp]
    call UpdatePointerOwner
    fxrstor [esp]
    mov esp,esi
    popad
    popfd
    ret
}}
__declspec(naked)void ShapeHook(){__asm{
    pushfd
    pushad
    mov esi,esp
    sub esp,528
    and esp,0fffffff0h
    fxsave [esp]
    // Native925410 passes (placement+100, contour, actual sprite instance).
    // Remap the whole authenticated subtree, INCLUDING its visual masks.
    // TransformOwnedShape explicitly leaves non-paint queries untouched.
    push dword ptr [esi+48]
    push dword ptr [esi+40]
    push dword ptr [esi+24]
    call TransformOwnedShape
    add esp,12
    mov dword ptr [esi+40],eax
    // Original second stack argument requests contour/mask-only processing.
    // It is independent of global+2C and must also keep the original function.
    cmp byte ptr [esi+44],0
    jne nativeShape
    push dword ptr [esi+48]
    push dword ptr [esi+24]
    call PaintOwnedShape
    add esp,8
    test al,al
    jz nativeShape
    lock inc dword ptr [paintCounters+8]
    fxrstor [esp]
    mov esp,esi
    popad
    popfd
    ret 12
nativeShape:
    fxrstor [esp]
    mov esp,esi
    popad
    popfd
    jmp dword ptr [shapeOriginal]
}}
}
void SetPaintOwner(PaintOwner owner) noexcept{paintOwner=owner;}
void SetContentPaint(ContentPaint callback) noexcept{contentPaint=callback;}
void SetContentInstancePaint(ContentInstancePaint callback) noexcept{contentInstancePaint=callback;}
void SetContentTransform(ContentTransform callback) noexcept{contentTransform=callback;}
void SetPointerOwner(PointerOwner owner) noexcept{pointerOwner=owner;}
void* ShapeEntry() noexcept{return reinterpret_cast<void*>(ShapeHook);}
PaintStatistics GetPaintStatistics() noexcept{return {static_cast<unsigned>(InterlockedCompareExchange(paintCounters,0,0)),static_cast<unsigned>(InterlockedCompareExchange(paintCounters+1,0,0)),static_cast<unsigned>(InterlockedCompareExchange(paintCounters+2,0,0))};}
bool Configure(void* text,void* glyph,Emit emit,bool preparedNativeContext) noexcept{if(!text||!glyph||!emit)return false;textOriginal=text;glyphOriginal=glyph;emitter=emit;nativeMode=preparedNativeContext;return true;}
void Disable() noexcept{emitter=nullptr;paintOwner=nullptr;contentPaint=nullptr;contentInstancePaint=nullptr;contentTransform=nullptr;pointerOwner=nullptr;}
void* TextEntry() noexcept{return reinterpret_cast<void*>(TextHook);}
void* GlyphEntry() noexcept{return reinterpret_cast<void*>(GlyphHook);}
void* MaskedEntry() noexcept{return reinterpret_cast<void*>(MaskedHook);}
void* DynamicEntry() noexcept{return reinterpret_cast<void*>(DynamicHook);}
void* LayoutEntry() noexcept{return reinterpret_cast<void*>(LayoutHook);}
void ConfigureDynamic(void* dynamicTrampoline) noexcept{dynamicOriginal=dynamicTrampoline;}
void CaptureNativeLayout(std::uintptr_t layout,std::uintptr_t text,std::uintptr_t context) noexcept{CaptureLayout(layout,text,context);}
Statistics GetStatistics() noexcept{return {static_cast<unsigned>(InterlockedCompareExchange(counters,0,0)),static_cast<unsigned>(InterlockedCompareExchange(counters+1,0,0)),static_cast<unsigned>(InterlockedCompareExchange(counters+2,0,0)),static_cast<unsigned>(InterlockedCompareExchange(counters+3,0,0)),static_cast<unsigned>(InterlockedCompareExchange(counters+4,0,0)),static_cast<unsigned>(InterlockedCompareExchange(counters+5,0,0)),static_cast<unsigned>(InterlockedCompareExchange(counters+6,0,0))};}
bool Install(bool authenticatedR29) noexcept {
    if(!authenticatedR29)return false;if(installed)return true;
    const unsigned char textBytes[]={0x55,0x8b,0xec,0x83,0xe4,0xf8},glyphBytes[]={0x8b,0x54,0x24,0x04,0x83,0xec,0x2c},maskedBytes[]={0x81,0xec,0x94,0,0,0},layoutBytes[]={0xe8,0x76,0x41,0,0};
    struct Site {unsigned address,length;const unsigned char* signature;void* target;bool call;};
    Site sites[]={{0x928410,6,textBytes,TextEntry(),false},{0x92b170,7,glyphBytes,GlyphEntry(),false},{0x92b3f0,6,maskedBytes,MaskedEntry(),false},{0x91fb10,6,textBytes,DynamicEntry(),false},{0x92a200,6,textBytes,ShapeEntry(),false},{0x929a35,5,layoutBytes,LayoutEntry(),true},{0x54b500,6,textBytes,reinterpret_cast<void*>(PointerHook),false}};
    unsigned char actual[7]{};for(const auto& site:sites)if(!Copy(site.address,actual,site.length)||std::memcmp(actual,site.signature,site.length))return false;
    auto* code=static_cast<unsigned char*>(VirtualAlloc(nullptr,96,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));if(!code)return false;
    for(unsigned i=0;i<6;++i){const auto& site=sites[i==5?6:i];auto* target=code+i*16u;std::memcpy(target,site.signature,site.length);target[site.length]=0xe9;*reinterpret_cast<unsigned*>(target+site.length+1)=site.address+site.length-static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(target+site.length+5));}
    DWORD old{};if(!VirtualProtect(code,96,PAGE_EXECUTE_READ,&old)){VirtualFree(code,0,MEM_RELEASE);return false;}
    const unsigned pages[]={0x928000,0x92b000,0x91f000,0x929000,0x92a000,0x54b000};DWORD protections[6]{};unsigned protectedPages=0;
    for(;protectedPages<6;++protectedPages)if(!VirtualProtect(reinterpret_cast<void*>(pages[protectedPages]),4096,PAGE_EXECUTE_READWRITE,&protections[protectedPages]))break;
    if(protectedPages!=6){for(unsigned i=0;i<protectedPages;++i)VirtualProtect(reinterpret_cast<void*>(pages[i]),4096,protections[i],&old);VirtualFree(code,0,MEM_RELEASE);return false;}
    Configure(code,code+16,NativeEmit,true);maskedOriginal=code+32;dynamicOriginal=code+48;shapeOriginal=code+64;pointerOriginal=code+80;
    for(const auto& site:sites){unsigned char patch[7]={0xe9,0,0,0,0,0x90,0x90};if(site.call)patch[0]=0xe8;*reinterpret_cast<unsigned*>(patch+1)=static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(site.target))-site.address-5;std::memcpy(reinterpret_cast<void*>(site.address),patch,site.length);}
    for(unsigned i=0;i<6;++i)VirtualProtect(reinterpret_cast<void*>(pages[i]),4096,protections[i],&old);FlushInstructionCache(GetCurrentProcess(),nullptr,0);installed=true;return true;
}
}
