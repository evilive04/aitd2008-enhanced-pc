// Included inside the shared phone-page context. Names and route identifiers
// come from the native roster; no invented contacts or replacement portraits.
struct RosterRoute {const char* id;const char* caption;DWORD resource;};
static const RosterRoute rosterRoutes[]={{"bungle","m_bungle",371},{"hartford","m_hartford",374},
 {"sarah","m_sarah",377},{"tips","m_tips",380},{"no_one","m_no_one",383},{"documents","m_documents",388}};
static const RosterRoute* Route(const char* text){for(const auto& r:rosterRoutes)if(!strcmp(r.id,text))return &r;return nullptr;}
static bool Raw(DWORD root,DWORD node,const char* name,char* out,unsigned cap){
 dialog_text::Value v{};if(!phone_context::Value(root,node,name,v)||v.kind!=dialog_text::Kind::String||!v.text[0]||strlen(v.text)>=cap)return false;
 strcpy_s(out,cap,v.text);return true;
}
static bool RosterIcon(unsigned char* handle,const char* path,bool interactive,DWORD& node){
 DWORD binding[2]{};if(!menu_context::Resolve(handle,path,binding)||!binding[0]||binding[1]!=Word(handle))return false;
 node=binding[0];const auto resource=Word(reinterpret_cast<void*>(node),0x18);
 // The original outgoing animation hides icons one at a time. Their complete
 // current rows remain valid data under this same authenticated visible panel.
 // Stable interactive captures still require actual visible native icons.
 return resource&&Word(reinterpret_cast<void*>(resource),8)==389&&
        (!interactive||(Word(reinterpret_cast<void*>(node),0x6c)&1)!=0);
}
static bool CaptureRoster(DWORD mode,unsigned char* movie,OptionsChildrenStateV1& s,Owner& owner){
 auto* handle=reinterpret_cast<unsigned char*>(Word(movie,0x24));const DWORD root=Word(handle),scope=root+200;
 const char* panelPath=mode==2?"/menu_messages":"/menu_index";const DWORD resource=mode==2?414:575,wanted=mode==2?5:4;
 DWORD platform{},released{},panel{},count{},selected{};
 if(!phone_context::OptionalZero(root,scope,"fake")||!phone_context::Number(root,scope,"WhatPlatform",platform,1)||platform!=1||
    !phone_context::Number(root,scope,"release_button",released,1)||!Node(handle,panelPath,resource,panel)||
    !phone_timeline_policy::RosterPaint(mode,Word(reinterpret_cast<void*>(panel),0x60))||!phone_context::Number(root,panel,"nb_ic",count,5)||count!=wanted||
    !phone_context::Number(root,panel,"num_ic",selected,5)||!selected||selected>count)return false;
 char page[32]{};if(!Raw(root,scope,"whatpage",page,32))return false;
 if(strcmp(page,mode==2?"messages":"index")&&!Route(page))return false;
 auto& v=s.view;v.version=1;v.page=mode==2?options_children_art::Page::PhoneMessages:options_children_art::Page::PhoneContacts;v.count=count;v.selected=static_cast<LONG>(selected-1);
 if(!Text(root,scope,mode==2?"m_messages":"m_index",v.title,96))return false;
 const bool busy=released||!phone_timeline_policy::RosterInteractive(mode,Word(reinterpret_cast<void*>(panel),0x60))||Word(reinterpret_cast<void*>(panel),0x64)!=2||*reinterpret_cast<BYTE*>(panel+0x68);
 for(unsigned n=0;n<count;++n){
  char whoName[24]{},whereName[24]{},who[32]{},where[32]{},path[96]{};DWORD icon{},locked{};
  sprintf_s(whoName,"who%u",n+1);sprintf_s(whereName,"whereto%u",n+1);
  if(!Raw(root,panel,whoName,who,32)||!Raw(root,panel,whereName,where,32)||strcmp(who,where))return false;
  const auto* route=Route(who);if(!route)return false;
  sprintf_s(path,"%s/ic_%u",panelPath,n+1);
  if(!RosterIcon(handle,path,phone_timeline_policy::RosterInteractive(mode,Word(reinterpret_cast<void*>(panel),0x60)),icon)||!phone_context::Number(root,icon,"lock",locked,1)||
     (!strcmp(who,"no_one")&&!locked)||!Text(root,scope,route->caption,v.rows[n].label,128))return false;
  auto& row=v.rows[n];row.kind=options_children_art::Kind::Navigation;row.enabled=busy?0:1;row.locked=locked;row.current=n+1;
  // Copied native route metadata; never infer content from a localized name.
  for(unsigned k=0;who[k]&&k<31;++k)row.value[k]=static_cast<unsigned char>(who[k]);
 }
 if(v.rows[v.selected].locked){v.selected=-1;for(unsigned n=0;n<count;++n)if(!v.rows[n].locked){v.selected=static_cast<LONG>(n);break;}}
 const char* labels[]={"m_Valid","m_Back"};const wchar_t* keys[]={L"Enter",L"Esc"};
 for(unsigned n=0;n<2;++n){auto& f=v.footer[n];if(!Text(root,scope,labels[n],f.label,64))return false;wcscpy_s(f.key,keys[n]);f.action=n?Action::Cancel:Action::Confirm;f.enabled=busy?0:n?1:v.selected>=0?1:0;}
 s.nativeMode=mode;s.flags=OptionsChildVisible|OptionsChildActionsProven|OptionsChildInGame|(busy?OptionsChildBusy:0);
 for(const auto a:{Action::FocusRow,Action::Activate,Action::Confirm,Action::Cancel})s.actions|=1u<<static_cast<unsigned>(a);
 if(*reinterpret_cast<unsigned char**>(0xff4790)!=movie||Word(movie,0x24)!=reinterpret_cast<DWORD>(handle)||Word(handle)!=root)return false;
 owner={reinterpret_cast<DWORD>(movie),reinterpret_cast<DWORD>(handle),root,panel,mode};return true;
}
static __declspec(noinline) bool Command(unsigned char*,const char*);
static char rosterIconPaths[6][5][64]{},rosterCaptions[6][4096]{};
static bool FocusRoster(unsigned char* movie,const OptionsChildrenStateV1& s,LONG selected,bool activate){
 if(selected<0||static_cast<DWORD>(selected)>=s.view.count||!s.view.rows[selected].enabled||s.view.rows[selected].locked)return false;
 auto* handle=reinterpret_cast<unsigned char*>(Word(movie,0x24));const DWORD root=Word(handle);
 const char* prefix=s.nativeMode==2?"/menu_messages":"/menu_index";DWORD panel{},actor{};
 char path[96]{},whoKey[24]{},who[32]{},number[96]{},scopePath[96]{};
 if(!Node(handle,prefix,s.nativeMode==2?414:575,panel))return false;
 sprintf_s(whoKey,"who%ld",selected+1);if(!Raw(root,panel,whoKey,who,32))return false;
 const auto* route=Route(who);if(!route||!strcmp(who,"no_one"))return false;
 const unsigned routeIndex=static_cast<unsigned>(route-rosterRoutes);
 auto& icon=rosterIconPaths[routeIndex][selected];
 if(!icon[0])sprintf_s(icon,"ic_%ld/%s",selected+1,route->id);
 sprintf_s(path,"%s/%s",prefix,icon);
 if(activate&&!Node(handle,path,route->resource,actor))return false; // Validate before any native mutation.
 if(!Raw(root,root+200,route->caption,rosterCaptions[routeIndex],4096))return false;
 if(activate&&!Command(movie,"val_circ"))return false;
 sprintf_s(number,"%s/:num_ic",prefix);options_context::NativeInteger(handle,number,selected+1);
 sprintf_s(scopePath,"%s/:who",prefix);if(!phone_context::String(movie,scopePath,route->id))return false;
 sprintf_s(scopePath,"%s/:cur_ic",prefix);if(!phone_context::String(movie,scopePath,icon)||!phone_context::String(movie,"/:m_subtitle",rosterCaptions[routeIndex]))return false;
 if(!activate)return true;
 if(!phone_context::String(movie,"/:where_to",route->id))return false;
 options_context::NativeInteger(handle,"/:ic_val",selected+1);options_context::NativeInteger(handle,"/:release_button",1);
 return menu_context::Timeline(movie,path,"valid");
}
static bool DispatchRoster(unsigned char* movie,const OptionsChildrenStateV1& s,const Request& request){
 const auto a=static_cast<Action>(request.action);
 if(a==Action::Cancel&&request.row==-1)return menu_context::Timeline(movie,"/","back_home");
 if(a==Action::FocusRow)return FocusRoster(movie,s,request.row,false);
 if(a==Action::Activate)return FocusRoster(movie,s,request.row,true);
 if(a==Action::Confirm&&request.row==-1)return FocusRoster(movie,s,s.view.selected,true);
 return false;
}
