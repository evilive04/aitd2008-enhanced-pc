// Included inside menu_video. The original game maps only a frame and metadata;
// all Media Foundation decoder allocations live in our disposable x64 child.
struct Player::Impl {
    HANDLE process{},job{},mapping{},mutex{},stop{},changed{},parent{};
    menu_video_ipc::Header* header{};const unsigned char* pixels{};DWORD mappedBytes{};
    HRESULT error{S_OK};Diagnostic diagnostic{};
    void Close(){
        if(pixels)UnmapViewOfFile(pixels);pixels=nullptr;mappedBytes=0;
        if(header)UnmapViewOfFile(header);header=nullptr;
        for(auto* h:{&process,&job,&mapping,&mutex,&stop,&changed,&parent}){if(*h)CloseHandle(*h);*h=nullptr;}
    }
    void Snapshot(){
        if(!header||!mutex)return;
        const DWORD wait=WaitForSingleObject(mutex,0);
        if(wait==WAIT_ABANDONED){error=HRESULT_FROM_WIN32(ERROR_ABANDONED_WAIT_0);ReleaseMutex(mutex);return;}
        if(wait==WAIT_OBJECT_0){diagnostic=header->diagnostic;ReleaseMutex(mutex);}
    }
};
static bool VideoWorkerPath(wchar_t(&path)[MAX_PATH]){
    HMODULE module{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&VideoWorkerPath),&module)||!GetModuleFileNameW(module,path,MAX_PATH))return false;
    auto* slash=wcsrchr(path,L'\\');if(!slash)return false;
    return wcscpy_s(slash,MAX_PATH-static_cast<std::size_t>(slash-path),L"\\AITD Video Worker.exe")==0;
}
Player::Player():impl_(new Impl){}
Player::~Player(){Stop();}
bool Player::Start(const wchar_t* path){
    Stop();impl_->error=S_OK;impl_->diagnostic={};
    if(!path||!*path||wcslen(path)>=MAX_PATH||PathIsURLW(path)||PathIsUNCW(path)||
        GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES||(GetFileAttributesW(path)&FILE_ATTRIBUTE_DIRECTORY)){impl_->error=E_INVALIDARG;return false;}
    wchar_t worker[MAX_PATH]{};if(!VideoWorkerPath(worker)){impl_->error=E_FAIL;return false;}
    // Keep the selected executable open against replacement through startup.
    HANDLE image=CreateFileW(worker,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(image==INVALID_HANDLE_VALUE){impl_->error=HRESULT_FROM_WIN32(GetLastError());return false;}
    SECURITY_ATTRIBUTES security{sizeof security,nullptr,TRUE};
    impl_->mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,&security,PAGE_READWRITE,0,menu_video_ipc::MappingBytes,nullptr);
    impl_->mutex=CreateMutexW(&security,FALSE,nullptr);
    impl_->stop=CreateEventW(&security,TRUE,FALSE,nullptr);
    impl_->changed=CreateEventW(&security,FALSE,FALSE,nullptr);
    if(!DuplicateHandle(GetCurrentProcess(),GetCurrentProcess(),GetCurrentProcess(),&impl_->parent,SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,TRUE,0))impl_->parent=nullptr;
    if(impl_->mapping)impl_->header=static_cast<menu_video_ipc::Header*>(MapViewOfFile(impl_->mapping,FILE_MAP_ALL_ACCESS,0,0,menu_video_ipc::HeaderBytes));
    bool okay=impl_->mapping&&impl_->mutex&&impl_->stop&&impl_->changed&&impl_->parent&&impl_->header;
    if(okay){*impl_->header={};impl_->header->magic=menu_video_ipc::Magic;impl_->header->version=menu_video_ipc::Version;
        impl_->header->capacity=menu_video_ipc::Capacity;impl_->header->parentPid=GetCurrentProcessId();impl_->header->active=1;}
    impl_->job=CreateJobObjectW(nullptr,nullptr);JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    okay=okay&&impl_->job&&SetInformationJobObject(impl_->job,JobObjectExtendedLimitInformation,&limits,sizeof limits);
    SIZE_T bytes{};InitializeProcThreadAttributeList(nullptr,1,0,&bytes);
    std::vector<unsigned char> storage;
    try{storage.resize(bytes);}catch(...){okay=false;}
    auto* attributes=storage.empty()?nullptr:reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
    bool initialized=okay&&InitializeProcThreadAttributeList(attributes,1,0,&bytes)!=FALSE;
    HANDLE handles[]={impl_->mapping,impl_->mutex,impl_->stop,impl_->changed,impl_->parent};
    okay=initialized&&UpdateProcThreadAttribute(attributes,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,handles,sizeof handles,nullptr,nullptr);
    wchar_t command[MAX_PATH*2+256]{};
    // Windows filenames cannot contain a quote. Both arguments are regular
    // absolute .exe/.mp4 paths, so neither can end in an escaping backslash.
    if(wcschr(path,L'"')||swprintf_s(command,L"\"%s\" --aitd-video-worker-v1 %llu %llu %llu %llu %llu \"%s\"",worker,
        static_cast<unsigned long long>(reinterpret_cast<ULONG_PTR>(handles[0])),static_cast<unsigned long long>(reinterpret_cast<ULONG_PTR>(handles[1])),
        static_cast<unsigned long long>(reinterpret_cast<ULONG_PTR>(handles[2])),static_cast<unsigned long long>(reinterpret_cast<ULONG_PTR>(handles[3])),
        static_cast<unsigned long long>(reinterpret_cast<ULONG_PTR>(handles[4])),path)<0)okay=false;
    STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof startup;startup.lpAttributeList=attributes;PROCESS_INFORMATION child{};
    const bool created=okay&&CreateProcessW(worker,command,nullptr,nullptr,TRUE,
        CREATE_NO_WINDOW|CREATE_SUSPENDED|EXTENDED_STARTUPINFO_PRESENT,nullptr,nullptr,&startup.StartupInfo,&child);
    if(created){
        impl_->process=child.hProcess;
        okay=AssignProcessToJobObject(impl_->job,child.hProcess)!=FALSE;
        if(okay)okay=ResumeThread(child.hThread)!=static_cast<DWORD>(-1);
        if(!okay)TerminateProcess(child.hProcess,ERROR_CANCELLED); // This newly created, suspended own helper only.
        CloseHandle(child.hThread);
    }else okay=false;
    const DWORD failure=GetLastError();
    if(initialized)DeleteProcThreadAttributeList(attributes);CloseHandle(image);
    if(!okay){impl_->error=HRESULT_FROM_WIN32(failure?failure:ERROR_INVALID_DATA);impl_->Close();}
    return okay;
}
void Player::Stop(){
    if(impl_->stop)SetEvent(impl_->stop);
    if(impl_->process&&WaitForSingleObject(impl_->process,1500)==WAIT_TIMEOUT){
        if(SUCCEEDED(impl_->error))impl_->error=HRESULT_FROM_WIN32(ERROR_TIMEOUT);
        // A blocked local-media operation cannot hold up the game indefinitely.
        // The job contains only the child created by this Player instance.
        if(impl_->job)TerminateJobObject(impl_->job,ERROR_CANCELLED);
        WaitForSingleObject(impl_->process,1000);
    }
    impl_->Snapshot();
    if(impl_->header){const auto hr=static_cast<HRESULT>(InterlockedCompareExchange(&impl_->header->error,0,0));if(FAILED(hr))impl_->error=hr;}
    impl_->Close();
}
void Player::SetActive(bool active)noexcept{
    if(impl_->header&&impl_->changed&&InterlockedExchange(&impl_->header->active,active?1:0)!=(active?1:0))SetEvent(impl_->changed);
}
bool Player::CopyLatest(Frame& output){
    if(!impl_->header||!impl_->mutex||FAILED(Error()))return false;
    const DWORD wait=WaitForSingleObject(impl_->mutex,0);
    if(wait==WAIT_ABANDONED){impl_->error=HRESULT_FROM_WIN32(ERROR_ABANDONED_WAIT_0);ReleaseMutex(impl_->mutex);return false;}
    if(wait!=WAIT_OBJECT_0)return false;
    if(FAILED(Error())){ReleaseMutex(impl_->mutex);return false;}
    bool copied=false;
    try{
        const auto& h=*impl_->header;impl_->diagnostic=h.diagnostic;
        if(menu_video_ipc::Valid(h)&&h.serial!=output.serial&&menu_video_ipc::FrameValid(h)){
            if(!impl_->pixels||impl_->mappedBytes!=h.bytes){
                if(impl_->pixels)UnmapViewOfFile(impl_->pixels);impl_->pixels=nullptr;impl_->mappedBytes=0;
                impl_->pixels=static_cast<const unsigned char*>(MapViewOfFile(impl_->mapping,FILE_MAP_READ,0,menu_video_ipc::HeaderBytes,h.bytes));
                if(!impl_->pixels)impl_->error=HRESULT_FROM_WIN32(GetLastError());else impl_->mappedBytes=h.bytes;
            }
            if(impl_->pixels){output.pixels.resize(h.bytes);std::memcpy(output.pixels.data(),impl_->pixels,h.bytes);
                output.width=h.width;output.height=h.height;output.matrix=h.matrix;output.nominalRange=h.range;output.chromaSiting=h.chroma;
                output.primaries=h.primaries;output.transfer=h.transfer;output.timestamp100ns=h.timestamp;output.duration100ns=h.duration;
                output.serial=h.serial;output.loop=h.loop;++impl_->header->diagnostic.copied;impl_->diagnostic=impl_->header->diagnostic;copied=true;}
        }
    }catch(const std::bad_alloc&){impl_->error=E_OUTOFMEMORY;}catch(...){impl_->error=E_UNEXPECTED;}
    ReleaseMutex(impl_->mutex);return copied;
}
HRESULT Player::Error()const{
    if(FAILED(impl_->error))return impl_->error;
    if(impl_->header){const auto hr=static_cast<HRESULT>(InterlockedCompareExchange(&impl_->header->error,0,0));if(FAILED(hr))return hr;}
    if(impl_->process&&WaitForSingleObject(impl_->process,0)==WAIT_OBJECT_0)return E_FAIL;
    return S_OK;
}
Diagnostic Player::Diagnostics()const noexcept{impl_->Snapshot();auto result=impl_->diagnostic;result.error=Error();return result;}
