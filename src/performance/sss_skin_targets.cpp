#define NOMINMAX
#include "sss_skin_targets.h"
#include "sss_native_recipes.h"
#include "sss_edward_mask.h"
#include "skin_material_catalog.h"
#include <wrl/client.h>
#include <array>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <new>
namespace sss_skin_targets {
using Microsoft::WRL::ComPtr;
namespace {
struct PSInfo {std::uint64_t hash;UINT bytes,semantic,vsMask;bool kills;};
struct VSInfo {std::uint64_t hash;UINT bytes,worldView;};
const VSInfo VertexPrograms[]={
    {0x0c744ab5cdce9221ull,1884,224},
    {0x1339f96b7a362ba6ull,2068,220},
    {0x33df51721d4ac731ull,2176,224},
    {0x44490a1699fac49full,2104,220},
    {0x47696daaea9ed054ull,2284,224},
    {0x779d8dcfe07668b2ull,1776,220},
    {0xac3b6860cf7b100cull,1992,224},
    {0xc9559181a2efd437ull,1812,220},
};
const PSInfo PixelPrograms[]={
    {0xa11f3e912dcd0943ull,2396,1,0x27,false},
    {0x370412adf1ff71b4ull,1848,2,0xd8,false},
    {0xf9ce7c813c329deaull,2164,1,0x27,true},
    {0x7c684d57b0292e33ull,2540,1,0x27,true},
    {0x099bfb9034524603ull,2000,2,0xd8,true},
    {0x79601c494ff20022ull,2100,1,0x27,false},
    {0xae1abfa4ac48564cull,1752,1,0x27,false},
    {0x0f7f5da0d11e605dull,1396,1,0x27,true},
    {0x750ff93fb2ca5d57ull,1564,2,0xd8,false},
    {0xfc61b2317d4e5443ull,2552,1,0x27,true},
    {0x317dda492f941823ull,1396,1,0x27,true},
    {0xd2b05c048e44f673ull,2048,1,0x27,true},
    {0xb37d896d449ce578ull,1620,1,0x27,true},
    {0xf656f4270b31b036ull,1628,2,0xd8,true},
    {0x6242eac5bcf44a81ull,2372,1,0x27,false},
    {0x6e173162b92aad17ull,2616,1,0x27,false},
    {0x3896d23be231d366ull,1332,1,0x27,false},
    {0xf43ba5fbd3a67619ull,2532,2,0xd8,true},
    {0x4dfdf70f78bc4398ull,2144,2,0xd8,true},
    {0x3e779fc25d7c8298ull,2188,1,0x27,false},
    {0x26e8d256431dd843ull,1716,2,0xd8,true},
    {0x578d084a965cdbd5ull,2284,2,0xd8,false},
    {0xd2bceb554572b474ull,2648,2,0xd8,true},
    {0x67cc41cbf76a23caull,2144,2,0xd8,false},
    {0xc34145e6543afa2eull,2056,2,0xd8,false},
    {0xec4752bf987fc41full,2776,2,0xd8,true},
    {0x35f25e0c271a5217ull,1912,2,0xd8,true},
    {0xd84367aa5f3cf170ull,1428,2,0xd8,false},
    {0xfafa6b3bc4628624ull,2512,1,0x27,false},
    {0xc29ab2035e4b93a2ull,2492,2,0xd8,false},
    {0xc3656792ca995c92ull,2260,2,0xd8,true},
    {0x409e51d62ad20da0ull,2572,2,0xd8,false},
    {0x2153799f920d70e9ull,2196,2,0xd8,false},
    {0xe4fb5b3e8542c50aull,1940,2,0xd8,false},
    {0xa26e82da9def3c54ull,2048,1,0x27,false},
    {0x24fbb44d6df6ba34ull,2576,1,0x27,true},
    {0x0619be914a7b49b8ull,1844,1,0x27,false},
    {0xf1eb9c1b07f9c563ull,2120,2,0xd8,true},
    {0xc044583db008dac1ull,1924,2,0xd8,true},
    {0x97e2dfc1d042b1e6ull,2208,2,0xd8,true},
    {0x02282b5e7b1c718bull,1652,2,0xd8,false},
    {0xd6a1afbfed231e01ull,2712,2,0xd8,false},
    {0x788fb61d511496e2ull,2092,2,0xd8,false},
    {0xb34e4b61eaabf921ull,1428,2,0xd8,false},
    {0x3c95bfdb0a06ebdaull,1504,2,0xd8,false},
    {0x6d039e989b109825ull,2220,2,0xd8,true},
    {0x99bfd97e482048bdull,1840,1,0x27,false},
    {0x6c6fff1f7d3bf750ull,2024,1,0x27,true},
    {0xf16dc114836e49a3ull,1956,1,0x27,true},
    {0x6c5717e456b85002ull,1984,1,0x27,false},
    {0x80acd71e5b75dabfull,1960,1,0x27,false},
    {0xcc7c47bb004719f2ull,2156,2,0xd8,true},
    {0x629626346cfe3655ull,1828,1,0x27,true},
    {0x0005bf047d440d67ull,2488,1,0x27,false},
    {0x1fbd38d6f02f398dull,1860,2,0xd8,false},
    {0x56084e0726b91da0ull,1908,1,0x27,true},
    {0x3a2f774a5310096dull,1936,2,0xd8,false},
    {0xdcf49949fdd6cb12ull,1408,1,0x27,false},
    {0xf91583384be0db58ull,2468,2,0xd8,false},
    {0xa691c0595a938257ull,1332,1,0x27,false},
    {0x73e4e3524a0c87dbull,1996,1,0x27,false},
    {0x3284d4b79e3eeeabull,2584,2,0xd8,false},
    {0x08464aa00e8fc528ull,2636,2,0xd8,true},
    {0xa4c88dceee8d265aull,2052,2,0xd8,true},
    {0x378ab90da426971aull,2004,2,0xd8,true},
    {0x4bbb2d14e58fa01eull,1532,1,0x27,true},
    {0x720876321b02ebfeull,2460,1,0x27,true},
    {0xa27cd43529ac7d45ull,2060,1,0x27,false},
    {0x095f193cd8b985e2ull,2348,2,0xd8,true},
    {0xd2221255a923e543ull,2608,2,0xd8,false},
    {0xad15d2c8dd4a7625ull,1988,2,0xd8,false},
    {0x738eb600514e5c52ull,2436,1,0x27,true},
    {0xf6959d9dfdfa07f3ull,2080,2,0xd8,false},
    {0x365f8024a3b8a481ull,1468,1,0x27,false},
    {0x6ab91fcaebfccc6full,2112,1,0x27,true},
    {0x11dbe97771df9ec4ull,1892,1,0x27,false},
    {0x337fd9513489ea53ull,2156,2,0xd8,false},
    {0x72036928466b63a6ull,1472,1,0x27,true},
    {0x4051b14828f2f867ull,1764,1,0x27,false},
    {0x74b7e5bc0f8ec2daull,2680,1,0x27,true},
    {0xae53534848441f35ull,1568,2,0xd8,true},
    {0x0296a521dc619bd1ull,2060,1,0x27,true},
    {0x94e45edb99826783ull,1904,1,0x27,true},
    {0x289fa37287fd445cull,1556,1,0x27,false},
    {0xb53d16397112198eull,1492,2,0xd8,true},
    {0x9b50df0a79820f1cull,2476,1,0x27,false},
    {0xf13ffef9609e3604ull,2124,1,0x27,true},
    {0xbc0999c6a9d88ffdull,2556,2,0xd8,true},
    {0x64ad399626a10c44ull,1492,2,0xd8,true},
    {0x30a31c86a41e3c0cull,2252,1,0x27,true},
    {0xdf69e929dff1c351ull,2672,2,0xd8,true},
    {0x100e7890793e4bc9ull,1816,1,0x27,true},
};
std::uint64_t Hash(const void* data,UINT bytes) noexcept {
    auto p=static_cast<const unsigned char*>(data);std::uint64_t h=14695981039346656037ull;
    for(UINT n=0;n<bytes;++n){h^=p[n];h*=1099511628211ull;}return h;
}
template<class Shader> bool Code(Shader* shader,std::vector<DWORD>& code) {
    UINT size{};if(!shader||FAILED(shader->GetFunction(nullptr,&size))||size<8||size>65536||size%4)return false;
    code.resize(size/4);return SUCCEEDED(shader->GetFunction(code.data(),&size))&&size==code.size()*4;
}
bool Close(float a,float b) noexcept {return std::abs(a-b)<=1e-4f*std::max(1.0f,std::max(std::abs(a),std::abs(b)));}
double Dot(const float* a,const float* b) noexcept {return double(a[0])*b[0]+double(a[1])*b[1]+double(a[2])*b[2];}
bool RowScale(const float* out,const float* axis,const float* z,double& scale) noexcept {
    const double aa=Dot(axis,axis),az=Dot(axis,z),zz=Dot(z,z),det=aa*zz-az*az;
    if(aa<1e-14||zz<1e-14||det<=1e-10*aa*zz)return false;
    const double oa=Dot(out,axis),oz=Dot(out,z);scale=(oa*zz-oz*az)/det;
    const double offset=(oz*aa-oa*az)/det;double err=0;
    for(UINT i=0;i<3;++i){const double d=out[i]-scale*axis[i]-offset*z[i];err+=d*d;}
    return err<=1e-8*std::max(1e-12,Dot(out,out));
}
// For exact allowlisted branch-free native programs only. All 92 were proved
// to leave r31/c223 unused. Preserve every instruction and authored texkill;
// redirect output writes to the spare temp, then emit only the proven view-Z
// mask. No native material, lighting or texture classification is invented.
bool BuildMask(const std::vector<DWORD>& original,std::vector<DWORD>& clone) {
    if(original.size()<2||original.front()!=0xffff0300u||original.back()!=0xffffu)return false;
    clone=original;DWORD depth{};bool occupied[32]{};
    for(size_t at=1;at<original.size();){const DWORD token=original[at];const UINT op=token&65535u;if(op==65535u)break;const UINT n=op==65534u?((token>>16)&32767u):((token>>24)&15u);
        if(n>=original.size()-at)return false;
        if(op!=65534u)for(UINT i=1;i<=n;++i){if((op==31&&i==1)||((op==81||op==48||op==47)&&i>1))continue;const DWORD t=original[at+i];if((((t>>28)&7u)|((t>>8)&24u))==0&&(t&2047u)<32)occupied[t&2047u]=true;}at+=n+1;}
    UINT capture=30;while(occupied[capture]){if(!capture)return false;--capture;}
    for(size_t at=1;at<clone.size();){
        const DWORD token=clone[at];const UINT op=token&0xffffu;if(op==0xffffu){if(at+1!=clone.size())return false;break;}
        const UINT length=op==0xfffeu?((token>>16)&0x7fffu):((token>>24)&15u);if(length>=clone.size()-at)return false;
        if(op!=0xfffeu&&length){const DWORD dst=clone[at+1];const UINT type=((dst>>28)&7u)|((dst>>8)&24u);
            if(type==8){
                if((dst&0x7ffu)==3&&(dst&0x10000u)){
                    if(depth||(op!=5&&op!=4))return false;
                    // Preserve the exact original X expression at its original
                    // execution point, including native .1*bump+viewZ MAD.
                    clone[at+1]=(dst&~0x700f1fffu)|0x00080000u|capture;
                    for(UINT i=2;i<=length;++i){const DWORD src=clone[at+i];const UINT x=(src>>16)&3u;clone[at+i]=(src&~0x00ff0000u)|((x*0x55u)<<16);}
                    depth=0x80ff0000u|capture;
                }else clone[at+1]=(dst&~0x70001fffu)|31u;
            }
        }
        at+=length+1;
    }
    if(!depth)return false;clone.pop_back();const DWORD tail[]={
        0x02000001u,0x80030800u,0xa0e400dfu,
        0x02000001u,0x80040800u,depth,
        0x02000001u,0x80080800u,0xa0aa00dfu,0xffffu};
    clone.insert(clone.end(),std::begin(tail),std::end(tail));return true;
}
struct Saved {
    IDirect3DDevice9* d;ComPtr<IDirect3DStateBlock9> block;
    ComPtr<IDirect3DSurface9> rt[4],ds;ComPtr<IDirect3DVertexBuffer9> vb;
    ComPtr<IDirect3DIndexBuffer9> ib;D3DVIEWPORT9 vp{};UINT offset{},stride{},frequency{};bool valid=false;
    explicit Saved(IDirect3DDevice9* dev):d(dev){
        if(FAILED(d->CreateStateBlock(D3DSBT_ALL,&block))||FAILED(block->Capture()))return;
        for(UINT i=0;i<4;++i){const auto hr=d->GetRenderTarget(i,&rt[i]);if(FAILED(hr)&&!(i&&hr==D3DERR_NOTFOUND))return;}
        if(FAILED(d->GetDepthStencilSurface(&ds))||FAILED(d->GetViewport(&vp))||
           FAILED(d->GetStreamSource(0,&vb,&offset,&stride))||FAILED(d->GetStreamSourceFreq(0,&frequency))||FAILED(d->GetIndices(&ib)))return;
        valid=true;
    }
    ~Saved(){if(valid){
        // RT changes can reset viewport. State blocks omit RT/DS and UP can
        // clear stream/index bindings, so restore these explicitly as well.
        for(UINT i=1;i<4;++i)d->SetRenderTarget(i,nullptr);
        d->SetRenderTarget(0,rt[0].Get());for(UINT i=1;i<4;++i)d->SetRenderTarget(i,rt[i].Get());
        d->SetDepthStencilSurface(ds.Get());block->Apply();d->SetViewport(&vp);
        d->SetStreamSource(0,vb.Get(),offset,stride);d->SetStreamSourceFreq(0,frequency);d->SetIndices(ib.Get());
    }}
};
HRESULT Issue(IDirect3DDevice9* d,const Draw& p) noexcept {
    switch(p.kind){
    case 81:return d->DrawPrimitive(p.type,p.startVertex,p.primitiveCount);
    case 82:return d->DrawIndexedPrimitive(p.type,p.baseVertexIndex,p.minVertexIndex,p.numVertices,p.startIndex,p.primitiveCount);
    case 83:return p.vertexData&&p.stride?d->DrawPrimitiveUP(p.type,p.primitiveCount,p.vertexData,p.stride):E_INVALIDARG;
    case 84:return p.vertexData&&p.indexData&&p.stride&&(p.indexFormat==D3DFMT_INDEX16||p.indexFormat==D3DFMT_INDEX32)?
        d->DrawIndexedPrimitiveUP(p.type,p.minVertexIndex,p.numVertices,p.primitiveCount,p.indexData,p.indexFormat,p.vertexData,p.stride):E_INVALIDARG;
    default:return E_INVALIDARG;
    }
}
}
bool DeriveProjection(const float wvp[16],const float wv[12],float& x,float& y,float& sign) noexcept {
    x=y=sign=0;if(!wvp||!wv)return false;
    for(UINT i=0;i<16;++i)if(!std::isfinite(wvp[i]))return false;
    for(UINT i=0;i<12;++i)if(!std::isfinite(wv[i]))return false;
    const double zz=Dot(wv+8,wv+8);if(zz<1e-14)return false;
    const double k=Dot(wvp+12,wv+8)/zz;if(!std::isfinite(k)||std::abs(k)<1e-8)return false;
    double residual=0;for(UINT i=0;i<3;++i){const double e=wvp[12+i]-k*wv[8+i];residual+=e*e;}
    // Clip W must be proportional to view Z, including homogeneous translation.
    if(residual>1e-8*std::max(1e-12,Dot(wvp+12,wvp+12))||std::abs(wvp[15]-k*wv[11])>1e-4*std::max(1.0,std::abs(k*wv[11])))return false;
    double a{},c{};if(!RowScale(wvp,wv,wv+8,a)||!RowScale(wvp+4,wv+4,wv+8,c))return false;
    const double sx=std::abs(a/k),sy=std::abs(c/k);
    if(!std::isfinite(sx)||!std::isfinite(sy)||sx<0.05||sx>100||sy<0.05||sy>100)return false;
    x=static_cast<float>(sx);y=static_cast<float>(sy);sign=k>0?1.0f:-1.0f;return true;
}
struct Targets::Impl {
    struct Entry {ComPtr<IDirect3DPixelShader9> original,replay,maskedReplay;PSInfo meta{};ComPtr<IDirect3DPixelShader9> maskOnly,maskedMaskReplay;sss_native_recipes::ReplayInfo info{};bool forward=false;UINT colorSampler=sss_native_recipes::MissingRegister;};
    struct Atlas {ComPtr<IDirect3DTexture9> texture;const skin_material_catalog::Profile* profile=nullptr;};
    ComPtr<IDirect3DDevice9> device;ComPtr<IDirect3DTexture9> mask,forward,depth;
    ComPtr<IDirect3DSurface9> maskSurface,forwardSurface,nativeRT[4],nativeDS;
    std::vector<Entry> cache;
    struct VertexEntry {ComPtr<IDirect3DVertexShader9> shader;std::uint64_t hash;UINT bytes;bool reviewed;};std::vector<VertexEntry> vertexCache;
    bool VertexKey(IDirect3DVertexShader9* shader,std::uint64_t& hash,UINT& bytes){
        for(size_t n=0;n<vertexCache.size();++n)if(vertexCache[n].shader.Get()==shader){hash=vertexCache[n].hash;bytes=vertexCache[n].bytes;const bool reviewed=vertexCache[n].reviewed;if(n+1<vertexCache.size()){auto hot=std::move(vertexCache[n]);vertexCache.erase(vertexCache.begin()+n);vertexCache.push_back(std::move(hot));}return reviewed;}
        std::vector<DWORD> code;if(!Code(shader,code))return false;bytes=static_cast<UINT>(code.size()*4);hash=Hash(code.data(),bytes);if(vertexCache.size()>=128)vertexCache.erase(vertexCache.begin());sss_native_recipes::ViewInfo view{};const bool reviewed=sss_native_recipes::KnownSkinVertex(hash,bytes,view);vertexCache.push_back({shader,hash,bytes,reviewed});return reviewed;
    }
    std::vector<Atlas> atlasCache;ComPtr<IDirect3DTexture9> authoredMask;
    struct OwnMaskEntry {const skin_material_catalog::Profile* profile;ComPtr<IDirect3DTexture9> texture;};std::vector<OwnMaskEntry> ownMasks;
    std::uint64_t serial{};UINT width{},height{};float sx{},sy{},sign{};
    bool enabled=false,written=false,forwardWritten=false,maskCleared=false,forwardCleared=false,failed=false;
    Status status=Status::Disabled;
    Entry* Find(IDirect3DPixelShader9* ps){
        for(size_t n=0;n<cache.size();++n)if(cache[n].original.Get()==ps){if(n+1<cache.size()){Entry hot=std::move(cache[n]);cache.erase(cache.begin()+n);cache.push_back(std::move(hot));}return &cache.back();}
        std::vector<DWORD> code;if(!Code(ps,code))return nullptr;
        const UINT bytes=static_cast<UINT>(code.size()*4);const auto hash=Hash(code.data(),bytes);
        const auto m=std::find_if(std::begin(PixelPrograms),std::end(PixelPrograms),[&](const PSInfo& p){return p.hash==hash&&p.bytes==bytes;});
        sss_native_recipes::GeometryInfo geometry{};if(!sss_native_recipes::KnownSkinGeometry(hash,bytes,geometry))return nullptr;Entry e;e.original=ps;e.meta=m==std::end(PixelPrograms)?PSInfo{hash,bytes,geometry.depthSemantic,0,false}:*m;e.colorSampler=geometry.colorSampler;
        if(cache.size()>=256)cache.erase(cache.begin());cache.push_back(std::move(e));return &cache.back();
    }
    bool EnsureReplay(Entry& e){
        if(e.replay)return true;std::vector<DWORD> code,clone;if(!Code(e.original.Get(),code))return false;
        const auto hr=sss_native_recipes::BuildReplayShader(code.data(),e.meta.bytes,clone,e.info);
        if(hr==S_OK&&e.info.hasCharacterLighting){if(FAILED(device->CreatePixelShader(clone.data(),&e.replay)))return false;e.forward=true;if(!BuildMask(code,clone)||FAILED(device->CreatePixelShader(clone.data(),&e.maskOnly)))return false;}
        else if(!BuildMask(code,clone)||FAILED(device->CreatePixelShader(clone.data(),&e.replay)))return false;
        return true;
    }
    const skin_material_catalog::Profile* ApproveAtlas(IDirect3DDevice9* d,UINT sampler){
        if(sampler>=15)return nullptr;
        ComPtr<IDirect3DBaseTexture9> bound;if(FAILED(d->GetTexture(sampler,&bound))||!bound||bound->GetType()!=D3DRTYPE_TEXTURE)return nullptr;
        ComPtr<IDirect3DTexture9> texture;if(FAILED(bound.As(&texture)))return nullptr;
        for(size_t n=0;n<atlasCache.size();++n)if(atlasCache[n].texture.Get()==texture.Get()){const auto* profile=atlasCache[n].profile;if(n+1<atlasCache.size()){Atlas hot=std::move(atlasCache[n]);atlasCache.erase(atlasCache.begin()+n);atlasCache.push_back(std::move(hot));}return profile;}
        Atlas a;a.texture=texture;D3DSURFACE_DESC base{};
        if(SUCCEEDED(texture->GetLevelDesc(0,&base))&&base.Pool==D3DPOOL_MANAGED&&!base.Usage){
            const auto bytesFor=[](const D3DSURFACE_DESC& mip,UINT& row,UINT& rows){
                rows=(mip.Height+3)/4;
                if(mip.Format==D3DFMT_DXT1)row=((mip.Width+3)/4)*8;
                else if(mip.Format==D3DFMT_DXT3||mip.Format==D3DFMT_DXT5)row=((mip.Width+3)/4)*16;
                else if(mip.Format==D3DFMT_A8R8G8B8||mip.Format==D3DFMT_X8R8G8B8){row=mip.Width*4;rows=mip.Height;}
                else return false;return row&&rows&&std::uint64_t(row)*rows<=524288;
            };
            UINT chosen=~0u,row=0,rows=0;D3DSURFACE_DESC mip{};
            for(UINT level=0;level<texture->GetLevelCount();++level){D3DSURFACE_DESC desc{};UINT pitch=0,count=0;if(SUCCEEDED(texture->GetLevelDesc(level,&desc))&&desc.Width<=256&&desc.Height<=256&&bytesFor(desc,pitch,count)&&std::uint64_t(pitch)*count<=65536){chosen=level;mip=desc;row=pitch;rows=count;break;}}
            if(chosen==~0u&&bytesFor(base,row,rows)){chosen=0;mip=base;}
            if(chosen!=~0u){D3DLOCKED_RECT lock{};if(SUCCEEDED(texture->LockRect(chosen,&lock,nullptr,D3DLOCK_READONLY))){
                if(lock.pBits&&lock.Pitch>=static_cast<INT>(row)&&lock.Pitch<=2097152){std::uint64_t hash=14695981039346656037ull;for(UINT y=0;y<rows;++y){const auto* data=static_cast<const BYTE*>(lock.pBits)+y*lock.Pitch;for(UINT x=0;x<row;++x)hash=(hash^data[x])*1099511628211ull;}
                    const skin_material_catalog::Identity identity{base.Width,base.Height,static_cast<unsigned>(base.Format),static_cast<unsigned>(base.Pool),chosen,mip.Width,mip.Height,row*rows,hash};a.profile=skin_material_catalog::Match(identity);if(a.profile&&a.profile->index>254)a.profile=nullptr;}
                if(FAILED(texture->UnlockRect(chosen)))a.profile=nullptr;
            }}
        }if(atlasCache.size()>=32)atlasCache.erase(atlasCache.begin());atlasCache.push_back(a);return a.profile;
    }
    bool OwnMask(const skin_material_catalog::Profile* profile){
        if(!profile)return false;
        for(size_t i=0;i<ownMasks.size();++i)if(ownMasks[i].profile==profile){authoredMask=ownMasks[i].texture;if(i+1<ownMasks.size()){OwnMaskEntry hot=std::move(ownMasks[i]);ownMasks.erase(ownMasks.begin()+i);ownMasks.push_back(std::move(hot));}return true;}
        ComPtr<IDirect3DTexture9> texture;if(FAILED(device->CreateTexture(128,128,8,0,D3DFMT_L8,D3DPOOL_MANAGED,&texture,nullptr)))return false;
        std::vector<BYTE> pixels(128*128);if(!skin_material_catalog::BuildMask(profile,128,128,pixels.data(),pixels.size()))return false;
        UINT size=128;for(UINT mip=0;mip<8;++mip){D3DLOCKED_RECT lock{};if(FAILED(texture->LockRect(mip,&lock,nullptr,0)))return false;
            for(UINT y=0;y<size;++y)memcpy(static_cast<BYTE*>(lock.pBits)+y*lock.Pitch,pixels.data()+y*size,size);
            if(FAILED(texture->UnlockRect(mip)))return false;
            if(size>1){const UINT next=size/2;std::vector<BYTE> smaller(next*next);for(UINT y=0;y<next;++y)for(UINT x=0;x<next;++x)smaller[y*next+x]=std::min(std::min(pixels[y*2*size+x*2],pixels[y*2*size+x*2+1]),std::min(pixels[(y*2+1)*size+x*2],pixels[(y*2+1)*size+x*2+1]));pixels.swap(smaller);size=next;}
        }authoredMask=texture;if(ownMasks.size()>=32)ownMasks.erase(ownMasks.begin());ownMasks.push_back({profile,texture});return true;
    }
    bool Texture(ComPtr<IDirect3DTexture9>& t,ComPtr<IDirect3DSurface9>& s){
        ComPtr<IDirect3DTexture9> texture;ComPtr<IDirect3DSurface9> surface;
        if(FAILED(device->CreateTexture(width,height,1,D3DUSAGE_RENDERTARGET,D3DFMT_A16B16G16R16F,D3DPOOL_DEFAULT,&texture,nullptr))||FAILED(texture->GetSurfaceLevel(0,&surface)))return false;
        t=std::move(texture);s=std::move(surface);return true;
    }

};
Targets::Targets():impl_(std::make_unique<Impl>()){}
Targets::~Targets()=default;
void Targets::Release() noexcept {impl_->cache.clear();impl_->vertexCache.clear();impl_->atlasCache.clear();impl_->ownMasks.clear();impl_->authoredMask.Reset();impl_->maskSurface.Reset();impl_->forwardSurface.Reset();impl_->mask.Reset();impl_->forward.Reset();impl_->depth.Reset();for(auto& rt:impl_->nativeRT)rt.Reset();impl_->nativeDS.Reset();impl_->device.Reset();impl_->enabled=impl_->written=impl_->forwardWritten=false;impl_->width=impl_->height=0;impl_->status=Status::Disabled;}
void Targets::BeginFrame(IDirect3DDevice9* device,std::uint64_t serial,bool enabled) noexcept {
    auto& p=*impl_;if(!enabled||p.device.Get()!=device)Release();p.serial=serial;p.enabled=enabled&&device;
    if(p.enabled)p.device=device;p.written=p.forwardWritten=p.maskCleared=p.forwardCleared=p.failed=false;
    p.depth.Reset();for(auto& rt:p.nativeRT)rt.Reset();p.nativeDS.Reset();p.sx=p.sy=p.sign=0;
    p.status=p.enabled?Status::Unapproved:Status::Disabled;
}
bool Targets::ApproveKnownSkin(IDirect3DDevice9* device,BYTE* group) noexcept {
    if(group)*group=0;
    auto& p=*impl_;if(!p.enabled||p.device.Get()!=device||p.failed)return false;
    try{ComPtr<IDirect3DVertexShader9> vs;std::uint64_t vsHash{};UINT vsBytes{};if(FAILED(device->GetVertexShader(&vs))||!vs||!p.VertexKey(vs.Get(),vsHash,vsBytes))return false;
        DWORD alpha{};if(FAILED(device->GetRenderState(D3DRS_ALPHATESTENABLE,&alpha))||alpha)return false;
        ComPtr<IDirect3DPixelShader9> ps;if(FAILED(device->GetPixelShader(&ps))||!ps)return false;auto* e=p.Find(ps.Get());sss_native_recipes::ViewInfo view{};
        if(!e||!sss_native_recipes::KnownSkinPair(e->meta.hash,e->meta.bytes,vsHash,vsBytes,view))return false;
        const auto* profile=p.ApproveAtlas(device,e->colorSampler);if(!profile)return false;if(group)*group=static_cast<BYTE>(profile->index+1);return true;
    }catch(...){return false;}
}
bool Targets::Replay(IDirect3DDevice9* device,const Draw& draw,bool approved,BYTE group,bool knownUvMask) noexcept {
    auto& p=*impl_;if(!p.enabled||p.device.Get()!=device||p.failed){p.status=Status::Disabled;return false;}
    if(!approved||!group){p.status=Status::Unapproved;return false;}
    if(draw.kind<81||draw.kind>84||!draw.primitiveCount||draw.type<D3DPT_POINTLIST||draw.type>D3DPT_TRIANGLEFAN){p.status=Status::Unsupported;return false;}
    try {
        ComPtr<IDirect3DPixelShader9> ps;ComPtr<IDirect3DVertexShader9> vs;
        if(FAILED(device->GetPixelShader(&ps))||FAILED(device->GetVertexShader(&vs))||!ps||!vs){p.status=Status::Unsupported;return false;}
        std::uint64_t vsHash{};UINT vsBytes{};sss_native_recipes::ViewInfo nativeView{};
        if(!p.VertexKey(vs.Get(),vsHash,vsBytes)){p.status=Status::Unsupported;return false;}
        auto* entry=p.Find(ps.Get());if(!entry||!sss_native_recipes::KnownSkinPair(entry->meta.hash,entry->meta.bytes,vsHash,vsBytes,nativeView)){p.status=Status::Unsupported;return false;}
        const auto* profile=knownUvMask?p.ApproveAtlas(device,entry->colorSampler):nullptr;
        if(knownUvMask&&(!profile||group!=profile->index+1)){p.status=Status::Unapproved;return false;}
        if(!p.EnsureReplay(*entry)){p.status=Status::ResourceFailure;return false;}
        bool forward=entry->forward;
        if(forward){float emission[4]{};if(FAILED(device->GetPixelShaderConstantF(entry->info.materialEmissionRegister,emission,1)))return false;if(emission[0]!=0||emission[1]!=0||emission[2]!=0)forward=false;}
        if(knownUvMask){
            if(!p.OwnMask(profile)){p.status=Status::ResourceFailure;return false;}
            auto& masked=forward?entry->maskedReplay:entry->maskedMaskReplay;
            if(!masked){std::vector<DWORD> original,clone;sss_native_recipes::ReplayInfo info{};
                if(!Code(ps.Get(),original)||(forward?sss_native_recipes::BuildReplayShader(original.data(),static_cast<UINT>(original.size()*4),clone,info)!=S_OK:!BuildMask(original,clone))||
                   sss_native_recipes::AddEdwardUvMask(original.data(),static_cast<UINT>(original.size()*4),clone,forward?1u:0u)!=S_OK||FAILED(device->CreatePixelShader(clone.data(),&masked))){p.status=Status::Unsupported;return false;}
            }
        }
        DWORD alpha{},blend{},z{},stencil{};
        if(FAILED(device->GetRenderState(D3DRS_ALPHATESTENABLE,&alpha))||alpha||FAILED(device->GetRenderState(D3DRS_ALPHABLENDENABLE,&blend))||blend||
           FAILED(device->GetRenderState(D3DRS_ZENABLE,&z))||z!=D3DZB_TRUE||FAILED(device->GetRenderState(D3DRS_STENCILENABLE,&stencil))||stencil){p.status=Status::NativeState;return false;}
        Saved saved(device);if(!saved.valid){p.status=Status::NativeState;return false;}
        D3DSURFACE_DESC desc[4]{},ds{};
        for(UINT i=0;i<4;++i)if(!saved.rt[i]||FAILED(saved.rt[i]->GetDesc(&desc[i]))||desc[i].MultiSampleType!=D3DMULTISAMPLE_NONE){p.status=Status::NativeState;return false;}
        const auto& vp=saved.vp;const UINT width=desc[0].Width,height=desc[0].Height;
        if(!width||!height||width>4096||height>4096||std::uint64_t(width)*height>8294400ull||vp.X||vp.Y||vp.Width!=width||vp.Height!=height||vp.MinZ!=0||vp.MaxZ!=1||
           !saved.ds||FAILED(saved.ds->GetDesc(&ds))||ds.Width<width||ds.Height<height||ds.MultiSampleType!=D3DMULTISAMPLE_NONE){p.status=Status::NativeState;return false;}
        for(UINT i=1;i<4;++i)if(desc[i].Width!=width||desc[i].Height!=height){p.status=Status::NativeState;return false;}
        if(desc[3].Format!=D3DFMT_G16R16F&&desc[3].Format!=D3DFMT_G32R32F&&desc[3].Format!=D3DFMT_A16B16G16R16F&&desc[3].Format!=D3DFMT_A32B32G32R32F){p.status=Status::NativeState;return false;}
        ComPtr<IDirect3DTexture9> depth;ComPtr<IDirect3DSurface9> level;
        if(FAILED(saved.rt[3]->GetContainer(__uuidof(IDirect3DTexture9),reinterpret_cast<void**>(depth.GetAddressOf())))||FAILED(depth->GetSurfaceLevel(0,&level))||level.Get()!=saved.rt[3].Get()||!(desc[3].Usage&D3DUSAGE_RENDERTARGET)){p.status=Status::NativeState;return false;}
        float wvp[16]{},wv[12]{},sx{},sy{},sign{};
        if(FAILED(device->GetVertexShaderConstantF(nativeView.worldViewProjection,wvp,4))||FAILED(device->GetVertexShaderConstantF(nativeView.worldView,wv,3))||!DeriveProjection(wvp,wv,sx,sy,sign)){p.status=Status::Matrices;return false;}
        if(p.depth){
            bool same=p.depth.Get()==depth.Get()&&p.nativeDS.Get()==saved.ds.Get()&&p.width==width&&p.height==height&&Close(p.sx,sx)&&Close(p.sy,sy)&&p.sign==sign;
            for(UINT i=0;i<4;++i)same=same&&p.nativeRT[i].Get()==saved.rt[i].Get();
            if(!same){p.status=Status::FrameMismatch;return false;}
        }else{
            if(p.width!=width||p.height!=height){p.maskSurface.Reset();p.forwardSurface.Reset();p.mask.Reset();p.forward.Reset();}
            p.width=width;p.height=height;p.sx=sx;p.sy=sy;p.sign=sign;p.depth=depth;p.nativeDS=saved.ds;
            for(UINT i=0;i<4;++i)p.nativeRT[i]=saved.rt[i];
        }
        if(!p.mask&& !p.Texture(p.mask,p.maskSurface)){p.status=Status::ResourceFailure;return false;}
        if(entry->forward&&!p.forward&&!p.Texture(p.forward,p.forwardSurface)){p.status=Status::ResourceFailure;return false;}
        IDirect3DPixelShader9* replay=knownUvMask?(forward?entry->maskedReplay.Get():entry->maskedMaskReplay.Get()):(forward?entry->replay.Get():(entry->forward?entry->maskOnly.Get():entry->replay.Get()));
        auto set=[&](D3DRENDERSTATETYPE s,DWORD v){return SUCCEEDED(device->SetRenderState(s,v));};
        auto target=[&](UINT index,IDirect3DSurface9* s){return SUCCEEDED(device->SetRenderTarget(index,s));};
        bool ok=target(1,nullptr)&&target(2,nullptr)&&target(3,nullptr)&&SUCCEEDED(device->SetDepthStencilSurface(saved.ds.Get()));
        // Clear target-only, with native viewport, never native depth/stencil.
        if(ok&&!p.maskCleared){ok=target(0,p.maskSurface.Get())&&SUCCEEDED(device->SetViewport(&vp))&&SUCCEEDED(device->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0));p.maskCleared=ok;}
        if(ok&&forward&&!p.forwardCleared){ok=target(0,p.forwardSurface.Get())&&SUCCEEDED(device->SetViewport(&vp))&&SUCCEEDED(device->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0));p.forwardCleared=ok;}
        ok=ok&&target(0,forward?p.forwardSurface.Get():p.maskSurface.Get())&&target(1,forward?p.maskSurface.Get():nullptr)&&SUCCEEDED(device->SetViewport(&vp));
        ok=ok&&set(D3DRS_ZENABLE,D3DZB_TRUE)&&set(D3DRS_ZFUNC,D3DCMP_EQUAL)&&set(D3DRS_ZWRITEENABLE,FALSE)&&set(D3DRS_SRGBWRITEENABLE,FALSE)&&
            set(D3DRS_ALPHABLENDENABLE,FALSE)&&set(D3DRS_SEPARATEALPHABLENDENABLE,FALSE)&&set(D3DRS_COLORWRITEENABLE,15)&&set(D3DRS_COLORWRITEENABLE1,15)&&
            set(D3DRS_COLORWRITEENABLE2,0)&&set(D3DRS_COLORWRITEENABLE3,0)&&SUCCEEDED(device->SetPixelShader(replay));
        if(ok&&forward){
            float c[4]{};ok=SUCCEEDED(device->GetPixelShaderConstantF(entry->info.specularStrengthRegister,c,1));c[0]=0;
            ok=ok&&SUCCEEDED(device->SetPixelShaderConstantF(entry->info.specularStrengthRegister,c,1));
            if(entry->info.reflectionRegister!=sss_native_recipes::MissingRegister){ok=ok&&SUCCEEDED(device->GetPixelShaderConstantF(entry->info.reflectionRegister,c,1));c[0]=0;ok=ok&&SUCCEEDED(device->SetPixelShaderConstantF(entry->info.reflectionRegister,c,1));}
        }
        const float control[4]={1,static_cast<float>(group)/255.0f,0,0};
        if(ok&&knownUvMask){DWORD addressU{},addressV{};ok=SUCCEEDED(device->GetSamplerState(entry->colorSampler,D3DSAMP_ADDRESSU,&addressU))&&SUCCEEDED(device->GetSamplerState(entry->colorSampler,D3DSAMP_ADDRESSV,&addressV))&&SUCCEEDED(device->SetTexture(15,p.authoredMask.Get()));
            const D3DSAMPLERSTATETYPE states[]={D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER,D3DSAMP_MIPFILTER,D3DSAMP_ADDRESSU,D3DSAMP_ADDRESSV,D3DSAMP_SRGBTEXTURE,D3DSAMP_MAXMIPLEVEL,D3DSAMP_MIPMAPLODBIAS,D3DSAMP_BORDERCOLOR};
            const DWORD values[]={D3DTEXF_LINEAR,D3DTEXF_LINEAR,D3DTEXF_LINEAR,addressU,addressV,FALSE,0,0,0};
            for(UINT i=0;i<std::size(states);++i)ok=ok&&SUCCEEDED(device->SetSamplerState(15,states[i],values[i]));
        }
        ok=ok&&SUCCEEDED(device->SetPixelShaderConstantF(223,control,1))&&SUCCEEDED(Issue(device,draw));
        if(!ok){p.failed=true;p.written=p.forwardWritten=false;p.status=Status::ReplayFailure;return false;}
        p.written=true;p.forwardWritten=p.forwardWritten||forward;p.status=Status::Ready;return true;
    }catch(...){p.failed=true;p.written=p.forwardWritten=false;p.status=Status::ResourceFailure;return false;}
}
bool Targets::ReplayDiffuseLight(IDirect3DDevice9* device,const Draw& draw,bool approved,UINT depthSampler,UINT zeroSpecularRgbRegister,bool beforeNative) noexcept {
    auto& p=*impl_;if(!Ready()||p.device.Get()!=device){p.status=Status::Disabled;return false;}
    if(!approved){p.status=Status::Unapproved;return false;}
    if((zeroSpecularRgbRegister!=~0u&&zeroSpecularRgbRegister>=224)||depthSampler>=16||draw.kind<81||draw.kind>84||!draw.primitiveCount||draw.type<D3DPT_POINTLIST||draw.type>D3DPT_TRIANGLEFAN){p.status=Status::Unsupported;return false;}
    try {
        ComPtr<IDirect3DBaseTexture9> boundDepth;
        if(FAILED(device->GetTexture(depthSampler,&boundDepth))||boundDepth.Get()!=p.depth.Get()){p.status=Status::NativeState;return false;}
        Saved saved(device);D3DSURFACE_DESC desc{};
        if(!saved.valid||!saved.rt[0]||FAILED(saved.rt[0]->GetDesc(&desc))||saved.ds.Get()!=p.nativeDS.Get()||desc.Width!=p.width||desc.Height!=p.height||desc.MultiSampleType!=D3DMULTISAMPLE_NONE||
           saved.vp.X||saved.vp.Y||saved.vp.Width!=p.width||saved.vp.Height!=p.height||saved.vp.MinZ!=0||saved.vp.MaxZ!=1||
           (desc.Format!=D3DFMT_A16B16G16R16F&&desc.Format!=D3DFMT_A32B32G32R32F)){p.status=Status::NativeState;return false;}
        auto state=[&](D3DRENDERSTATETYPE s,DWORD expected){DWORD v{};return SUCCEEDED(device->GetRenderState(s,&v))&&v==expected;};
        if(!state(D3DRS_ALPHATESTENABLE,FALSE)||!state(D3DRS_ZWRITEENABLE,FALSE)||!state(D3DRS_ALPHABLENDENABLE,TRUE)||
           !state(D3DRS_SRCBLEND,D3DBLEND_ONE)||!state(D3DRS_DESTBLEND,D3DBLEND_ONE)||!state(D3DRS_BLENDOP,D3DBLENDOP_ADD)||!state(D3DRS_SRGBWRITEENABLE,FALSE)||
           !state(D3DRS_SEPARATEALPHABLENDENABLE,FALSE)){p.status=Status::NativeState;return false;}
        bool protectStencil=false;DWORD stencil{};if(FAILED(device->GetRenderState(D3DRS_STENCILENABLE,&stencil))){p.status=Status::NativeState;return false;}
        if(stencil){
            DWORD twoSided{};
            if(FAILED(device->GetRenderState(D3DRS_TWOSIDEDSTENCILMODE,&twoSided))){p.status=Status::NativeState;return false;}
            const bool readOnly=state(D3DRS_STENCILWRITEMASK,0)&&state(D3DRS_STENCILFAIL,D3DSTENCILOP_KEEP)&&state(D3DRS_STENCILZFAIL,D3DSTENCILOP_KEEP)&&state(D3DRS_STENCILPASS,D3DSTENCILOP_KEEP)&&
                (!twoSided||(state(D3DRS_CCW_STENCILFAIL,D3DSTENCILOP_KEEP)&&state(D3DRS_CCW_STENCILZFAIL,D3DSTENCILOP_KEEP)&&state(D3DRS_CCW_STENCILPASS,D3DSTENCILOP_KEEP)));
            const bool capturedDestructive=beforeNative&&!twoSided&&state(D3DRS_STENCILFUNC,D3DCMP_EQUAL)&&state(D3DRS_STENCILREF,1)&&state(D3DRS_STENCILMASK,0xffffffffu)&&
                state(D3DRS_STENCILWRITEMASK,0xffffffffu)&&state(D3DRS_STENCILFAIL,D3DSTENCILOP_KEEP)&&state(D3DRS_STENCILZFAIL,D3DSTENCILOP_INCR)&&state(D3DRS_STENCILPASS,D3DSTENCILOP_DECR);
            if(!readOnly&&!capturedDestructive){p.status=Status::NativeState;return false;}protectStencil=capturedDestructive;
        }
        ComPtr<IDirect3DPixelShader9> ps;ComPtr<IDirect3DVertexShader9> vs;
        if(FAILED(device->GetPixelShader(&ps))||!ps||FAILED(device->GetVertexShader(&vs))||!vs){p.status=Status::Unsupported;return false;}
        if(!p.forward&&!p.Texture(p.forward,p.forwardSurface)){p.status=Status::ResourceFailure;return false;}
        bool ok=true;for(UINT i=1;i<4;++i)ok=ok&&SUCCEEDED(device->SetRenderTarget(i,nullptr));
        ok=ok&&SUCCEEDED(device->SetRenderTarget(0,p.forwardSurface.Get()))&&SUCCEEDED(device->SetViewport(&saved.vp));
        if(ok&&protectStencil){const D3DRENDERSTATETYPE states[]={D3DRS_STENCILWRITEMASK,D3DRS_STENCILFAIL,D3DRS_STENCILZFAIL,D3DRS_STENCILPASS};const DWORD values[]={0,D3DSTENCILOP_KEEP,D3DSTENCILOP_KEEP,D3DSTENCILOP_KEEP};
            for(UINT i=0;i<std::size(states);++i)ok=ok&&SUCCEEDED(device->SetRenderState(states[i],values[i]));
        }
        if(ok&&!p.forwardCleared){ok=SUCCEEDED(device->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0));p.forwardCleared=ok;}
        // Shader bytecode and native input/state stay exact. For independently
        // reviewed mixed variants, remove only the specular RGB uniform on the
        // copied draw; state-block restoration reinstates all four components.
        if(ok&&zeroSpecularRgbRegister!=~0u){
            float specular[4]{};ok=SUCCEEDED(device->GetPixelShaderConstantF(zeroSpecularRgbRegister,specular,1));
            specular[0]=specular[1]=specular[2]=0;
            ok=ok&&SUCCEEDED(device->SetPixelShaderConstantF(zeroSpecularRgbRegister,specular,1));
        }
        ok=ok&&SUCCEEDED(Issue(device,draw));
        if(!ok){p.failed=true;p.written=p.forwardWritten=false;p.status=Status::ReplayFailure;return false;}
        p.forwardWritten=true;p.status=Status::Ready;return true;
    }catch(...){p.failed=true;p.written=p.forwardWritten=false;p.status=Status::ResourceFailure;return false;}
}
bool Targets::MatchesBaseRadiance(IDirect3DBaseTexture9* texture)const noexcept {
 if(!texture||!impl_->written||!impl_->nativeRT[0])return false;
 ComPtr<IDirect3DTexture9> source;
 return SUCCEEDED(impl_->nativeRT[0]->GetContainer(__uuidof(IDirect3DTexture9),reinterpret_cast<void**>(source.GetAddressOf())))&&source.Get()==texture;
}
IDirect3DTexture9* Targets::CombinedDiffuse() const noexcept{return ForwardDiffuse();}
IDirect3DTexture9* Targets::Mask() const noexcept{return Ready()?impl_->mask.Get():nullptr;}
IDirect3DTexture9* Targets::ForwardDiffuse() const noexcept{return Ready()&&impl_->forwardWritten?impl_->forward.Get():nullptr;}
IDirect3DTexture9* Targets::Depth() const noexcept{return Ready()?impl_->depth.Get():nullptr;}
std::uint64_t Targets::FrameSerial() const noexcept{return impl_->serial;}
UINT Targets::Width() const noexcept{return impl_->width;}
UINT Targets::Height() const noexcept{return impl_->height;}
float Targets::ProjectionScaleX() const noexcept{return impl_->sx;}
float Targets::ProjectionScaleY() const noexcept{return impl_->sy;}
float Targets::DepthSign() const noexcept{return impl_->sign;}
bool Targets::Ready() const noexcept{return impl_->enabled&&impl_->written&&!impl_->failed;}
Status Targets::LastStatus() const noexcept{return impl_->status;}
}
