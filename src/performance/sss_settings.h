#pragma once
#include <cwchar>
#include <cwctype>
#include <cstddef>
#include <cstdint>
#if defined(_WIN32)
#include <windows.h>
#include <string>
#endif
namespace sss_settings {
// The INI is the shared launcher/hotkey setting. Scene readiness is separate.
struct Snapshot {bool requested=false,capture=false,requestValid=true,captureValid=true;bool gtao=false,contacts=false,gtaoValid=true,contactsValid=true;};
inline bool Same(const Snapshot& a,const Snapshot& b) noexcept {return a.requested==b.requested&&a.capture==b.capture&&a.requestValid==b.requestValid&&a.captureValid==b.captureValid&&a.gtao==b.gtao&&a.contacts==b.contacts&&a.gtaoValid==b.gtaoValid&&a.contactsValid==b.contactsValid;}
inline bool ParseBoolean(const wchar_t* text,bool& out) noexcept {
 out=false;if(!text)return false;while(*text&&std::iswspace(*text))++text;const wchar_t* end=text+std::wcslen(text);while(end>text&&std::iswspace(end[-1]))--end;
 const auto size=end-text;wchar_t token[6]{};if(size<1||size>5)return false;for(std::ptrdiff_t n=0;n<size;++n)token[n]=static_cast<wchar_t>(std::towlower(text[n]));
 if(!std::wcscmp(token,L"true")||!std::wcscmp(token,L"1")||!std::wcscmp(token,L"on")){out=true;return true;}
 return !std::wcscmp(token,L"false")||!std::wcscmp(token,L"0")||!std::wcscmp(token,L"off");
}
enum class Reason {Disabled,InvalidConfiguration,Unsupported,SceneUnavailable,Active};
struct Decision {bool requested{},enabled{},active{},capture{};Reason reason=Reason::Disabled;};
inline Decision Evaluate(const Snapshot& s,bool supported,bool sceneReady) noexcept {
 Decision out{};out.requested=s.requested;out.capture=s.capture&&s.captureValid;
 if(!s.requestValid){out.reason=Reason::InvalidConfiguration;return out;}
 if(!s.requested)return out;if(!supported){out.reason=Reason::Unsupported;return out;}
 out.enabled=true;if(!sceneReady){out.reason=Reason::SceneUnavailable;return out;}out.active=true;out.reason=Reason::Active;return out;
}
inline const char* ReasonName(Reason reason) noexcept {
 switch(reason){case Reason::Disabled:return "disabled";case Reason::InvalidConfiguration:return "invalid_configuration";case Reason::Unsupported:return "unsupported";case Reason::SceneUnavailable:return "scene_unavailable";case Reason::Active:return "active";}return "unknown";
}
// Parent logs transitions at most once a second; no files/logs are written here.
class DecisionLogGate {
 bool known=false;Decision last{};std::uint64_t lastLog{};
public:
 bool Changed(const Decision& s,std::uint64_t now) noexcept {const bool changed=!known||s.requested!=last.requested||s.enabled!=last.enabled||s.active!=last.active||s.capture!=last.capture||s.reason!=last.reason;if(!changed||(known&&now>=lastLog&&now-lastLog<1000))return false;known=true;last=s;lastLog=now;return true;}
};
#if defined(_WIN32)
class Configuration {
 struct Stamp {bool exists{};DWORD low{},high{},sizeLow{},sizeHigh{};};
 std::wstring path;Snapshot state{};Stamp stamp{};bool checked=false,stamped=false;ULONGLONG lastCheck{};
 static Stamp Metadata(const wchar_t* file) noexcept {WIN32_FILE_ATTRIBUTE_DATA data{};if(!GetFileAttributesExW(file,GetFileExInfoStandard,&data)||(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))return {};return {true,data.ftLastWriteTime.dwLowDateTime,data.ftLastWriteTime.dwHighDateTime,data.nFileSizeLow,data.nFileSizeHigh};}
 static bool Equal(const Stamp& a,const Stamp& b) noexcept {return a.exists==b.exists&&a.low==b.low&&a.high==b.high&&a.sizeLow==b.sizeLow&&a.sizeHigh==b.sizeHigh;}
 static bool Read(const wchar_t* file,const wchar_t* section,const wchar_t* key,bool& value) noexcept {
  wchar_t text[64]{};const auto length=GetPrivateProfileStringW(section,key,L"<missing>",text,64,file);value=false;if(!std::wcscmp(text,L"<missing>"))return true;return length<63&&ParseBoolean(text,value);
 }
public:
 // Configure/Refresh belong on an existing frame/control tick, never per draw.
 void Configure(const wchar_t* file,ULONGLONG now=GetTickCount64()){path=file?file:L"";state={};checked=stamped=false;lastCheck=0;Refresh(now);}
 bool Refresh(ULONGLONG now=GetTickCount64()) noexcept {
  if(checked&&now>=lastCheck&&now-lastCheck<1000)return false;checked=true;lastCheck=now;const auto current=Metadata(path.c_str());if(stamped&&Equal(stamp,current))return false;stamp=current;stamped=true;Snapshot next{};
  if(current.exists){next.requestValid=Read(path.c_str(),L"Graphics",L"SubsurfaceScattering",next.requested);next.captureValid=Read(path.c_str(),L"Diagnostics",L"SssCapture",next.capture);next.gtaoValid=Read(path.c_str(),L"Graphics",L"GTAO",next.gtao);next.contactsValid=Read(path.c_str(),L"Graphics",L"ContactShadows",next.contacts);}
  const bool changed=!Same(state,next);state=next;return changed;
 }
 const Snapshot& Get() const noexcept {return state;}
 // One user-requested key press writes only this key and takes effect this frame.
 // A failed write preserves both the in-memory request and other INI settings.
private:
 bool SetChoice(const wchar_t* key,bool value,ULONGLONG now) noexcept {
  if(path.empty()||!Metadata(path.c_str()).exists)return false;
  if(!WritePrivateProfileStringW(L"Graphics",key,value?L"true":L"false",path.c_str()))return false;
  // Preserve concurrent launcher edits to the other graphics keys as well.
  Snapshot next{};next.requestValid=Read(path.c_str(),L"Graphics",L"SubsurfaceScattering",next.requested);next.captureValid=Read(path.c_str(),L"Diagnostics",L"SssCapture",next.capture);next.gtaoValid=Read(path.c_str(),L"Graphics",L"GTAO",next.gtao);next.contactsValid=Read(path.c_str(),L"Graphics",L"ContactShadows",next.contacts);
  state=next;stamp=Metadata(path.c_str());stamped=checked=true;lastCheck=now;return true;
 }
public:
 bool SetRequested(bool value,ULONGLONG now=GetTickCount64()) noexcept {return SetChoice(L"SubsurfaceScattering",value,now);}
 bool SetGtaoRequested(bool value,ULONGLONG now=GetTickCount64()) noexcept {return SetChoice(L"GTAO",value,now);}
};
#endif
}
