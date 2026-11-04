// Exactr29 menu safety: suspend hostile NPAB combat brain/animation delta while
// inventory or healing is open, and consume hero EHIT messages during those menus.
// NPCF-friendly followers, other behavior classes and the world clock remain native.
// Native64D4A0 reads context floats at1D64: inventory9 and healing39.
struct InventorySafetySnapshot {
    bool inventory;
    bool healing;
};

static bool InventorySafetyContextValue(float value) {
    return value > 0.0f && value <= 3.402823466e38F;
}

static InventorySafetySnapshot ReadInventorySafetySnapshot() {
    InventorySafetySnapshot result{};
    __try {
        auto hero=*reinterpret_cast<const BYTE* const*>(0xfd61ec);
        if(!hero||*reinterpret_cast<const DWORD*>(hero)!=0xc834c0)return result;
        result.inventory=InventorySafetyContextValue(*reinterpret_cast<const float*>(hero+0x1d88));
        result.healing=InventorySafetyContextValue(*reinterpret_cast<const float*>(hero+0x1e00));
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        result={};
    }
    return result;
}

static bool InventorySafetyMenuActive() {
    const auto snapshot=ReadInventorySafetySnapshot();
    return snapshot.inventory||snapshot.healing;
}

namespace inventory_safety {
static bool enabled=false;
static volatile LONG installAttempt=0,blockedTicks=0,blockedHits=0;
struct CombatActor {BYTE* actor;BYTE* model;DWORD identity;};
struct CombatActorNode {CombatActor value;CombatActorNode* next;};
static CombatActorNode* actors=nullptr;
static SRWLOCK actorsLock=SRWLOCK_INIT;
static void* brainCall=reinterpret_cast<void*>(0x478b1d);
static void* brainAfter=reinterpret_cast<void*>(0x478b1f);
static void* messageOriginal=reinterpret_cast<void*>(0x64c480);
static void* animationNextA=reinterpret_cast<void*>(0x6017d3);
static void* animationNextB=reinterpret_cast<void*>(0x601ad4);
static bool HasType(const BYTE* object,DWORD type){
    auto descriptor=*reinterpret_cast<const BYTE* const*>(object+0x18);
    for(unsigned depth=0;descriptor&&depth<24;++depth){
        if(*reinterpret_cast<const DWORD*>(descriptor+4)==type)return true;
        auto parent=*reinterpret_cast<const BYTE* const*>(descriptor+0x10);
        if(parent==descriptor)break;
        descriptor=parent;
    }
    return false;
}
static bool FriendlyFollower(const BYTE* actor,const BYTE* hero){
    const int count=*reinterpret_cast<const int*>(actor+0x90);
    auto entries=*reinterpret_cast<const BYTE* const*>(actor+0x94);
    if(count<0||count>256||(!entries&&count))return false;
    for(int i=0;i<count;++i){
        auto behavior=*reinterpret_cast<const BYTE* const*>(entries+i*8);
        if(behavior&&*reinterpret_cast<const BYTE* const*>(behavior+0x20)==actor&&
            HasType(behavior,0x4e504346)&&*reinterpret_cast<const BYTE* const*>(behavior+0x240)==hero)return true;
    }
    return false;
}
static bool CombatBehavior(const BYTE* behavior,const BYTE* hero){
    if(!behavior||!hero||!HasType(behavior,0x4e504142))return false;
    auto actor=*reinterpret_cast<const BYTE* const*>(behavior+0x20);
    return actor&&actor!=hero&&*reinterpret_cast<const BYTE* const*>(actor+0x25c)==behavior&&!FriendlyFollower(actor,hero);
}
static void RecordActor(BYTE* actor){
    auto model=*reinterpret_cast<BYTE**>(actor+0x3b4);
    if(!model)return;
    const DWORD identity=*reinterpret_cast<const DWORD*>(actor+0xc);
    AcquireSRWLockExclusive(&actorsLock);
    CombatActorNode* node=actors;
    while(node&&node->value.model!=model)node=node->next;
    if(!node){
        node=static_cast<CombatActorNode*>(HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(CombatActorNode)));
        if(node){node->next=actors;actors=node;}
    }
    if(node)node->value={actor,model,identity};
    ReleaseSRWLockExclusive(&actorsLock);
}
static bool __cdecl SuspendBehavior(BYTE* behavior){
    if(!enabled)return false;
    __try {
        auto hero=*reinterpret_cast<const BYTE* const*>(0xfd61ec);
        if(!hero||*reinterpret_cast<const DWORD*>(hero)!=0xc834c0||!CombatBehavior(behavior,hero))return false;
        RecordActor(*reinterpret_cast<BYTE**>(behavior+0x20));
        if(!InventorySafetyMenuActive())return false;
        if(InterlockedIncrement(&blockedTicks)<=4)Log("Safe inventory: native hostile combat behavior suspended");
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool __cdecl BlockHeroHit(const BYTE* recipient,const BYTE* message){
    if(!enabled||!InventorySafetyMenuActive())return false;
    __try {
        if(recipient!=*reinterpret_cast<const BYTE* const*>(0xfd61ec)||!recipient||
            *reinterpret_cast<const DWORD*>(recipient)!=0xc834c0||!message||
            *reinterpret_cast<const DWORD*>(message)!=0xc651b0)return false;
        if(InterlockedIncrement(&blockedHits)<=4)Log("Safe inventory: EHIT delivery consumed while menu active");
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static void __cdecl FilterAnimation(const BYTE* model,BYTE* fx){
    if(!enabled||!InventorySafetyMenuActive()||!model||!fx)return;
    CombatActor candidate{};
    AcquireSRWLockShared(&actorsLock);
    for(auto node=actors;node;node=node->next)if(node->value.model==model){candidate=node->value;break;}
    ReleaseSRWLockShared(&actorsLock);
    if(!candidate.actor)return;
    __try {
        auto hero=*reinterpret_cast<const BYTE* const*>(0xfd61ec);
        if(!hero||*reinterpret_cast<const DWORD*>(hero)!=0xc834c0||
            *reinterpret_cast<const DWORD*>(candidate.actor+0xc)!=candidate.identity||
            *reinterpret_cast<const BYTE* const*>(candidate.actor+0x3b4)!=model)return;
        auto behavior=*reinterpret_cast<const BYTE* const*>(candidate.actor+0x25c);
        if(CombatBehavior(behavior,hero))*reinterpret_cast<DWORD*>(fx+160)=0;
    } __except(EXCEPTION_EXECUTE_HANDLER){}
}
static __declspec(naked) void BehaviorGate(){
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        push ecx
        call SuspendBehavior
        add esp,4
        test al,al
        jnz suspended
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        mov edx,[ecx]
        mov eax,[edx+68h]
        jmp dword ptr [brainCall]
    suspended:
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        jmp dword ptr [brainAfter]
    }
}
static __declspec(naked) void HeroMessageGate(){
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        push [ebp+0ch]
        push ecx
        call BlockHeroHit
        add esp,8
        test al,al
        jnz consumed
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        jmp dword ptr [messageOriginal]
    consumed:
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        mov eax,1
        ret 10h
    }
}
static __declspec(naked) void AnimationGateA(){
    __asm {
        movss xmm0,[ebx+34h]
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        mov eax,esp
        push eax
        push ebx
        call FilterAnimation
        add esp,8
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        jmp dword ptr [animationNextA]
    }
}
static __declspec(naked) void AnimationGateB(){
    __asm {
        movss xmm0,[edi+34h]
        push ebp
        mov ebp,esp
        pushfd
        pushad
        sub esp,528
        and esp,0fffffff0h
        fxsave [esp]
        mov eax,esp
        push eax
        push edi
        call FilterAnimation
        add esp,8
        fxrstor [esp]
        lea esp,[ebp-36]
        popad
        popfd
        mov esp,ebp
        pop ebp
        jmp dword ptr [animationNextB]
    }
}
}
static void ConfigureInventorySafety(const wchar_t* config,bool supported){
    inventory_safety::enabled=supported&&ConfigFlag(L"Gameplay",L"SafeInventory",true,config);
}
static void InstallInventorySafety(){
    using namespace inventory_safety;
    if(!inventory_safety::enabled||InterlockedCompareExchange(&installAttempt,1,0))return;
    struct Patch {DWORD address;unsigned size;BYTE expected[5];void* target;bool pointer;DWORD protection;};
    Patch patches[]={
        {0x478b18,5,{0x8b,0x11,0x8b,0x42,0x68},reinterpret_cast<void*>(&BehaviorGate),false,0},
        {0xc835dc,4,{0x80,0xc4,0x64,0},reinterpret_cast<void*>(&HeroMessageGate),true,0},
        {0x6017ce,5,{0xf3,0x0f,0x10,0x43,0x34},reinterpret_cast<void*>(&AnimationGateA),false,0},
        {0x601acf,5,{0xf3,0x0f,0x10,0x47,0x34},reinterpret_cast<void*>(&AnimationGateB),false,0}};
    for(const auto& p:patches)if(memcmp(reinterpret_cast<void*>(p.address),p.expected,p.size)){
        inventory_safety::enabled=false;Log("Safe inventory signature mismatch at%08lx; no hooks installed",p.address);return;}
    unsigned writable=0;
    for(auto& p:patches){if(!VirtualProtect(reinterpret_cast<void*>(p.address),p.size,PAGE_EXECUTE_READWRITE,&p.protection))break;++writable;}
    if(writable!=4){
        for(int i=static_cast<int>(writable)-1;i>=0;--i){DWORD ignored;VirtualProtect(reinterpret_cast<void*>(patches[i].address),patches[i].size,patches[i].protection,&ignored);}
        inventory_safety::enabled=false;Log("Safe inventory page access failed; no hooks installed");return;
    }
    for(const auto& p:patches){
        auto site=reinterpret_cast<BYTE*>(p.address);
        if(p.pointer)InterlockedExchangePointer(reinterpret_cast<void* volatile*>(site),p.target);
        else {site[0]=0xe9;*reinterpret_cast<DWORD*>(site+1)=static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(p.target)-p.address-5);}
        FlushInstructionCache(GetCurrentProcess(),site,p.size);
    }
    for(int i=3;i>=0;--i){DWORD ignored;VirtualProtect(reinterpret_cast<void*>(patches[i].address),patches[i].size,patches[i].protection,&ignored);}
    Log("Safe inventory hooks installed: hostile NPAB combat+animation suspended; hero EHIT consumed only in inventory/healing; world clock unchanged");
}
