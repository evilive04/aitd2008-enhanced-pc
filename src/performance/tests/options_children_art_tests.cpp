#define NOMINMAX
#include "../options_children_art.h"
#include <wincodec.h>
#include <wrl/client.h>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <limits>
using namespace options_children_art;
using Microsoft::WRL::ComPtr;
static unsigned checks;
static void Check(bool x){++checks;if(!x){std::printf("FAIL check %u\n",checks);std::abort();}}
template<size_t N>void Copy(wchar_t (&out)[N],const wchar_t* s){wcscpy_s(out,s);}
static View Fixture(Page page){
 if(page==Page::PhoneMessages||page==Page::PhoneContacts){
  View v;v.page=page;v.count=page==Page::PhoneMessages?5:4;v.selected=0;
  Copy(v.title,page==Page::PhoneMessages?L"MESSAGES":L"CONTACTS");
  const wchar_t* names[]={L"Hints",L"Dr. Hartford",L"Sarah",L"Crowley",L"Documents"};
  const wchar_t* routes[]={L"tips",L"hartford",L"sarah",L"bungle",L"documents"};
  for(unsigned n=0;n<v.count;++n){v.rows[n].kind=Kind::Navigation;Copy(v.rows[n].label,names[n]);Copy(v.rows[n].value,routes[n]);}
  v.footer[0].action=Action::Confirm;Copy(v.footer[0].label,L"Select");Copy(v.footer[0].key,L"Enter");
  v.footer[1].action=Action::Cancel;Copy(v.footer[1].label,L"Back");Copy(v.footer[1].key,L"Esc");return v;
 }
 if(page==Page::PhoneParameters){
  View v;v.page=page;v.count=4;v.selected=0;Copy(v.title,L"PARAMETERS");
  const wchar_t* names[]={L"Ring settings",L"Map display",L"Map rotation",L"Map transparency"};
  for(unsigned n=0;n<4;++n){Copy(v.rows[n].label,names[n]);Copy(v.rows[n].value,n==0?L"Ring":L"Enabled");}
  v.rows[3].kind=Kind::Gauge;v.rows[3].minimum=0;v.rows[3].maximum=1;v.rows[3].current=.5;
  v.footer[0].action=Action::Confirm;Copy(v.footer[0].label,L"OK");Copy(v.footer[0].key,L"Enter");
  v.footer[1].action=Action::Cancel;Copy(v.footer[1].label,L"Cancel");Copy(v.footer[1].key,L"Esc");return v;
 }
 View v;v.page=page;v.count=page==Page::Audio?3:page==Page::Languages?1:7;v.selected=0;
 Copy(v.title,page==Page::Audio?L"AUDIO":page==Page::Languages?L"LANGUAGES":L"DISPLAY");
 const wchar_t* names[]={L"Resolution",L"Details",L"Vegetation",L"Light Clip Distance",L"FX",L"Shadows",L"HDR"};
 for(unsigned n=0;n<v.count;++n){Copy(v.rows[n].label,page==Page::Audio?(n==0?L"Master Volume":n==1?L"Music":L"Audio Quality"):page==Page::Languages?L"Language":names[n]);Copy(v.rows[n].value,page==Page::Languages?L"Čeština":page==Page::Display&&n==0?L"3840 × 2160":page==Page::Audio&&n==1?L"ON":page==Page::Display&&n==1?L"Custom":page==Page::Display&&n==6?L"Enabled":L"High");}
 for(unsigned n=0;n<v.count;++n)if((page==Page::Audio&&n==0)||(page==Page::Display&&(n==2||n==3))){v.rows[n].kind=Kind::Gauge;v.rows[n].minimum=11;v.rows[n].maximum=31;v.rows[n].current=26;}
 Copy(v.infoTitle,L"INFORMATION");Copy(v.information,L"Localized information supplied by the native game.\nThe renderer does not invent setting descriptions.");
 const Action actions[]={Action::Reset,Action::Calibration,Action::Confirm,Action::Cancel};const wchar_t* labels[]={L"Reset",L"Screen calibration",L"OK",L"Cancel"};const wchar_t* keys[]={L"X",L"C",L"Enter",L"Esc"};
 for(unsigned n=0;n<4;++n){if((page==Page::Languages&&n<2)||(page==Page::Audio&&n==1))continue;v.footer[n].action=actions[n];Copy(v.footer[n].label,labels[n]);Copy(v.footer[n].key,keys[n]);}
 return v;
}
static HRESULT Save(IDirect3DDevice9* d,const std::filesystem::path& file){
 ComPtr<IDirect3DSurface9> target,copy;D3DSURFACE_DESC desc{};HRESULT hr=d->GetRenderTarget(0,&target);
 if(SUCCEEDED(hr))hr=target->GetDesc(&desc);if(SUCCEEDED(hr))hr=d->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&copy,nullptr);if(SUCCEEDED(hr))hr=d->GetRenderTargetData(target.Get(),copy.Get());
 D3DLOCKED_RECT lock{};bool locked=false;if(SUCCEEDED(hr)){hr=copy->LockRect(&lock,nullptr,D3DLOCK_READONLY);locked=SUCCEEDED(hr);}
 ComPtr<IWICImagingFactory> f;ComPtr<IWICStream> stream;ComPtr<IWICBitmapEncoder> encoder;ComPtr<IWICBitmapFrameEncode> frame;
 if(SUCCEEDED(hr))hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&f));if(SUCCEEDED(hr))hr=f->CreateStream(&stream);if(SUCCEEDED(hr))hr=stream->InitializeFromFilename(file.c_str(),GENERIC_WRITE);if(SUCCEEDED(hr))hr=f->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder);if(SUCCEEDED(hr))hr=encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache);if(SUCCEEDED(hr))hr=encoder->CreateNewFrame(&frame,nullptr);if(SUCCEEDED(hr))hr=frame->Initialize(nullptr);if(SUCCEEDED(hr))hr=frame->SetSize(desc.Width,desc.Height);
 WICPixelFormatGUID fmt=GUID_WICPixelFormat32bppBGRA;if(SUCCEEDED(hr))hr=frame->SetPixelFormat(&fmt);if(SUCCEEDED(hr)&&fmt!=GUID_WICPixelFormat32bppBGRA)hr=E_FAIL;if(SUCCEEDED(hr))hr=frame->WritePixels(desc.Height,static_cast<UINT>(lock.Pitch),static_cast<UINT>(lock.Pitch)*desc.Height,static_cast<BYTE*>(lock.pBits));if(SUCCEEDED(hr))hr=frame->Commit();if(SUCCEEDED(hr))hr=encoder->Commit();if(locked)copy->UnlockRect();return hr;
}
int wmain(int argc,wchar_t** argv){
 ui_art::Scene scene;Layout layout;
 for(auto page:{Page::Audio,Page::Languages,Page::Display,Page::PhoneParameters,Page::PhoneMessages,Page::PhoneContacts}){
  auto v=Fixture(page);Check(SUCCEEDED(Build(v,scene,layout)));Check(layout.count>0&&layout.count<=40);Check(!scene.batches.empty());bool slate=false,roots=false;
  for(const auto& b:scene.batches){slate|=b.material==ui_art::Material::Slate;roots|=b.material==ui_art::Material::Roots;Check(b.vertices.size()%3==0);bool finite=true;for(const auto& p:b.vertices)finite=finite&&std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.u)&&std::isfinite(p.v);Check(finite);}Check(!slate&&!roots);
  for(unsigned n=0;n<v.count;++n){const auto b=RowBounds(page,n);auto h=HitTest(layout,b.x+50,b.y+20);Check(h.action==(v.rows[n].kind==Kind::Navigation?Action::Activate:Action::FocusRow)&&h.row==static_cast<int>(n));}
  Check(HitTest(layout,-100,-100).action==Action::None);Check(HitTest(layout,std::numeric_limits<float>::quiet_NaN(),0).action==Action::None);
  v.rows[0].locked=1;Check(SUCCEEDED(Build(v,scene,layout)));const auto b=RowBounds(page,0);Check(HitTest(layout,b.x+50,b.y+20).action==Action::None);
  v.count++;Check(Build(v,scene,layout)==E_INVALIDARG&&scene.batches.empty()&&!layout.count);
 }
 for(auto page:{Page::PhoneMessages,Page::PhoneContacts}){auto roster=Fixture(page);const auto portrait=RosterPortrait();Check(portrait.w>0&&portrait.h>0);for(unsigned n=0;n<roster.count;++n){const auto row=RowBounds(page,n);Check(row.x+row.w<portrait.x);}Check(SUCCEEDED(Build(roster,scene,layout)));Check(HitTest(layout,portrait.x+portrait.w*.5f,portrait.y+portrait.h*.5f).action==Action::None);}
 auto v=Fixture(Page::Audio);Check(SUCCEEDED(Build(v,scene,layout)));const float cy=RowBounds(Page::Audio,0).y+54;auto hit=HitTest(layout,1095+330*.75f,cy);Check(hit.action==Action::Gauge&&std::fabs(hit.normalized-.75f)<.001f);Check(HitTest(layout,1080,cy).action==Action::Previous);Check(HitTest(layout,1450,cy).action==Action::Next);
 v.rows[0].current=40;Check(Build(v,scene,layout)==E_INVALIDARG);v=Fixture(Page::Audio);v.rows[0].maximum=v.rows[0].minimum;Check(Build(v,scene,layout)==E_INVALIDARG);v=Fixture(Page::Audio);v.rows[0].minimum=std::numeric_limits<double>::quiet_NaN();Check(Build(v,scene,layout)==E_INVALIDARG);v=Fixture(Page::Audio);v.version=2;Check(Build(v,scene,layout)==E_INVALIDARG);v=Fixture(Page::Audio);for(auto& c:v.title)c=L'a';Check(Build(v,scene,layout)==E_INVALIDARG);v=Fixture(Page::Audio);v.footer[0].key[0]=0;Check(Build(v,scene,layout)==E_INVALIDARG);

 // Real font fitting: moderate wide strings must shrink actual type, never warp.
 v=Fixture(Page::Audio);for(unsigned n=0;n<40;++n)v.title[n]=L'W';v.title[40]=0;Check(SUCCEEDED(Build(v,scene,layout)));Check(layout.text.title>=40&&layout.text.title<64);
 v=Fixture(Page::Audio);Copy(v.rows[0].label,L"WWWWWWWWWWWWWWWWWWWWWW");Check(SUCCEEDED(Build(v,scene,layout)));Check(layout.text.row[0]>=28&&layout.text.row[0]<42);
 v=Fixture(Page::Audio);Copy(v.rows[1].value,L"WWWWWWWWWW");Check(SUCCEEDED(Build(v,scene,layout)));Check(layout.text.value[1]>=28&&layout.text.value[1]<38);
 v=Fixture(Page::Audio);Copy(v.footer[0].label,L"WWWWWWWWWWW");Check(SUCCEEDED(Build(v,scene,layout)));Check(layout.text.footer[0]>=22&&layout.text.footer[0]<28);
 v=Fixture(Page::Audio);Copy(v.footer[0].key,L"WWWWWWW");Check(SUCCEEDED(Build(v,scene,layout)));Check(layout.text.key[0]>=18&&layout.text.key[0]<23);
 v=Fixture(Page::Languages);Copy(v.rows[0].value,L"WWWWWWWWWWWWWWWW");Check(SUCCEEDED(Build(v,scene,layout)));Check(layout.text.value[0]>=36&&layout.text.value[0]<56);
 v=Fixture(Page::Display);Copy(v.rows[0].label,L"WWWWWWWWWWWWWWWWWW");Check(SUCCEEDED(Build(v,scene,layout)));Check(layout.text.row[0]>=26&&layout.text.row[0]<34);
 v=Fixture(Page::Display);Copy(v.rows[0].label,L"Rozlišení obrazovky – úprava");Copy(v.rows[0].value,L"3840 × 2160");Check(SUCCEEDED(Build(v,scene,layout)));
 v=Fixture(Page::Display);v.information[0]=0;for(unsigned n=0;n<14;++n)wcscat_s(v.information,L"Localized information.\n");wcscat_s(v.information,L"Final line.");Check(SUCCEEDED(Build(v,scene,layout)));Check(layout.text.information>=26&&layout.text.information<30);
 const auto overflow=[&](const View& input){Check(FAILED(Build(input,scene,layout)));Check(scene.batches.empty()&&!layout.count&&!layout.text.title);};
 v=Fixture(Page::Audio);for(unsigned n=0;n<95;++n)v.title[n]=L'W';v.title[95]=0;overflow(v);
 v=Fixture(Page::Audio);for(unsigned n=0;n<127;++n)v.rows[0].label[n]=L'W';v.rows[0].label[127]=0;overflow(v);
 v=Fixture(Page::Audio);for(unsigned n=0;n<127;++n)v.rows[1].value[n]=L'W';v.rows[1].value[127]=0;overflow(v);
 v=Fixture(Page::Audio);for(unsigned n=0;n<63;++n)v.footer[0].label[n]=L'W';v.footer[0].label[63]=0;overflow(v);
 v=Fixture(Page::Audio);Copy(v.rows[0].label,L"Two\nlines");overflow(v);
 v=Fixture(Page::Languages);Copy(v.rows[0].value,L"Language\nname");overflow(v);
 v=Fixture(Page::Audio);Copy(v.footer[0].key,L"Two\nlines");overflow(v);
 v=Fixture(Page::Audio);v.rows[0].label[0]=0xd800;v.rows[0].label[1]=0;overflow(v);
 v=Fixture(Page::Languages);for(unsigned n=0;n<127;++n)v.rows[0].value[n]=L'W';v.rows[0].value[127]=0;overflow(v);
 v=Fixture(Page::Display);for(unsigned n=0;n<100;++n){v.information[n*2]=L'A';v.information[n*2+1]=L'\n';}v.information[200]=0;overflow(v);
 v=Fixture(Page::Display);for(unsigned n=0;n<767;++n)v.information[n]=L'W';v.information[767]=0;overflow(v);
 v=Fixture(Page::Display);Copy(v.information,L"Řádek jedna.\nPříliš žluťoučký kůň úpěl ďábelské ódy.\nInformation remains inside its own panel.");Check(SUCCEEDED(Build(v,scene,layout)));Check(layout.text.information>=26&&layout.text.information<=30);

 // Gauge-only refresh leaves text, every other vertex attribute and hit layout intact.
 const auto sameVertex=[](const ui_art::Vertex& a,const ui_art::Vertex& b){return a.x==b.x&&a.y==b.y&&a.z==b.z&&a.rhw==b.rhw&&a.color==b.color&&a.u==b.u&&a.v==b.v;};
 const auto sameScene=[&](const ui_art::Scene& a,const ui_art::Scene& b){if(a.batches.size()!=b.batches.size())return false;for(size_t i=0;i<a.batches.size();++i){if(a.batches[i].material!=b.batches[i].material||a.batches[i].vertices.size()!=b.batches[i].vertices.size())return false;for(size_t j=0;j<a.batches[i].vertices.size();++j)if(!sameVertex(a.batches[i].vertices[j],b.batches[i].vertices[j]))return false;}return true;};
 View cached=Fixture(Page::Display);Check(SUCCEEDED(Build(cached,scene,layout)));auto priorScene=scene;View changed=cached;changed.rows[2].current=changed.rows[2].minimum;changed.rows[3].current=changed.rows[3].maximum;Check(UpdateGaugeCurrents(cached,changed,scene,layout));
 unsigned changedVertices=0;bool staticExact=true;
 for(size_t b=0;b<scene.batches.size();++b)for(size_t i=0;i<scene.batches[b].vertices.size();++i){const auto& before=priorScene.batches[b].vertices[i];const auto& after=scene.batches[b].vertices[i];if(!sameVertex(before,after)){++changedVertices;bool knob=false;for(unsigned n:{2u,3u}){const auto& k=layout.knobs[n];knob|=k.batch==b&&i>=k.firstVertex&&i<k.firstVertex+6;}staticExact=staticExact&&knob&&before.y==after.y&&before.z==after.z&&before.rhw==after.rhw&&before.color==after.color&&before.u==after.u&&before.v==after.v;}}
 Check(changedVertices==12&&staticExact);Check(scene.batches[layout.knobs[2].batch].vertices[layout.knobs[2].firstVertex].x==805.f);Check(scene.batches[layout.knobs[3].batch].vertices[layout.knobs[3].firstVertex].x==1131.f);
 cached=changed;changed.rows[2].current=21;changed.rows[3].current=16;Check(UpdateGaugeCurrents(cached,changed,scene,layout));Check(std::fabs(scene.batches[layout.knobs[2].batch].vertices[layout.knobs[2].firstVertex].x-968.f)<.001f);
 cached=changed;priorScene=scene;Check(UpdateGaugeCurrents(cached,cached,scene,layout)&&sameScene(priorScene,scene));
 // Every failed cache refresh is transactional, even with a valid first gauge.
 const auto rejected=[&](View next,Layout badLayout){const auto prior=scene;Check(!UpdateGaugeCurrents(cached,next,scene,badLayout));Check(sameScene(prior,scene));};
 changed=cached;changed.rows[2].current=19;changed.rows[3].current=99;rejected(changed,layout);
 changed=cached;changed.rows[2].minimum=1;rejected(changed,layout);
 changed=cached;changed.rows[2].maximum=100;rejected(changed,layout);
 changed=cached;changed.rows[2].current=std::numeric_limits<double>::quiet_NaN();rejected(changed,layout);
 changed=cached;Copy(changed.rows[0].label,L"Changed label");rejected(changed,layout);
 changed=cached;Copy(changed.rows[0].value,L"Changed selector");rejected(changed,layout);
 changed=cached;Copy(changed.title,L"Changed title");rejected(changed,layout);
 changed=cached;Copy(changed.information,L"Changed description");rejected(changed,layout);
 changed=cached;Copy(changed.footer[0].key,L"R");rejected(changed,layout);
 changed=cached;changed.rows[2].locked=1;rejected(changed,layout);
 changed=cached;changed.rows[2].kind=Kind::Selector;rejected(changed,layout);
 changed=cached;changed.selected=1;rejected(changed,layout);
 changed=cached;changed.page=Page::Audio;rejected(changed,layout);
 auto broken=layout;broken.knobs[2].trackWidth=1;rejected(cached,broken);
 broken=layout;broken.knobs[2].firstVertex=0xffffffff;rejected(cached,broken);
 broken=layout;broken.knobs[2].batch=0xffffffff;rejected(cached,broken);
 const auto pristine=scene;scene.batches[layout.knobs[3].batch].vertices[layout.knobs[3].firstVertex].x+=1;changed=cached;changed.rows[2].current=20;rejected(changed,layout);scene=pristine;
 changed=cached;changed.rows[0].label[wcslen(changed.rows[0].label)+1]=L'Z';Check(UpdateGaugeCurrents(cached,changed,scene,layout)); // Ignore unused trailing UTF16 storage.
 if(argc==3){
  Check(SUCCEEDED(CoInitializeEx(nullptr,COINIT_MULTITHREADED)));const auto instance=GetModuleHandleW(nullptr);WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=instance;wc.lpszClassName=L"AITDOptionsChildrenArtCheck";Check(RegisterClassW(&wc)!=0);HWND window=CreateWindowW(wc.lpszClassName,L"",WS_OVERLAPPEDWINDOW,0,0,640,480,nullptr,nullptr,instance,nullptr);Check(window!=nullptr);
  ComPtr<IDirect3D9> d3d;d3d.Attach(Direct3DCreate9(D3D_SDK_VERSION));Check(d3d!=nullptr);D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;pp.BackBufferWidth=1920;pp.BackBufferHeight=1080;pp.BackBufferFormat=D3DFMT_X8R8G8B8;ComPtr<IDirect3DDevice9> device;Check(SUCCEEDED(d3d->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&device)));ui_art::Assets assets;Check(SUCCEEDED(assets.Load(device.Get(),argv[1])));std::filesystem::create_directories(argv[2]);
  for(auto page:{Page::Audio,Page::Languages,Page::Display}){Check(SUCCEEDED(Build(Fixture(page),scene,layout)));Check(ui_art::Draw(device.Get(),scene,assets,1));Check(SUCCEEDED(Save(device.Get(),std::filesystem::path(argv[2])/(page==Page::Audio?L"audio-1920.png":page==Page::Languages?L"languages-1920.png":L"display-1920.png"))));}
  ComPtr<IDirect3DSurface9> large,previous;Check(SUCCEEDED(device->GetRenderTarget(0,&previous)));Check(SUCCEEDED(device->CreateRenderTarget(3840,2160,D3DFMT_X8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&large,nullptr)));Check(SUCCEEDED(device->SetRenderTarget(0,large.Get())));Check(ui_art::Draw(device.Get(),scene,assets,2));Check(SUCCEEDED(device->SetRenderTarget(0,previous.Get())));assets.Reset();device.Reset();d3d.Reset();DestroyWindow(window);UnregisterClassW(wc.lpszClassName,instance);CoUninitialize();
 }
 std::printf("PASS %u grouped assertions: layout/hits, domain/UTF16 negatives, typography fitting/overflow; standalone art, no game connection\n",checks);return 0;
}
