// Local launcher candidate for the exact unmodified protected Steam build.
// Native activation remains in the game; a two-CPU startup mask bounds its old worker pool.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include "../shared/startup_affinity_policy.h"
#include <string>
#include <vector>
#include <algorithm>
#include <cwctype>
#pragma comment(lib, "bcrypt.lib")

#include "../shared/executable_identity.h"

static bool verify(const std::wstring& file,HANDLE* locked=nullptr) {
    if(locked)*locked=INVALID_HANDLE_VALUE;
    HANDLE input = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (input == INVALID_HANDLE_VALUE) return false;
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    bool ok = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0;
    DWORD objectSize = 0, received = 0;
    if (ok) ok = BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize), &received, 0) >= 0;
    std::vector<unsigned char> object(objectSize), buffer(65536);
    if (ok) ok = BCryptCreateHash(algorithm, &hash, object.data(), objectSize, nullptr, 0, 0) >= 0;
    while (ok) {
        DWORD count = 0;
        if (!ReadFile(input, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr)) { ok = false; break; }
        if (!count) break;
        ok = BCryptHashData(hash, buffer.data(), count, 0) >= 0;
    }
    unsigned char digest[32]{};
    if (ok) ok = BCryptFinishHash(hash, digest, sizeof(digest), 0) >= 0;
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    ok=ok&&executable_identity::SupportedDigest(digest,sizeof digest);
    if(ok&&locked)*locked=input;else CloseHandle(input);
    return ok;
}


// Keep the verified file open without write/delete sharing until CreateProcess
// has opened the same pathname. Asset compilation happens before this lock.
struct VerifiedImage {
    HANDLE handle=INVALID_HANDLE_VALUE;
    void Release(){if(handle!=INVALID_HANDLE_VALUE){CloseHandle(handle);handle=INVALID_HANDLE_VALUE;}}
    ~VerifiedImage(){Release();}
};
[[maybe_unused]] static bool StockStartupAffinity(HANDLE child){
    DWORD_PTR allowed{},system{};
    if(!GetProcessAffinityMask(child,&allowed,&system))return false;
    std::vector<startup_affinity::Core> cores;
    USHORT groupCount=1,group{};DWORD bytes{};
    if(GetProcessGroupAffinity(child,&groupCount,&group)&&groupCount==1&&
       !GetLogicalProcessorInformationEx(RelationProcessorCore,nullptr,&bytes)&&
       GetLastError()==ERROR_INSUFFICIENT_BUFFER&&bytes&&bytes<=1024*1024){
        std::vector<BYTE> topology(bytes);
        if(GetLogicalProcessorInformationEx(RelationProcessorCore,
                reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(topology.data()),&bytes)&&bytes<=topology.size()){
            const std::size_t firstMask=offsetof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX,Processor.GroupMask);
            bool valid=true;std::size_t offset=0;
            while(offset<bytes){
                if(bytes-offset<offsetof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX,Processor)){valid=false;break;}
                const auto* record=reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(topology.data()+offset);
                if(record->Size<firstMask||record->Size>bytes-offset||
                   record->Relationship!=RelationProcessorCore||!record->Processor.GroupCount||
                   record->Processor.GroupCount>(record->Size-firstMask)/sizeof(GROUP_AFFINITY)){valid=false;break;}
                for(unsigned i=0;i<record->Processor.GroupCount;++i){
                    const auto& affinity=record->Processor.GroupMask[i];
                    if(affinity.Group==group&&(affinity.Mask&allowed))
                        cores.push_back({affinity.Mask&allowed,record->Processor.EfficiencyClass});
                }
                offset+=record->Size;
            }
            if(!valid)cores.clear();
        }
    }
    const auto selected=startup_affinity::Select(allowed,cores.data(),cores.size());
    return selected&&SetProcessAffinityMask(child,selected)!=FALSE;
}

// Asset packs are declarative data. Only the installed compiler is executed, never pack scripts.
namespace AssetCompile {
static std::wstring Quote(const std::wstring& value) {
    std::wstring result=L"\""; size_t slashes=0;
    for(wchar_t c:value) {
        if(c==L'\\') {++slashes;continue;}
        result.append(slashes*(c==L'"'?2:1),L'\\');slashes=0;
        if(c==L'"')result+=L'\\';
        result+=c;
    }
    result.append(slashes*2,L'\\');result+=L'"';return result;
}
static std::wstring Setting(const std::wstring& config,const wchar_t* key,const wchar_t* fallback=L"") {
    wchar_t value[32768]{};
    GetPrivateProfileStringW(L"Mods",key,fallback,value,32768,config.c_str());return value;
}
static bool File(const std::wstring& path) {
    const DWORD attributes=GetFileAttributesW(path.c_str());
    return attributes!=INVALID_FILE_ATTRIBUTES && !(attributes&FILE_ATTRIBUTE_DIRECTORY);
}
static std::wstring Absolute(const std::wstring& base,const std::wstring& input) {
    if(input.empty())return {};
    std::wstring path=input;
    if(!(path.size()>2 && path[1]==L':') && path.compare(0,2,L"\\\\"))path=base+L"\\"+path;
    wchar_t full[32768]{};
    DWORD n=GetFullPathNameW(path.c_str(),32768,full,nullptr);
    return n && n<32768 ? std::wstring(full,n):L"";
}
static std::wstring Canonical(const std::wstring& path) {
    HANDLE file=CreateFileW(path.c_str(),0,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr);
    if(file==INVALID_HANDLE_VALUE)return {};
    wchar_t full[32768]{};DWORD n=GetFinalPathNameByHandleW(file,full,32768,FILE_NAME_NORMALIZED);
    CloseHandle(file);return n && n<32768?std::wstring(full,n):L"";
}
static bool Under(const std::wstring& path,const std::wstring& parent) {
    return !parent.empty() && path.size()>parent.size() && !_wcsnicmp(path.c_str(),parent.c_str(),parent.size()) && path[parent.size()]==L'\\';
}
static bool HasPacks(const std::wstring& mods) {
    WIN32_FIND_DATAW item{};HANDLE find=FindFirstFileW((mods+L"\\*").c_str(),&item);
    if(find==INVALID_HANDLE_VALUE)return false;
    bool found=false;
    do {
        if(item.cFileName[0]!=L'.' && (item.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) && File(mods+L"\\"+item.cFileName+L"\\mod.json")){found=true;break;}
    }while(FindNextFileW(find,&item));
    FindClose(find);return found;
}
static bool Run(const std::wstring& program,const std::vector<std::wstring>& arguments,const std::wstring& directory,HANDLE log,DWORD& exitCode) {
    std::wstring command=Quote(program);
    for(const auto& arg:arguments)command+=L" "+Quote(arg);
    if(command.size()>=32767)return false;
    SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};
    HANDLE input=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,0,nullptr);
    if(input==INVALID_HANDLE_VALUE)return false;
    SIZE_T bytes=0;InitializeProcThreadAttributeList(nullptr,1,0,&bytes);
    std::vector<unsigned char> storage(bytes);
    auto attributes=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
    if(!InitializeProcThreadAttributeList(attributes,1,0,&bytes)){CloseHandle(input);return false;}
    HANDLE handles[]={input,log};
    bool ok=UpdateProcThreadAttribute(attributes,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,handles,sizeof(handles),nullptr,nullptr)!=FALSE;
    STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof(startup);startup.lpAttributeList=attributes;
    startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;startup.StartupInfo.wShowWindow=SW_HIDE;
    startup.StartupInfo.hStdInput=input;startup.StartupInfo.hStdOutput=log;startup.StartupInfo.hStdError=log;
    PROCESS_INFORMATION child{};
    if(ok)ok=CreateProcessW(program.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|EXTENDED_STARTUPINFO_PRESENT,nullptr,directory.c_str(),&startup.StartupInfo,&child)!=FALSE;
    DeleteProcThreadAttributeList(attributes);CloseHandle(input);
    if(!ok)return false;
    CloseHandle(child.hThread);
    const DWORD wait=WaitForSingleObject(child.hProcess,INFINITE);
    ok=wait==WAIT_OBJECT_0 && GetExitCodeProcess(child.hProcess,&exitCode)!=FALSE;
    CloseHandle(child.hProcess);return ok;
}
static bool PythonWorks(const std::wstring& python,const std::wstring& directory,HANDLE log) {
    auto actual=Canonical(python);std::transform(actual.begin(),actual.end(),actual.begin(),towlower);
    if(actual.empty() || actual.find(L"\\windowsapps\\")!=std::wstring::npos || !File(python))return false;
    DWORD result=~0u;
    // Ignore Python environment/user site and sitecustomize; stdlib compiler siblings remain importable.
    return Run(python,{L"-E",L"-s",L"-S",L"-c",L"import sys; sys.exit(0 if sys.version_info >= (3, 11) else 11)"},directory,log,result) && result==0;
}
static std::wstring FindPython(const std::wstring& directory,const std::wstring& config,HANDLE log) {
    auto configured=Setting(config,L"PythonExecutable");
    if(!configured.empty()) {auto path=Absolute(directory,configured);return PythonWorks(path,directory,log)?path:L"";}
    DWORD n=GetEnvironmentVariableW(L"PATH",nullptr,0);std::vector<wchar_t> buffer(n?n:1);
    if(!n || !GetEnvironmentVariableW(L"PATH",buffer.data(),n))return {};
    std::wstring paths(buffer.data());size_t start=0;
    while(start<paths.size()) {
        auto end=paths.find(L';',start);auto folder=paths.substr(start,end==std::wstring::npos?end:end-start);
        if(folder.size()>1 && folder.front()==L'"' && folder.back()==L'"')folder=folder.substr(1,folder.size()-2);
        // Empty or relative PATH entries must not select a pack-local executable.
        if(folder.size()>2 && (folder[1]==L':' || folder.compare(0,2,L"\\\\")==0))
            for(const auto* name:{L"\\python.exe",L"\\python3.exe"}) {
                auto candidate=folder+name;if(PythonWorks(candidate,directory,log))return candidate;
            }
        if(end==std::wstring::npos)break;start=end+1;
    }
    return {};
}
static bool Prepare(const std::wstring& directory,const std::wstring& config,std::wstring& error) {
    auto enabled=Setting(config,L"AssetOverrides",L"false");
    if(_wcsicmp(enabled.c_str(),L"true") && enabled!=L"1")return true;
    const auto mods=directory+L"\\mods";
    if(!HasPacks(mods) && !File(mods+L"\\.cache\\active.ini"))return true;
    const DWORD attributes=GetFileAttributesW(mods.c_str());
    if(attributes==INVALID_FILE_ATTRIBUTES || (attributes&FILE_ATTRIBUTE_REPARSE_POINT)) {error=L"The mods folder must not be a symbolic link or junction.";return false;}
    const auto script=Absolute(directory,Setting(config,L"CompilerScript",L".enhanced\\tools\\compile_asset_mods.py"));
    const auto actual=Canonical(script),actualMods=Canonical(mods);
    if(actual.empty() || !File(script) || Under(actual,actualMods)) {error=L"The trusted installed asset mod compiler is missing (.enhanced\\tools).";return false;}
    const auto state=directory+L"\\.enhanced";
    if(!CreateDirectoryW(state.c_str(),nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS) {error=L"Could not create the .enhanced folder.";return false;}
    const auto logPath=state+L"\\asset-compile.log";
    SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};
    HANDLE log=CreateFileW(logPath.c_str(),GENERIC_WRITE,FILE_SHARE_READ,&security,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(log==INVALID_HANDLE_VALUE){error=L"Could not write the asset compiler log.";return false;}
    auto python=FindPython(directory,config,log);DWORD result=~0u;
    bool ok=!python.empty() && Run(python,{L"-E",L"-s",L"-S",script,directory,mods},directory,log,result) && result==0;
    CloseHandle(log);
    if(!ok)error=(python.empty()?L"Set [Mods] PythonExecutable to a Python 3.11 or newer executable.":L"Asset mod compilation failed; the game was not started.")+std::wstring(L"\nLog: ")+logPath;
    return ok;
}
} // namespace AssetCompile

#ifndef AITD_LAUNCHER_HELPER_TEST
#include "crash_monitor.h"
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    wchar_t module[32768]{};
    const DWORD length = GetModuleFileNameW(nullptr, module, 32768);
    if (!length || length >= 32768) return 2;
    std::wstring directory(module);
    directory.resize(directory.find_last_of(L"\\/"));
    const std::wstring executable = directory + L"\\Alone.exe";
    if (!verify(executable)) {
        MessageBoxW(nullptr, L"Alone.exe is missing or does not match the supported game build.",
                    L"Enhanced PC - game verification", MB_OK | MB_ICONERROR);
        return 3;
    }
    // Per-child compatibility layer enables pixel-accurate Windows rendering before its window exists.
    // Copy the inherited environment; never alter the user's environment or compatibility registry.
    const std::wstring config=directory+L"\\aitd-enhanced.ini";
    std::wstring compileError;
    if(!AssetCompile::Prepare(directory,config,compileError)) {
        MessageBoxW(nullptr,compileError.c_str(),L"Enhanced PC - asset mods",MB_OK|MB_ICONERROR);return 7;
    }
    wchar_t mode[32]{};
    GetPrivateProfileStringW(L"Display",L"BorderlessFullscreen",L"false",mode,32,config.c_str());
    const bool borderless=!_wcsicmp(mode,L"true") || !wcscmp(mode,L"1");
    std::vector<wchar_t> environment;
    if(borderless) {
        wchar_t general[32]{};
        GetPrivateProfileStringW(L"General",L"Enabled",L"true",general,32,config.c_str());
        if(!_wcsicmp(general,L"true") || !wcscmp(general,L"1")) {
            wchar_t* inherited=GetEnvironmentStringsW();
            if(!inherited)return 6;
            std::wstring layers;
            std::vector<std::wstring> variables;
            for(const wchar_t* item=inherited;*item;item+=wcslen(item)+1) {
                if(!_wcsnicmp(item,L"__COMPAT_LAYER=",15))layers=item+15;
                else variables.emplace_back(item);
            }
            FreeEnvironmentStringsW(inherited);
            const std::wstring layer=L"__COMPAT_LAYER="+layers+L" HIGHDPIAWARE";
            variables.push_back(layer);
            std::sort(variables.begin(),variables.end(),[](const std::wstring& a,const std::wstring& b){return _wcsicmp(a.c_str(),b.c_str())<0;});
            for(const auto& item:variables)environment.insert(environment.end(),item.c_str(),item.c_str()+item.size()+1);
            environment.push_back(0);
        }
    }

    VerifiedImage verified;
    if(!verify(executable,&verified.handle)){
        MessageBoxW(nullptr,L"The original game executable changed during preparation. No game was started.",L"Enhanced PC - game verification",MB_OK|MB_ICONERROR);return 3;
    }
    std::wstring command = L"\"" + executable + L"\"";
    STARTUPINFOW startup{}; startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE,
                        CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT, environment.empty()?nullptr:environment.data(), directory.c_str(), &startup, &process)) {
        MessageBoxW(nullptr, L"Could not start the game.", L"Enhanced PC", MB_OK | MB_ICONERROR);
        return 4;
    }
    verified.Release(); // Child image has been opened; normal game startup may now proceed.
    if(!StockStartupAffinity(process.hProcess)){
        TerminateProcess(process.hProcess,125);CloseHandle(process.hThread);CloseHandle(process.hProcess);
        MessageBoxW(nullptr,L"Could not apply the original-game startup compatibility mask.",L"Enhanced PC",MB_OK|MB_ICONERROR);return 5;
    }
    crash_monitor::Channel crashChannel;
    const bool crashReady=crashChannel.Create(process.dwProcessId);
    bool ok = ResumeThread(process.hThread) != static_cast<DWORD>(-1);
    if (!ok) {
        TerminateProcess(process.hProcess, 125);
        MessageBoxW(nullptr, L"Could not complete compatible game startup.", L"Enhanced PC", MB_OK | MB_ICONERROR);
    }
    if(ok)crash_monitor::Monitor(process,crashChannel);
    (void)crashReady; // A logging IPC failure never blocks normal game startup.
    CloseHandle(process.hThread); CloseHandle(process.hProcess);
    return ok ? 0 : 5;
}

#endif
