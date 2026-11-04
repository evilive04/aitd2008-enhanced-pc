// Included in phone_content_context. Native digits and call lifecycle only.
static bool OptionalText(DWORD root,DWORD scope,const char* name,wchar_t* out,int cap){
 dialog_text::Value v{};if(!phone_context::Value(root,scope,name,v)||v.kind!=dialog_text::Kind::String)return false;
 int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,v.text,-1,out,cap);if(!n)n=MultiByteToWideChar(CP_ACP,0,v.text,-1,out,cap);return n!=0;
}
static bool CaptureTelephony(DWORD mode,unsigned char* movie,PhoneContentStateV1& out,Owner& owner){
 auto* handle=reinterpret_cast<unsigned char*>(Word(movie,0x24));const DWORD root=Word(handle),scope=root+200;
 DWORD platform{},released{},panel{};const auto sub=Word(reinterpret_cast<void*>(0xff4b8c));
 if(!phone_context::OptionalZero(root,scope,"fake")||!phone_context::Number(root,scope,"WhatPlatform",platform,1)||platform!=1||!phone_context::Number(root,scope,"release_button",released,1))return false;
 auto& v=out.view;
 if((mode==5&&sub==5)||(mode==4&&sub==8)){
  if(!phone_context::Equal(root,scope,"whatpage","calling"))return false;
  if(mode==5){if(!phone_context::Resolve(handle,"/menu_index",575,panel)||!phone_timeline_policy::ContactCallPaint(Word(reinterpret_cast<void*>(panel),0x60)))return false;}
  else{
   // Numeric calls replace menu_call entirely. Actual supported runtime stops
   // on root frame333; it is not the dialer's to_calling/frame43 branch.
   panel=scope;if(Word(reinterpret_cast<void*>(panel),0x60)!=333)return false;
  }
  if(mode==5){if(Word(reinterpret_cast<void*>(panel),0x60)!=97||Word(reinterpret_cast<void*>(panel),0x64)!=2||*reinterpret_cast<BYTE*>(panel+0x68)||released)out.flags|=PhoneContentBusy;}
  else if(Word(reinterpret_cast<void*>(panel),0x64)!=2||*reinterpret_cast<BYTE*>(panel+0x68))return false;
  v.kind=Kind::Call;
  if(!phone_parameters_context::Text(root,scope,"m_calling",v.title,96)||!OptionalText(root,scope,"name_calling",v.subtitle,256)||!OptionalText(root,scope,"txt_calling",v.body,4096)||!phone_parameters_context::Text(root,scope,"m_Back",v.back,64)||!phone_parameters_context::Text(root,scope,"m_call",v.open,64))return false;
 }else if(mode==4&&sub==6){
  DWORD digits{},maximum{},first{};
  if(!phone_context::Equal(root,scope,"whatpage","phone")||!phone_context::Resolve(handle,"/menu_call",315,panel)||
     !phone_context::Resolve(handle,"/menu_call/numerotation",282,digits)||
     !phone_timeline_policy::DialerPaint(Word(reinterpret_cast<void*>(panel),0x60),Word(reinterpret_cast<void*>(digits),0x60))||
     !phone_context::Number(root,digits,"max_number",maximum,10)||!maximum||!phone_context::Number(root,digits,"first_valid",first,1))return false;
  v.kind=Kind::Dialer;v.offset=maximum;
  if(!phone_parameters_context::Text(root,scope,"m_phone",v.title,96)||!phone_parameters_context::Text(root,scope,"m_correct",v.subtitle,256)||
     !OptionalText(root,scope,"call_number",v.body,4096)||!phone_parameters_context::Text(root,scope,"m_Back",v.back,64)||!phone_parameters_context::Text(root,scope,"m_call",v.open,64))return false;
  const auto length=wcslen(v.body);if(length>maximum)return false;for(unsigned n=0;n<length;++n)if(v.body[n]<L'0'||v.body[n]>L'9')return false;
  if(first)v.body[0]=0;
  if(released||!phone_timeline_policy::DialerInteractive(Word(reinterpret_cast<void*>(panel),0x60),Word(reinterpret_cast<void*>(digits),0x60))||
     Word(reinterpret_cast<void*>(panel),0x64)!=2||*reinterpret_cast<BYTE*>(panel+0x68)||
     Word(reinterpret_cast<void*>(digits),0x64)!=2||*reinterpret_cast<BYTE*>(digits+0x68))out.flags|=PhoneContentBusy;
 }else return false;
 out.flags|=PhoneContentVisible;owner={reinterpret_cast<DWORD>(movie),reinterpret_cast<DWORD>(handle),root,panel,static_cast<DWORD>(v.kind)};return true;
}
static char dialNumber[11]{};
static bool DispatchTelephony(unsigned char* movie,const PhoneContentView& v,Action a,DWORD row){
 if(v.kind==Kind::Call){
  if(a!=Action::Back)return false;
#ifdef AITD_PHONE_PARAMETERS_TEST
  return TestEndCall();
#else
  // Original manager EndCall: stops native audio/events and returns Home.
  reinterpret_cast<void(__stdcall*)(void*)>(0x552480)(reinterpret_cast<void*>(0xff4780));return true;
#endif
 }
 if(v.kind!=Kind::Dialer)return false;
 if(a==Action::Back)return menu_context::Timeline(movie,"/","back_home");
 auto* handle=reinterpret_cast<unsigned char*>(Word(movie,0x24));
 if(a==Action::Digit||a==Action::Delete){
  const auto before=wcslen(v.body);if(before>10)return false;for(unsigned n=0;n<before;++n)dialNumber[n]=static_cast<char>(v.body[n]);dialNumber[before]=0;auto length=before;
  if(a==Action::Digit){if(row>9||length>=v.offset)return false;dialNumber[length++]=static_cast<char>('0'+row);dialNumber[length]=0;}
  else{if(!length)return false;dialNumber[--length]=0;}
  const auto* number=length?dialNumber:"0";
  if(!phone_context::String(movie,"/:call_number",number))return false;
  options_context::NativeInteger(handle,"/menu_call/numerotation/:first_valid",length?0:1);return true;
 }
 if(a!=Action::Call||!v.body[0])return false;DWORD icon{};
 if(!phone_context::Resolve(handle,"/menu_call/numerotation/ic_1",219,icon))return false;
 options_context::NativeInteger(handle,"/menu_call/numerotation/:num_ic",1);
 if(!phone_context::String(movie,"/menu_call/numerotation/:cur_ic","ic_1"))return false;
 options_context::NativeInteger(handle,"/:release_button",1);
 return menu_context::Timeline(movie,"/menu_call/numerotation/ic_1","valid");
}
