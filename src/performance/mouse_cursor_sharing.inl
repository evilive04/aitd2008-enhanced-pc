// DirectInput owns clipping/visibility in exclusive mode. Release that ownership
// only while the original game publishes a cursor or a menu, then restore the
// originally requested mode. No desktop/global cursor rectangle is overwritten.
namespace mouse_cursor {
static bool enabled=false;
struct Record {void* object{};HWND window{};DWORD requested{},effective{},attempted{};bool switching{};};
static Record records[32]{};
static SRWLOCK lock=SRWLOCK_INIT;
static volatile LONG transitions=0,failures=0;
static bool Visible(){
 __try{return favoritesCursorActive()||*reinterpret_cast<const DWORD*>(0xff43b4)!=0||(*reinterpret_cast<const DWORD*>(0xfd8bd4)&15)!=0;}
 __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static DWORD Flags(bool visible,DWORD original){
 return visible&&(original&3)==DISCL_EXCLUSIVE?(original&~DISCL_EXCLUSIVE)|DISCL_NONEXCLUSIVE:original;
}
static void Remember(void* object,HWND window,DWORD requested,DWORD effective){
 AcquireSRWLockExclusive(&lock);
 Record* entry=nullptr;
 for(auto& r:records)if(r.object==object){entry=&r;break;}
 if(!entry)for(auto& r:records)if(!r.object){entry=&r;break;}
 if(entry)*entry={object,window,requested,effective,effective,false};
 ReleaseSRWLockExclusive(&lock);
}
static void Update(void* object){
 if(!enabled||!object)return;
 auto** table=*reinterpret_cast<void***>(object);
 auto capsMethod=reinterpret_cast<HRESULT (STDMETHODCALLTYPE*)(void*,LPDIDEVCAPS)>(table[3]);
 DIDEVCAPS caps{};caps.dwSize=sizeof(caps);
 if(FAILED(capsMethod(object,&caps))||GET_DIDEVICE_TYPE(caps.dwDevType)!=DI8DEVTYPE_MOUSE)return;
 Record snapshot{};Record* entry=nullptr;
 AcquireSRWLockExclusive(&lock);
 for(auto& r:records)if(r.object==object){entry=&r;break;}
 if(entry&&!entry->switching){
  const DWORD desired=Flags(Visible(),entry->requested);
  if(desired!=entry->attempted&&GetForegroundWindow()==entry->window&&!IsIconic(entry->window)){
   entry->attempted=desired;entry->switching=true;snapshot=*entry;
  }
 }
 ReleaseSRWLockExclusive(&lock);
 if(!snapshot.object)return;
 auto unacquire=reinterpret_cast<HRESULT (STDMETHODCALLTYPE*)(void*)>(table[8]);
 auto acquire=reinterpret_cast<HRESULT (STDMETHODCALLTYPE*)(void*)>(table[7]);
 auto cooperative=reinterpret_cast<HRESULT (STDMETHODCALLTYPE*)(void*,HWND,DWORD)>(Original(object,13));
 HRESULT result=unacquire(object);DWORD effective=snapshot.effective;
 if(SUCCEEDED(result)){
  result=cooperative(object,snapshot.window,snapshot.attempted);
  if(SUCCEEDED(result))effective=snapshot.attempted;
  else cooperative(object,snapshot.window,snapshot.effective);
  const HRESULT acquired=acquire(object);if(FAILED(acquired)&&SUCCEEDED(result))result=acquired;
 }
 AcquireSRWLockExclusive(&lock);
 if(entry->object==object&&entry->window==snapshot.window&&entry->requested==snapshot.requested){entry->effective=effective;entry->switching=false;}
 ReleaseSRWLockExclusive(&lock);
 if(SUCCEEDED(result))InterlockedIncrement(&transitions);else InterlockedIncrement(&failures);
 Log("mouse_cursor mode requested=0x%lx effective=0x%lx result=0x%08lx",snapshot.attempted,effective,static_cast<unsigned long>(result));
}
} // namespace mouse_cursor
