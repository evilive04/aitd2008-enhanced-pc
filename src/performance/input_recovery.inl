// One bounded retry for a foreground keyboard lost on Windows focus changes.
namespace input_recovery {
using StateMethod=HRESULT(STDMETHODCALLTYPE*)(void*,DWORD,void*);
static volatile LONG attempts=0,recovered=0;static DWORD rawKeys=0,rawPolls=0;
static void Sample(void* data,HRESULT result){if(SUCCEEDED(result)&&data){const auto* k=static_cast<const unsigned char*>(data);rawKeys=((k[0x1c]&128)?1u:0u)|((k[0x29]&128)?2u:0u)|((k[0xf]&128)?4u:0u)|((k[1]&128)?8u:0u);++rawPolls;}}static HRESULT lastKeyboardResult=S_OK;
static HRESULT Retry(void* object,DWORD bytes,void* data,HRESULT result,StateMethod original,bool allowed){
 if(!object||!original||!data||bytes!=256)return result;lastKeyboardResult=result;Sample(data,result);
 if(!allowed||(result!=DIERR_INPUTLOST&&result!=DIERR_NOTACQUIRED))return result;
 auto** table=*reinterpret_cast<void***>(object);auto capsMethod=reinterpret_cast<HRESULT(STDMETHODCALLTYPE*)(void*,LPDIDEVCAPS)>(table[3]);
 DIDEVCAPS caps{};caps.dwSize=sizeof(caps);if(FAILED(capsMethod(object,&caps))||GET_DIDEVICE_TYPE(caps.dwDevType)!=DI8DEVTYPE_KEYBOARD)return result;
 auto acquire=reinterpret_cast<HRESULT(STDMETHODCALLTYPE*)(void*)>(table[7]);InterlockedIncrement(&attempts);
 if(FAILED(acquire(object)))return result;result=original(object,bytes,data);lastKeyboardResult=result;Sample(data,result);
 if(SUCCEEDED(result))InterlockedIncrement(&recovered);return result;
}
}
