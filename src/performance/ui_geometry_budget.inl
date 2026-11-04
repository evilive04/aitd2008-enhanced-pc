// Exact-r29 graphics configuration, before the native display-list allocator.
// Both double-buffered CPU arrays and native VB/IB creation read this same
// transient graphics object. Never raise capacity fields after allocation.
namespace ui_geometry_budget {
static volatile LONG installed=0,adjusted=0;
static void* getter=reinterpret_cast<void*>(0x405d20);
static constexpr DWORD vertexBytes=32u*1024u*1024u,indexBytes=8u*1024u*1024u,jobs=8192;
static void __cdecl Prepare(DWORD object){
    __try{
        if(!object||object!=*reinterpret_cast<DWORD*>(0x1083608)||
           *reinterpret_cast<DWORD*>(object)!=0xcc68e0)return;
        auto* counts=reinterpret_cast<DWORD*>(object);
        const DWORD priorJobs=counts[0x70/4],priorVertices=counts[0x78/4],priorIndices=counts[0x80/4];
        if(!priorJobs||priorJobs>65535||!priorVertices||priorVertices>256u*1024u*1024u||
           !priorIndices||priorIndices>64u*1024u*1024u)return;
        if(priorJobs<jobs)counts[0x70/4]=jobs;
        if(priorVertices<vertexBytes)counts[0x78/4]=vertexBytes;
        if(priorIndices<indexBytes)counts[0x80/4]=indexBytes;
        InterlockedIncrement(&adjusted);
        Log("Native UI allocation config jobs=%lu->%lu vertex_bytes=%lu->%lu index_bytes=%lu->%lu; before CPU/GPU allocation",
            priorJobs,counts[0x70/4],priorVertices,counts[0x78/4],priorIndices,counts[0x80/4]);
    }__except(EXCEPTION_EXECUTE_HANDLER){}
}
static __declspec(naked) void Adapter(){__asm{
    call dword ptr [getter]
    pushfd
    pushad
    mov esi,esp
    sub esp,528
    and esp,0fffffff0h
    fxsave [esp]
    push dword ptr [esi+28]
    call Prepare
    add esp,4
    fxrstor [esp]
    mov esp,esi
    popad
    popfd
    ret
}}
static bool Install(bool exactEnabled){
    if(!exactEnabled)return false;
    if(InterlockedCompareExchange(&installed,0,0))return true;
    auto* site=reinterpret_cast<BYTE*>(0x86da29);
    const BYTE expected[]={0xe8,0xf2,0x82,0xb9,0xff};
    if(memcmp(site,expected,sizeof(expected))){Log("Native UI allocation signature mismatch");return false;}
    DWORD old{};if(!VirtualProtect(site,5,PAGE_EXECUTE_READWRITE,&old))return false;
    BYTE replacement[5]{0xe8,0,0,0,0};const auto displacement=static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(&Adapter)-reinterpret_cast<ULONG_PTR>(site+5));
    memcpy(replacement+1,&displacement,4);memcpy(site,replacement,5);
    FlushInstructionCache(GetCurrentProcess(),site,5);DWORD ignored{};VirtualProtect(site,5,old,&ignored);
    InterlockedExchange(&installed,1);return true;
}
}
