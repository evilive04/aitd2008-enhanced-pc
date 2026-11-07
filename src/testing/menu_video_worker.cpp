#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../performance/menu_video.h"
#include "../performance/menu_video_ipc.h"
#include <shellapi.h>
#include <objbase.h>
#include <chrono>
#include <cwchar>
#include <cstring>
#include <new>
#include <limits>
static_assert(sizeof(void*)==8,"The menu decoder worker must be x64");
static_assert(sizeof(menu_video_ipc::Header)==320,"Fixed IPC ABI");
static const volatile wchar_t identity[]=L"AITD Enhanced isolated video worker v1";
namespace {
using Clock=std::chrono::steady_clock;
struct Session {
    HANDLE mapping{},mutex{},stop{},changed{},parent{};
    menu_video_ipc::Header* header{};unsigned char* pixels{};
    std::uint64_t decoded{};
    ~Session(){if(pixels)UnmapViewOfFile(pixels);if(header)UnmapViewOfFile(header);}
    bool Ending()const{HANDLE handles[]={stop,parent};return WaitForMultipleObjects(2,handles,FALSE,0)!=WAIT_TIMEOUT;}
    bool Pause(Clock::time_point& origin){
        if(InterlockedCompareExchange(&header->active,0,0))return true;
        const auto began=Clock::now();HANDLE handles[]={stop,parent,changed};
        while(!InterlockedCompareExchange(&header->active,0,0))if(WaitForMultipleObjects(3,handles,FALSE,INFINITE)!=WAIT_OBJECT_0+2)return false;
        origin+=Clock::now()-began;return true;
    }
    bool Due(Clock::time_point& origin,std::int64_t relative){
        HANDLE handles[]={stop,parent,changed};
        for(;;){
            if(Ending()||!Pause(origin))return false;
            const auto due=origin+std::chrono::nanoseconds(relative*100),now=Clock::now();if(now>=due)return true;
            const auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(due-now).count();
            const DWORD wait=WaitForMultipleObjects(3,handles,FALSE,static_cast<DWORD>(ms>0?ms:1));
            if(wait!=WAIT_TIMEOUT&&wait!=WAIT_OBJECT_0+2)return false;
        }
    }
    void Failure(HRESULT hr,DWORD stage){
        InterlockedExchange(&header->error,hr);
        const DWORD wait=WaitForSingleObject(mutex,100);
        if(wait==WAIT_OBJECT_0||wait==WAIT_ABANDONED){header->diagnostic.error=hr;header->diagnostic.stage=stage;ReleaseMutex(mutex);}
    }
    bool Publish(const menu_video::Frame& f,std::uint64_t loop){
        HANDLE handles[]={stop,parent,mutex};const DWORD wait=WaitForMultipleObjects(3,handles,FALSE,20);
        if(wait==WAIT_TIMEOUT)return true; // Latest-frame playback may skip an already late picture.
        if(wait==WAIT_ABANDONED_0+2){ReleaseMutex(mutex);Failure(HRESULT_FROM_WIN32(ERROR_ABANDONED_WAIT_0),51);return false;}
        if(wait!=WAIT_OBJECT_0+2)return false;
        if(f.pixels.size()>menu_video_ipc::Capacity){ReleaseMutex(mutex);Failure(E_INVALIDARG,50);return false;}
        std::memcpy(pixels,f.pixels.data(),f.pixels.size());
        auto& h=*header;h.width=f.width;h.height=f.height;h.matrix=f.matrix;h.range=f.nominalRange;h.chroma=f.chromaSiting;
        h.primaries=f.primaries;h.transfer=f.transfer;h.bytes=static_cast<DWORD>(f.pixels.size());h.timestamp=f.timestamp100ns;
        h.duration=f.duration100ns;h.loop=loop;++h.serial;h.diagnostic.decoded=decoded;++h.diagnostic.published;h.diagnostic.requestedBytes=f.pixels.size();h.diagnostic.stage=8;
        ReleaseMutex(mutex);return true;
    }
};
HANDLE Handle(const wchar_t* text){
    if(!text||!*text)return nullptr;for(const auto* c=text;*c;++c)if(*c<L'0'||*c>L'9')return nullptr;
    wchar_t* end{};const auto number=_wcstoui64(text,&end,10);if(!number||!end||*end||number>std::numeric_limits<ULONG_PTR>::max())return nullptr;
    HANDLE value=reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(number));DWORD flags{};return GetHandleInformation(value,&flags)?value:nullptr;
}
int Run(int count,wchar_t** args){
    if(count!=8||wcscmp(args[1],L"--aitd-video-worker-v1"))return 2;
    Session s;s.mapping=Handle(args[2]);s.mutex=Handle(args[3]);s.stop=Handle(args[4]);s.changed=Handle(args[5]);s.parent=Handle(args[6]);
    if(!s.mapping||!s.mutex||!s.stop||!s.changed||!s.parent)return 3;
    s.header=static_cast<menu_video_ipc::Header*>(MapViewOfFile(s.mapping,FILE_MAP_ALL_ACCESS,0,0,menu_video_ipc::HeaderBytes));
    if(!s.header||!menu_video_ipc::Valid(*s.header)||GetProcessId(s.parent)!=s.header->parentPid)return 4;
    s.pixels=static_cast<unsigned char*>(MapViewOfFile(s.mapping,FILE_MAP_WRITE,0,menu_video_ipc::HeaderBytes,menu_video_ipc::Capacity));
    if(!s.pixels){s.Failure(HRESULT_FROM_WIN32(GetLastError()),1);return 5;}
    const auto com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);if(FAILED(com)){s.Failure(com,2);return 6;}
    int result=0;
    try{
        menu_video::Decoder decoder;HRESULT hr=decoder.Open(args[7]);
        if(FAILED(hr)){s.Failure(hr,decoder.Stage());result=7;}
        else{
            auto origin=Clock::now();std::int64_t first{},loopEnd{};std::uint64_t loop{};bool have=false;menu_video::Frame frame;
            while(!s.Ending()&&s.Pause(origin)){
                bool end=false;hr=decoder.Read(frame,end);if(FAILED(hr)){s.Failure(hr,decoder.Stage());result=8;break;}
                if(end){if(!have){s.Failure(E_FAIL,30);result=9;break;}origin+=std::chrono::nanoseconds(loopEnd*100);++loop;have=false;loopEnd=0;
                    hr=decoder.Rewind();if(FAILED(hr)){s.Failure(hr,decoder.Stage());result=10;break;}continue;}
                ++s.decoded;if(!have){first=frame.timestamp100ns;have=true;}
                const auto relative=frame.timestamp100ns-first;
                if(relative<0||relative>36000000000ll||frame.duration100ns<=0){s.Failure(E_INVALIDARG,31);result=11;break;}
                loopEnd=relative+frame.duration100ns;
                if(!s.Due(origin,relative)||!s.Publish(frame,loop))break;
            }
        }
    }catch(const std::bad_alloc&){s.Failure(E_OUTOFMEMORY,40);result=12;}catch(...){s.Failure(E_UNEXPECTED,41);result=13;}
    CoUninitialize();return result;
}
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
    // The marker is deliberately retained for packaged-component identification.
    if(identity[0]!=L'A')return 1;
    int count{};auto** args=CommandLineToArgvW(GetCommandLineW(),&count);if(!args)return 1;
    const int result=Run(count,args);LocalFree(args);return result;
}
