#define NOMINMAX
#include "sss_capture.h"
#include "sss_native_passes.h"
#include <cstdio>
#include <cstring>
#include <new>
#include <cmath>
namespace sss_capture {
namespace {
constexpr unsigned MaxPreviews=32,PreviewBytes=64*1024;
constexpr unsigned MaxDraws=4096,MaxShaders=512,MaxResources=1024,ShaderBytes=4*1024*1024,MaxShaderBytes=32768;
struct Resource {unsigned id{},type{},width{},height{},depth{},format{},usage{},pool{},container{};ULONG_PTR pointer{};};
struct Sampler {Resource resource;DWORD srgb{},u{},v{},w{},min{},mag{},mip{};};
struct Shader {ULONG_PTR pointer{};ULONGLONG hash{};unsigned id{},stage{},offset{},bytes{};HRESULT result{};};
struct Preview {unsigned faces=1;unsigned resource{},firstDraw{},sampler{},mip{},width{},height{},format{},pool{},rowBytes{},rows{},bytes{},reason{};ULONGLONG shaderHash{},hash{};HRESULT result{};BYTE raw[PreviewBytes]{};};
const char* PreviewReason(unsigned reason){const char* names[]={"ok","not_managed_2d","no_existing_mip_at_most_256_within_64KiB","unsupported_format","lock_failed","invalid_pitch","unlock_failed"};return reason<7?names[reason]:"unknown";}
unsigned PixelBytes(D3DFORMAT format){switch(format){case D3DFMT_A8R8G8B8:case D3DFMT_X8R8G8B8:return 4;case D3DFMT_R5G6B5:case D3DFMT_A1R5G5B5:case D3DFMT_A4R4G4B4:case D3DFMT_A8L8:return 2;case D3DFMT_L8:case D3DFMT_A8:return 1;default:return 0;}}
struct Draw {
 char kind[32]{};unsigned ordinal{},primitive{},primitiveCount{},vertexCount{},ps{},vs{};Resource rt[4],ds;D3DVIEWPORT9 viewport{};
 DWORD zEnable{},zWrite{},zFunc{},alphaTest{},alphaRef{},alphaFunc{},blend{},srcBlend{},dstBlend{},blendOp{},separateAlpha{},srcBlendAlpha{},dstBlendAlpha{},blendOpAlpha{},colorWrite[4]{},srgbWrite{},cull{};
 Sampler samplers[16];float constants[224*4]{},vsConstants[21*4]{};DWORD stencil[13]{};bool constantsValid{},vsConstantsValid{},viewportValid{};ULONG_PTR callers[6]{};unsigned callerCount{};
};
ULONGLONG Hash(const BYTE* data,unsigned bytes){ULONGLONG value=14695981039346656037ull;for(unsigned n=0;n<bytes;++n){value^=data[n];value*=1099511628211ull;}return value;}
void ResourceJson(FILE* f,const Resource& r){std::fprintf(f,"{\"id\":%u,\"pointer\":\"%llx\",\"type\":%u,\"width\":%u,\"height\":%u,\"depth\":%u,\"format\":%u,\"usage\":%u,\"pool\":%u,\"container_texture_id\":%u}",r.id,static_cast<ULONGLONG>(r.pointer),r.type,r.width,r.height,r.depth,r.format,r.usage,r.pool,r.container);}
void FloatJson(FILE* f,float value){if(std::isfinite(value))std::fprintf(f,"%.9g",static_cast<double>(value));else std::fprintf(f,"null");}
bool SameResource(const Resource& a,const Resource& b){return a.pointer==b.pointer&&a.type==b.type&&a.width==b.width&&a.height==b.height&&a.depth==b.depth&&a.format==b.format&&a.usage==b.usage&&a.pool==b.pool;}
bool WriteText(FILE* f,const char* value){for(const unsigned char* p=reinterpret_cast<const unsigned char*>(value);*p;++p){if(*p=='"'||*p=='\\')std::fputc('\\',f);if(*p>=32)std::fputc(*p,f);}return !std::ferror(f);}
}
struct Recorder::Storage {
 Draw draws[MaxDraws];Shader shaders[MaxShaders];Resource resources[MaxResources];BYTE bytecode[ShaderBytes];Preview previews[MaxPreviews];
 unsigned drawsUsed{},shadersUsed{},resourcesUsed{},bytesUsed{},drawsDropped{},shaderDrops{},resourceDrops{},previewsUsed{},previewDrops{};ULONGLONG started{};
 unsigned Identify(Resource& item){if(!item.pointer)return 0;for(unsigned n=0;n<resourcesUsed;++n)if(SameResource(resources[n],item)){item.id=resources[n].id;return item.id;}if(resourcesUsed>=MaxResources){++resourceDrops;return 0;}item.id=resourcesUsed+1;resources[resourcesUsed++]=item;return item.id;}
 Resource Surface(IDirect3DSurface9* surface){Resource out{};if(!surface)return out;D3DSURFACE_DESC d{};if(SUCCEEDED(surface->GetDesc(&d))){out.pointer=reinterpret_cast<ULONG_PTR>(surface);out.type=D3DRTYPE_SURFACE;out.width=d.Width;out.height=d.Height;out.format=d.Format;out.usage=d.Usage;out.pool=d.Pool;IDirect3DBaseTexture9* owner=nullptr;if(SUCCEEDED(surface->GetContainer(__uuidof(IDirect3DBaseTexture9),reinterpret_cast<void**>(&owner)))&&owner){out.container=Texture(owner).id;owner->Release();}Identify(out);}return out;}
 Resource Texture(IDirect3DBaseTexture9* texture){Resource out{};if(!texture)return out;out.pointer=reinterpret_cast<ULONG_PTR>(texture);out.type=texture->GetType();
  if(out.type==D3DRTYPE_TEXTURE){D3DSURFACE_DESC d{};if(SUCCEEDED(static_cast<IDirect3DTexture9*>(texture)->GetLevelDesc(0,&d))){out.width=d.Width;out.height=d.Height;out.format=d.Format;out.usage=d.Usage;out.pool=d.Pool;}}
  else if(out.type==D3DRTYPE_CUBETEXTURE){D3DSURFACE_DESC d{};if(SUCCEEDED(static_cast<IDirect3DCubeTexture9*>(texture)->GetLevelDesc(0,&d))){out.width=d.Width;out.height=d.Height;out.depth=6;out.format=d.Format;out.usage=d.Usage;out.pool=d.Pool;}}
  else if(out.type==D3DRTYPE_VOLUMETEXTURE){D3DVOLUME_DESC d{};if(SUCCEEDED(static_cast<IDirect3DVolumeTexture9*>(texture)->GetLevelDesc(0,&d))){out.width=d.Width;out.height=d.Height;out.depth=d.Depth;out.format=d.Format;out.usage=d.Usage;out.pool=d.Pool;}}
  Identify(out);return out;
 }
 void RecordPreview(IDirect3DBaseTexture9* base,const Resource& resource,unsigned draw,unsigned sampler,ULONGLONG shaderHash){
  if(!resource.id)return;for(unsigned n=0;n<previewsUsed;++n)if(previews[n].resource==resource.id&&previews[n].sampler==sampler)return;if(previewsUsed>=MaxPreviews){++previewDrops;return;}
  auto& out=previews[previewsUsed++];out.faces=1;out.resource=resource.id;out.firstDraw=draw;out.sampler=sampler;out.shaderHash=shaderHash;out.pool=resource.pool;out.format=resource.format;
  const bool cube=resource.type==D3DRTYPE_CUBETEXTURE;
  if((resource.type!=D3DRTYPE_TEXTURE&&!cube)||resource.pool!=D3DPOOL_MANAGED){out.reason=1;return;}out.faces=cube?6u:1u;D3DSURFACE_DESC desc{};bool found=false;
  const auto levels=cube?static_cast<IDirect3DCubeTexture9*>(base)->GetLevelCount():static_cast<IDirect3DTexture9*>(base)->GetLevelCount();
  for(unsigned mip=0;mip<levels&&mip<16;++mip){const auto hr=cube?static_cast<IDirect3DCubeTexture9*>(base)->GetLevelDesc(mip,&desc):static_cast<IDirect3DTexture9*>(base)->GetLevelDesc(mip,&desc);if(FAILED(hr)||!desc.Width||!desc.Height||desc.Width>256||desc.Height>256)continue;
   const bool compressed=desc.Format>=D3DFMT_DXT1&&desc.Format<=D3DFMT_DXT5;const auto pixelBytes=PixelBytes(desc.Format);if(compressed){out.rowBytes=((desc.Width+3)/4)*(desc.Format==D3DFMT_DXT1?8:16);out.rows=(desc.Height+3)/4;}else if(pixelBytes){out.rowBytes=desc.Width*pixelBytes;out.rows=desc.Height;}else{out.reason=3;return;}
   if(out.rowBytes*out.rows*out.faces<=PreviewBytes){out.mip=mip;found=true;break;}}
  if(!found){out.reason=2;return;}out.width=desc.Width;out.height=desc.Height;out.format=desc.Format;
  if(desc.Pool!=D3DPOOL_MANAGED||(desc.Usage&(D3DUSAGE_RENDERTARGET|D3DUSAGE_DEPTHSTENCIL|D3DUSAGE_DYNAMIC))){out.reason=1;return;}
  const unsigned faceBytes=out.rowBytes*out.rows;if(faceBytes*out.faces>PreviewBytes){out.reason=3;return;}
  for(unsigned face=0;face<out.faces;++face){D3DLOCKED_RECT locked{};out.result=cube?static_cast<IDirect3DCubeTexture9*>(base)->LockRect(static_cast<D3DCUBEMAP_FACES>(face),out.mip,&locked,nullptr,D3DLOCK_READONLY):static_cast<IDirect3DTexture9*>(base)->LockRect(out.mip,&locked,nullptr,D3DLOCK_READONLY);if(FAILED(out.result)){out.reason=4;return;}
   if(locked.pBits&&locked.Pitch>0&&static_cast<unsigned>(locked.Pitch)>=out.rowBytes&&locked.Pitch<=65536){for(unsigned row=0;row<out.rows;++row)std::memcpy(out.raw+face*faceBytes+row*out.rowBytes,static_cast<const BYTE*>(locked.pBits)+row*locked.Pitch,out.rowBytes);}else out.reason=5;
   out.result=cube?static_cast<IDirect3DCubeTexture9*>(base)->UnlockRect(static_cast<D3DCUBEMAP_FACES>(face),out.mip):static_cast<IDirect3DTexture9*>(base)->UnlockRect(out.mip);if(FAILED(out.result)){out.reason=6;return;}if(out.reason)return;
  }
  out.bytes=faceBytes*out.faces;out.hash=Hash(out.raw,out.bytes);
 }
 template<class T>unsigned RecordShader(T* shader,unsigned stage){if(!shader)return 0;const auto pointer=reinterpret_cast<ULONG_PTR>(shader);for(unsigned n=0;n<shadersUsed;++n)if(shaders[n].pointer==pointer&&shaders[n].stage==stage)return shaders[n].id;if(shadersUsed>=MaxShaders){++shaderDrops;return 0;}
  Shader next{};next.pointer=pointer;next.stage=stage;next.id=shadersUsed+1;UINT bytes=0;next.result=shader->GetFunction(nullptr,&bytes);
  if(SUCCEEDED(next.result)&&bytes&&bytes<=MaxShaderBytes&&bytes<=ShaderBytes-bytesUsed){UINT actual=bytes;next.result=shader->GetFunction(bytecode+bytesUsed,&actual);if(SUCCEEDED(next.result)&&actual<=bytes){next.offset=bytesUsed;next.bytes=actual;next.hash=Hash(bytecode+bytesUsed,actual);bytesUsed+=actual;}}
  else {next.result=FAILED(next.result)?next.result:E_OUTOFMEMORY;++shaderDrops;}
  shaders[shadersUsed++]=next;return next.id;
 }
};

Recorder::~Recorder(){delete data;}
bool Recorder::Prepare() noexcept {
 static_assert(sizeof(Storage)<=32u*1024*1024,"SSS passive capture memory budget");
 if(!data)data=new(std::nothrow)Storage;if(!data){error=ERROR_NOT_ENOUGH_MEMORY;return false;}std::memset(data,0,sizeof *data);error=0;directory[0]=0;return true;
}
void Recorder::Start(IDirect3DDevice9* current,bool water) noexcept {data->started=GetTickCount64();device=current;active=true;suspended=false;lastWater=water;armed=false;waterArmed=false;waterPrepared=false;}
void Recorder::BeginFrame(IDirect3DDevice9* current,bool request,bool eligible,bool waterRequest) noexcept {
 if(request&&!previousRequest)armed=true;if(!request)armed=false;previousRequest=request;
 if(waterRequest&&!previousWaterRequest){waterArmed=true;waterPrepared=false;}if(!waterRequest){waterArmed=false;waterPrepared=false;}previousWaterRequest=waterRequest;
 waterEligible=waterRequest&&current;probeUsed=probeNext=0;if(active)return;
 if(waterArmed){device=current;if(waterEligible&&!waterPrepared){waterPrepared=Prepare();if(!waterPrepared)waterArmed=false;}return;}
 if(!armed||!eligible||!current)return;if(!Prepare()){armed=false;return;}Start(current,false);
}
bool Recorder::WaterTarget(IDirect3DDevice9* current) noexcept {
 IDirect3DPixelShader9* shader=nullptr;if(FAILED(current->GetPixelShader(&shader))||!shader)return false;const auto pointer=reinterpret_cast<ULONG_PTR>(shader);bool water=false,known=false;
 for(unsigned n=0;n<probeUsed;++n)if(probes[n].object==pointer){water=probes[n].water;known=true;break;}
 if(!known){UINT bytes{};if(SUCCEEDED(shader->GetFunction(nullptr,&bytes))&&(bytes==2544||bytes==1976)){BYTE code[2544]{};UINT count=bytes;if(SUCCEEDED(shader->GetFunction(code,&count))&&count==bytes){const auto hash=Hash(code,bytes);water=(bytes==2544&&hash==0xc12fdf95e31fdc22ull)||(bytes==1976&&hash==0xeee6246d44f2812bull);}}
  const unsigned slot=probeUsed<128?probeUsed++:probeNext++%128;probes[slot]={pointer,water};}
 shader->Release();if(!water)return false;
 D3DVIEWPORT9 viewport{};if(FAILED(current->GetViewport(&viewport))||viewport.Width<640||viewport.Height<360)return false;
 IDirect3DSurface9* target=nullptr;IDirect3DSurface9* back=nullptr;D3DSURFACE_DESC rt{},bb{};bool main=false;
 if(SUCCEEDED(current->GetRenderTarget(0,&target))&&target&&SUCCEEDED(target->GetDesc(&rt))&&SUCCEEDED(current->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))&&back&&SUCCEEDED(back->GetDesc(&bb)))main=rt.Width==bb.Width&&rt.Height<=bb.Height&&bb.Height-rt.Height<=64&&viewport.Width==rt.Width&&viewport.Height==rt.Height;
 if(target)target->Release();if(back)back->Release();return main;
}
void Recorder::BeforeDraw(IDirect3DDevice9* current,const char* kind,D3DPRIMITIVETYPE primitive,unsigned primitives,unsigned vertices) noexcept {
 if(suspended)return;if(!active&&waterArmed&&waterEligible&&waterPrepared&&current==device&&WaterTarget(current))Start(current,true);
 if(!active||current!=device||!data)return;if(data->drawsUsed>=MaxDraws){++data->drawsDropped;return;}auto& d=data->draws[data->drawsUsed++];d.ordinal=data->drawsUsed;d.primitive=primitive;d.primitiveCount=primitives;d.vertexCount=vertices;strncpy_s(d.kind,kind?kind:"unknown",_TRUNCATE);
 IDirect3DPixelShader9* ps=nullptr;IDirect3DVertexShader9* vs=nullptr;if(SUCCEEDED(current->GetPixelShader(&ps))&&ps){d.ps=data->RecordShader(ps,0);ps->Release();}if(SUCCEEDED(current->GetVertexShader(&vs))&&vs){d.vs=data->RecordShader(vs,1);vs->Release();}
 for(unsigned n=0;n<4;++n){IDirect3DSurface9* surface=nullptr;if(SUCCEEDED(current->GetRenderTarget(n,&surface))&&surface){d.rt[n]=data->Surface(surface);surface->Release();}}
 IDirect3DSurface9* depth=nullptr;if(SUCCEEDED(current->GetDepthStencilSurface(&depth))&&depth){d.ds=data->Surface(depth);depth->Release();}d.viewportValid=SUCCEEDED(current->GetViewport(&d.viewport));
 const D3DRENDERSTATETYPE states[]={D3DRS_ZENABLE,D3DRS_ZWRITEENABLE,D3DRS_ZFUNC,D3DRS_ALPHATESTENABLE,D3DRS_ALPHAREF,D3DRS_ALPHAFUNC,D3DRS_ALPHABLENDENABLE,D3DRS_SRCBLEND,D3DRS_DESTBLEND,D3DRS_BLENDOP,D3DRS_SEPARATEALPHABLENDENABLE,D3DRS_SRCBLENDALPHA,D3DRS_DESTBLENDALPHA,D3DRS_BLENDOPALPHA,D3DRS_COLORWRITEENABLE,D3DRS_COLORWRITEENABLE1,D3DRS_COLORWRITEENABLE2,D3DRS_COLORWRITEENABLE3,D3DRS_SRGBWRITEENABLE,D3DRS_CULLMODE};
 DWORD* outputs[]={&d.zEnable,&d.zWrite,&d.zFunc,&d.alphaTest,&d.alphaRef,&d.alphaFunc,&d.blend,&d.srcBlend,&d.dstBlend,&d.blendOp,&d.separateAlpha,&d.srcBlendAlpha,&d.dstBlendAlpha,&d.blendOpAlpha,&d.colorWrite[0],&d.colorWrite[1],&d.colorWrite[2],&d.colorWrite[3],&d.srgbWrite,&d.cull};for(unsigned n=0;n<20;++n){const auto hr=current->GetRenderState(states[n],outputs[n]);(void)hr;}
 const bool water=lastWater&&d.ps&&((data->shaders[d.ps-1].hash==0xc12fdf95e31fdc22ull&&data->shaders[d.ps-1].bytes==2544)||(data->shaders[d.ps-1].hash==0xeee6246d44f2812bull&&data->shaders[d.ps-1].bytes==1976));
 const bool facial=d.ps&&sss_native_passes::Lookup(data->shaders[d.ps-1].hash,data->shaders[d.ps-1].bytes).role==sss_native_passes::Role::Face;
 for(unsigned n=0;n<16;++n){auto& s=d.samplers[n];IDirect3DBaseTexture9* texture=nullptr;if(SUCCEEDED(current->GetTexture(n,&texture))&&texture){s.resource=data->Texture(texture);if((facial&&n>=2&&n<=5)||(water&&n<=3))data->RecordPreview(texture,s.resource,d.ordinal,n,data->shaders[d.ps-1].hash);texture->Release();}
  const D3DSAMPLERSTATETYPE statesSampler[]={D3DSAMP_SRGBTEXTURE,D3DSAMP_ADDRESSU,D3DSAMP_ADDRESSV,D3DSAMP_ADDRESSW,D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER,D3DSAMP_MIPFILTER};DWORD* values[]={&s.srgb,&s.u,&s.v,&s.w,&s.min,&s.mag,&s.mip};for(unsigned k=0;k<7;++k){const auto hr=current->GetSamplerState(n,statesSampler[k],values[k]);(void)hr;}}
 const D3DRENDERSTATETYPE stencilStates[]={D3DRS_STENCILENABLE,D3DRS_STENCILFAIL,D3DRS_STENCILZFAIL,D3DRS_STENCILPASS,D3DRS_STENCILFUNC,D3DRS_STENCILREF,D3DRS_STENCILMASK,D3DRS_STENCILWRITEMASK,D3DRS_TWOSIDEDSTENCILMODE,D3DRS_CCW_STENCILFUNC,D3DRS_CCW_STENCILPASS,D3DRS_CCW_STENCILFAIL,D3DRS_CCW_STENCILZFAIL};for(unsigned n=0;n<13;++n){const auto hr=current->GetRenderState(stencilStates[n],&d.stencil[n]);(void)hr;}
 d.vsConstantsValid=SUCCEEDED(current->GetVertexShaderConstantF(216,d.vsConstants,21));
 d.constantsValid=SUCCEEDED(current->GetPixelShaderConstantF(0,d.constants,224));void* trace[6]{};d.callerCount=CaptureStackBackTrace(1,6,trace,nullptr);for(unsigned n=0;n<d.callerCount;++n)d.callers[n]=reinterpret_cast<ULONG_PTR>(trace[n]);
}
void Recorder::EndFrame() noexcept {
 if(!active)return;active=false;device=nullptr;if(!data)return;wchar_t root[768]{};const auto size=GetEnvironmentVariableW(L"LOCALAPPDATA",root,768);if(!size||size>=768){error=ERROR_BAD_PATHNAME;return;}
 wchar_t folder[1024]{};swprintf_s(folder,L"%s\\AITD2008Enhanced",root);if(!CreateDirectoryW(folder,nullptr)&&GetLastError()!=ERROR_ALREADY_EXISTS){error=GetLastError();return;}swprintf_s(folder,L"%s\\AITD2008Enhanced\\sss-captures",root);if(!CreateDirectoryW(folder,nullptr)&&GetLastError()!=ERROR_ALREADY_EXISTS){error=GetLastError();return;}
 bool created=false;for(unsigned retry=0;retry<64;++retry){swprintf_s(directory,L"%s\\%lu-%lu",folder,GetCurrentProcessId(),++generation);if(CreateDirectoryW(directory,nullptr)){created=true;break;}if(GetLastError()!=ERROR_ALREADY_EXISTS){error=GetLastError();return;}}if(!created){error=ERROR_ALREADY_EXISTS;return;}
 for(unsigned n=0;n<data->shadersUsed;++n){const auto& s=data->shaders[n];if(!s.bytes)continue;wchar_t path[1200]{};swprintf_s(path,L"%s\\%s-%03u-%016llx.bin",directory,s.stage?L"vs":L"ps",s.id,s.hash);FILE* file=nullptr;_wfopen_s(&file,path,L"wb");if(!file){error=ERROR_WRITE_FAULT;continue;}if(std::fwrite(data->bytecode+s.offset,1,s.bytes,file)!=s.bytes)error=ERROR_WRITE_FAULT;std::fclose(file);}
 wchar_t constantsPath[1200]{};swprintf_s(constantsPath,L"%s\\ps-constants.bin",directory);FILE* raw=nullptr;_wfopen_s(&raw,constantsPath,L"wb");if(!raw)error=ERROR_WRITE_FAULT;else{for(unsigned n=0;n<data->drawsUsed;++n)if(std::fwrite(data->draws[n].constants,1,sizeof data->draws[n].constants,raw)!=sizeof data->draws[n].constants)error=ERROR_WRITE_FAULT;std::fclose(raw);}
 wchar_t vsPath[1200]{};swprintf_s(vsPath,L"%s\\vs-c216-c236.bin",directory);FILE* vsRaw=nullptr;_wfopen_s(&vsRaw,vsPath,L"wb");if(!vsRaw)error=ERROR_WRITE_FAULT;else{for(unsigned n=0;n<data->drawsUsed;++n)if(std::fwrite(data->draws[n].vsConstants,1,sizeof data->draws[n].vsConstants,vsRaw)!=sizeof data->draws[n].vsConstants)error=ERROR_WRITE_FAULT;std::fclose(vsRaw);}
 for(unsigned n=0;n<data->previewsUsed;++n){const auto& preview=data->previews[n];if(!preview.bytes)continue;wchar_t previewPath[1200]{};swprintf_s(previewPath,L"%s\\texture-%04u-mip%u-%016llx.bin",directory,preview.resource,preview.mip,preview.hash);FILE* previewFile=nullptr;_wfopen_s(&previewFile,previewPath,L"wb");if(!previewFile){error=ERROR_WRITE_FAULT;continue;}if(std::fwrite(preview.raw,1,preview.bytes,previewFile)!=preview.bytes)error=ERROR_WRITE_FAULT;std::fclose(previewFile);}
 bool previewPixels=false;for(unsigned n=0;n<data->previewsUsed;++n)if(data->previews[n].bytes)previewPixels=true;
 wchar_t path[1200]{};swprintf_s(path,L"%s\\capture.json",directory);FILE* file=nullptr;_wfopen_s(&file,path,L"wb");if(!file){error=ERROR_WRITE_FAULT;return;}
 std::fprintf(file,"{\"schema\":2,\"build\":\"v146\",\"pid\":%lu,\"generation\":%lu,\"capture_started_tick_ms\":%llu,\"finished_tick_ms\":%llu,\"passive\":true,\"pixels_read_back\":%s,\"gpu_pixels_read_back\":false,\"identity_note\":\"IDs are local to one capture; resource descriptors participate in identity; shader pointer reuse within one frame is not tracked\",\"limits\":{\"draws\":4096,\"shaders\":512,\"resources\":1024,\"shader_bytes\":4194304,\"previews\":32,\"preview_bytes_each\":65536,\"storage_bytes\":%zu},\"dropped\":{\"draws\":%u,\"shaders\":%u,\"resources\":%u},\"vs_constants_file\":\"vs-c216-c236.bin\",\"vs_constants_first_register\":216,\"vs_constants_stride_bytes\":336,\"ps_constants_file\":\"ps-constants.bin\",\"ps_constants_stride_bytes\":3584,\"shaders\":[",GetCurrentProcessId(),generation,data->started,GetTickCount64(),previewPixels?"true":"false",sizeof(Storage),data->drawsDropped,data->shaderDrops,data->resourceDrops);
 for(unsigned n=0;n<data->shadersUsed;++n){const auto& s=data->shaders[n];std::fprintf(file,"%s{\"id\":%u,\"stage\":\"%s\",\"pointer\":\"%llx\",\"fnv1a64\":\"%016llx\",\"bytes\":%u,\"get_function_hresult\":\"%08lx\"}",n?",":"",s.id,s.stage?"vs":"ps",static_cast<ULONGLONG>(s.pointer),s.hash,s.bytes,static_cast<DWORD>(s.result));}std::fprintf(file,"],\"managed_texture_previews\":[");for(unsigned n=0;n<data->previewsUsed;++n){const auto& p=data->previews[n];std::fprintf(file,"%s{\"resource\":%u,\"first_draw\":%u,\"sampler\":%u,\"source_ps_fnv1a64\":\"%016llx\",\"pool\":%u,\"mip\":%u,\"width\":%u,\"height\":%u,\"format\":%u,\"row_bytes\":%u,\"rows\":%u,\"bytes\":%u,\"logical_mip_fnv1a64\":\"%016llx\",\"result\":\"%s\",\"lock_flags\":16,\"cube_face_count\":%u,\"bytes_per_face\":%u,\"hresult\":\"%08lx\"}",n?",":"",p.resource,p.firstDraw,p.sampler,p.shaderHash,p.pool,p.mip,p.width,p.height,p.format,p.rowBytes,p.rows,p.bytes,p.hash,PreviewReason(p.reason),p.faces,p.rowBytes*p.rows,static_cast<DWORD>(p.result));}std::fprintf(file,"],\"capture_kind\":\"%s\",\"preview_dropped\":%u,\"resources\":[",lastWater?"water":"sss",data->previewDrops);for(unsigned n=0;n<data->resourcesUsed;++n){if(n)std::fputc(',',file);ResourceJson(file,data->resources[n]);}std::fprintf(file,"],\"draws\":[");
 for(unsigned n=0;n<data->drawsUsed;++n){const auto& d=data->draws[n];std::fprintf(file,"%s{\"ordinal\":%u,\"kind\":\"",n?",":"",d.ordinal);WriteText(file,d.kind);std::fprintf(file,"\",\"primitive_type\":%u,\"primitive_count\":%u,\"vertex_count\":%u,\"ps\":%u,\"vs\":%u,\"rt\":[",d.primitive,d.primitiveCount,d.vertexCount,d.ps,d.vs);for(unsigned k=0;k<4;++k){if(k)std::fputc(',',file);ResourceJson(file,d.rt[k]);}std::fprintf(file,"],\"ds\":");ResourceJson(file,d.ds);
  std::fprintf(file,",\"viewport\":{\"valid\":%s,\"x\":%lu,\"y\":%lu,\"width\":%lu,\"height\":%lu,\"min_z\":%.9g,\"max_z\":%.9g},\"states\":{\"z_enable\":%lu,\"z_write\":%lu,\"z_func\":%lu,\"alpha_test\":%lu,\"alpha_ref\":%lu,\"alpha_func\":%lu,\"blend\":%lu,\"src_blend\":%lu,\"dst_blend\":%lu,\"blend_op\":%lu,\"separate_alpha\":%lu,\"src_blend_alpha\":%lu,\"dst_blend_alpha\":%lu,\"blend_op_alpha\":%lu,\"color_write\":[%lu,%lu,%lu,%lu],\"srgb_write\":%lu,\"cull\":%lu},\"samplers\":[",d.viewportValid?"true":"false",d.viewport.X,d.viewport.Y,d.viewport.Width,d.viewport.Height,static_cast<double>(d.viewport.MinZ),static_cast<double>(d.viewport.MaxZ),d.zEnable,d.zWrite,d.zFunc,d.alphaTest,d.alphaRef,d.alphaFunc,d.blend,d.srcBlend,d.dstBlend,d.blendOp,d.separateAlpha,d.srcBlendAlpha,d.dstBlendAlpha,d.blendOpAlpha,d.colorWrite[0],d.colorWrite[1],d.colorWrite[2],d.colorWrite[3],d.srgbWrite,d.cull);
  for(unsigned k=0;k<16;++k){const auto& s=d.samplers[k];std::fprintf(file,"%s{\"slot\":%u,\"resource\":",k?",":"",k);ResourceJson(file,s.resource);std::fprintf(file,",\"srgb\":%lu,\"address_u\":%lu,\"address_v\":%lu,\"address_w\":%lu,\"min_filter\":%lu,\"mag_filter\":%lu,\"mip_filter\":%lu}",s.srgb,s.u,s.v,s.w,s.min,s.mag,s.mip);}std::fprintf(file,"],\"ps_float_constants_valid\":%s,\"ps_float_constants\":[",d.constantsValid?"true":"false");if(d.constantsValid)for(unsigned k=0;k<224*4;++k){if(k)std::fputc(',',file);FloatJson(file,d.constants[k]);}std::fprintf(file,"],\"vs_c216_c236_valid\":%s,\"vs_c216_c236\":[",d.vsConstantsValid?"true":"false");if(d.vsConstantsValid)for(unsigned k=0;k<84;++k){if(k)std::fputc(',',file);FloatJson(file,d.vsConstants[k]);}std::fprintf(file,"],\"stencil\":{\"enable\":%lu,\"fail\":%lu,\"zfail\":%lu,\"pass\":%lu,\"func\":%lu,\"ref\":%lu,\"mask\":%lu,\"write_mask\":%lu,\"two_sided\":%lu,\"ccw_func\":%lu,\"ccw_pass\":%lu,\"ccw_fail\":%lu,\"ccw_zfail\":%lu},\"callers\":[",d.stencil[0],d.stencil[1],d.stencil[2],d.stencil[3],d.stencil[4],d.stencil[5],d.stencil[6],d.stencil[7],d.stencil[8],d.stencil[9],d.stencil[10],d.stencil[11],d.stencil[12]);for(unsigned k=0;k<d.callerCount;++k)std::fprintf(file,"%s\"%llx\"",k?",":"",static_cast<ULONGLONG>(d.callers[k]));std::fprintf(file,"]}");
 }std::fprintf(file,"]}\n");if(std::ferror(file))error=ERROR_WRITE_FAULT;std::fclose(file);
}
}



