// Included inside controls_context; all native mutations run on the UI tick.
using Action=controls_art::Action;
static bool Notify(unsigned char* movie,const char* name){
 // Native FSCommand handlers intentionally return0 for several accepted widget
 // notifications. The original AVM ignores that return value as well.
 options_context::Command(movie,name);return true;
}
static bool Set(unsigned char* handle,const char* path,DWORD value){options_context::NativeInteger(handle,path,value);return true;}
static bool String(unsigned char* movie,const char* path,const char* value){return options_children_context::NativeString(movie,path,value);}
static bool Frame(unsigned char* movie,const char* path,const char* label){return menu_context::Timeline(movie,path,label);}
static bool Focus(unsigned char* movie,const ControlsStateV1& s,LONG row){
 if(row<0||static_cast<DWORD>(row)>=s.view.count||s.view.rows[row].locked)return false;
 auto* handle=reinterpret_cast<unsigned char*>(Word(movie,0x24));
 if(s.view.phase==Phase::Bindings){
  Set(handle,"/options/configuration/array/:cur_line",row+1);
  Set(handle,"/options/configuration/array/:start_line",s.view.offset+row+1);
  Notify(movie,"widg:kmap:rlvr");
  return Word(reinterpret_cast<void*>(0xfe1938),0x54)==static_cast<DWORD>(row)&&Word(reinterpret_cast<void*>(0xfe1938),0x60)==s.view.offset;
 }
 if(s.view.phase!=Phase::Settings)return false;
 const auto old=s.view.selected;if(static_cast<DWORD>(row)==old)return true;
 char prior[96]{},next[96]{},out[32]{},in[32]{};DWORD binding[2]{};
 sprintf_s(prior,"/options/configuration/param_%u",old+1);sprintf_s(next,"/options/configuration/param_%ld",row+1);
 if(!menu_context::Resolve(handle,prior,binding)||binding[1]!=Word(handle)||!options_children_context::RawText(Word(handle),binding[0],"msgout",out,32)||
    !menu_context::Resolve(handle,next,binding)||binding[1]!=Word(handle)||!options_children_context::RawText(Word(handle),binding[0],"msgin",in,32))return false;
 Notify(movie,out);if(!Frame(movie,prior,s.view.rows[old].locked?"out_lock":"out"))return false;
 Set(handle,"/options/configuration/:num_param",row+1);Notify(movie,in);return Frame(movie,next,"on");
}
static bool Dispatch(unsigned char* movie,const ControlsStateV1& s,const Request& r){
 auto* handle=reinterpret_cast<unsigned char*>(Word(movie,0x24));const auto action=static_cast<Action>(r.action);
 const bool settings=s.view.phase==Phase::Settings;const char* panel="/options/configuration";
 if(action==Action::Category){
  if(r.row<0||r.row>3)return false;const auto old=s.view.category;if(static_cast<DWORD>(r.row)==old)return true;
  char path[96]{};sprintf_s(path,"%s/o_%u",panel,old+1);if(!Frame(movie,path,"out"))return false;
  Set(handle,"/options/configuration/:cur_hinge",r.row+1);sprintf_s(path,"%s/o_%ld",panel,r.row+1);if(!Frame(movie,path,"on"))return false;
  Set(handle,"/:release_button",1);
  // Original tabs invoke ongl immediately only when crossing the settings tab;
  // binding->binding change_hinge invokes it itself on its timeline.
  if(settings||r.row==3)Notify(movie,"widg:kmap:ongl");
  return Frame(movie,panel,settings?"back_from_special":r.row==3?"to_special":"change_hinge");
 }
 if(action==Action::Focus)return Focus(movie,s,r.row);
 if(action==Action::Activate){
  if(!Focus(movie,s,r.row))return false;if(settings)return true;
  Set(handle,"/:release_button",1);Set(handle,"/:lockparam",1);return Notify(movie,"widg:kmap:getk");
 }
 if(action==Action::PagePrevious||action==Action::PageNext||action==Action::MovePrevious||action==Action::MoveNext){
  if(settings)return false;const bool previousPage=action==Action::PagePrevious||action==Action::MovePrevious;
  const LONG step=action==Action::MovePrevious||action==Action::MoveNext?1:10;
  const LONG selected=static_cast<LONG>(s.view.offset+s.view.selected);
  const LONG target=(std::max)(0L,(std::min)(static_cast<LONG>(s.view.total)-1,selected+(previousPage?-step:step)));
  if(target>=static_cast<LONG>(s.view.offset)&&target<static_cast<LONG>(s.view.offset+s.view.count))return Focus(movie,s,target-s.view.offset);
  const LONG offset=target<static_cast<LONG>(s.view.offset)?target:target-static_cast<LONG>(s.view.count)+1;
  const LONG delta=offset-static_cast<LONG>(s.view.offset);
  for(LONG n=0;n<std::abs(delta);++n)Notify(movie,delta<0?"widg:kmap:scru":"widg:kmap:scrd");
  ControlsStateV1 updated{};Owner owner{};if(!Capture(updated,owner)||updated.view.offset!=static_cast<DWORD>(offset))return false;
  return Focus(movie,updated,target-offset);
 }
 if(action==Action::Previous||action==Action::Next||action==Action::Gauge){
  if(!settings||r.row<0||static_cast<DWORD>(r.row)!=s.view.selected||s.view.rows[r.row].locked)return false;
  const auto& row=s.view.rows[r.row];
  if(row.kind==Kind::Gauge){
   if(r.row!=2&&r.row!=3)return false;const DWORD widget=0xfe1790+(r.row==2?0xe8:0x12318);
   if(*reinterpret_cast<BYTE*>(widget+8)!=1||Word(reinterpret_cast<void*>(widget),0x30)!=reinterpret_cast<DWORD>(handle))return false;
   options_native_values::Gauge gauge{};if(!options_children_context::Gauge(widget,gauge))return false;
   double value=gauge.current;
   if(action==Action::Gauge)value=gauge.minimum+r.normalized*(gauge.maximum-gauge.minimum);
   else value+=(action==Action::Previous?-1.:1.)*(gauge.maximum-gauge.minimum)/gauge.steps;
   value=std::fmax(gauge.minimum,std::fmin(gauge.maximum,value));
   return options_children_context::NativeGauge(widget,static_cast<float>(value))&&Notify(movie,"newparam");
  }
  if(row.kind!=Kind::Selector||action==Action::Gauge)return false;
  char path[128]{};sprintf_s(path,"%s/param_%ld/reglage/param",panel,r.row+1);return Frame(movie,path,action==Action::Previous?"left":"right");
 }
 if(r.row!=-1)return false;
 if(action==Action::Cancel||action==Action::Back)return Notify(movie,"opcancel")&&Frame(movie,"/options","back_from_config");
 if(action==Action::Confirm){
  Notify(movie,"testcfg");DWORD missing{};if(!Number(Word(handle),Word(handle)+200,"key_missing",missing,1))return false;
  if(missing)return String(movie,"/:key","key_missing")&&String(movie,"/:onglet",settings?"special":"default")&&String(movie,"/:whatpopup","warning")&&Frame(movie,"/options/pop_up","open")&&Frame(movie,panel,settings?"block_special":"block");
  return Notify(movie,"opvalid")&&Frame(movie,"/options","back_from_config");
 }
 if(action==Action::Reset)return String(movie,"/:key","reset")&&String(movie,"/:whatswitch","reset")&&String(movie,"/:onglet",settings?"special":"default")&&String(movie,"/:whatpopup","warningchoice")&&Frame(movie,"/options/pop_up","open")&&Frame(movie,panel,settings?"block_special":"block");
 return false;
}
static void Permissions(ControlsStateV1& s){
 if(!(s.flags&ControlsVisible))return;s.flags|=ControlsActionsProven;
 const bool usable=!(s.flags&(ControlsBusy|ControlsCaptureInput));
 for(auto& f:s.view.footer)if(f.action!=Action::None)f.enabled=usable?1:0;
 for(auto& row:s.view.rows)row.enabled=usable?1:0;
}
static BOOL GetState(void* out,DWORD bytes){
 if(!out||bytes!=sizeof state)return FALSE;ControlsStateV1 copy{};AcquireSRWLockShared(&lock);copy=state;ReleaseSRWLockShared(&lock);
 if(GetTickCount()-copy.tick>250)return FALSE;__try{memcpy(out,&copy,sizeof copy);}__except(EXCEPTION_EXECUTE_HANDLER){return FALSE;}return TRUE;
}
static void SetReady(BOOL value,DWORD epoch){
 AcquireSRWLockExclusive(&lock);if(value&&enabled&&(state.flags&ControlsVisible)&&state.generation==epoch&&GetTickCount()-state.tick<=250){ready=true;readyGeneration=epoch;readyTick=GetTickCount();}
 else if(readyGeneration==epoch)ready=false;ReleaseSRWLockExclusive(&lock);
}
static BOOL Queue(DWORD action,LONG row,double value,DWORD epoch){
 if(action<1||action>static_cast<DWORD>(Action::MoveNext)||!phone_context::Foreground()||!std::isfinite(value))return FALSE;
 AcquireSRWLockExclusive(&lock);const auto now=GetTickCount();const auto kind=static_cast<Action>(action);bool target=false;
 if(kind==Action::Category)target=row>=0&&row<4;
 else if(kind==Action::Focus||kind==Action::Activate||kind==Action::Previous||kind==Action::Next||kind==Action::Gauge){
  target=row>=0&&static_cast<DWORD>(row)<state.view.count&&state.view.rows[row].enabled&&!state.view.rows[row].locked;
  if(kind==Action::Previous||kind==Action::Next||kind==Action::Gauge)target=target&&static_cast<DWORD>(row)==state.view.selected&&state.view.phase==Phase::Settings;
  if(kind==Action::Gauge)target=target&&state.view.rows[row].kind==Kind::Gauge&&value>=0&&value<=1;
 }else if(kind==Action::PagePrevious||kind==Action::PageNext||kind==Action::MovePrevious||kind==Action::MoveNext)target=row==-1&&state.view.phase==Phase::Bindings&&state.view.total>0;
 else if(row==-1)for(const auto& f:state.view.footer)if(f.enabled&&f.action==kind)target=true;
 const bool accept=target&&enabled&&ready&&(state.flags&(ControlsVisible|ControlsActionsProven))==(ControlsVisible|ControlsActionsProven)&&!(state.flags&(ControlsBusy|ControlsCaptureInput))&&epoch==state.generation&&epoch==readyGeneration&&now-state.tick<=250&&now-readyTick<=250&&!queued.action;
 if(accept)queued={action,epoch,row,value};ReleaseSRWLockExclusive(&lock);return accept?TRUE:FALSE;
}
static bool WheelLease(DWORD now){return enabled&&ready&&(state.flags&(ControlsVisible|ControlsActionsProven))==(ControlsVisible|ControlsActionsProven)&&!(state.flags&(ControlsBusy|ControlsCaptureInput))&&state.view.phase==Phase::Bindings&&state.generation==readyGeneration&&now-state.tick<=250&&now-readyTick<=250;}
static bool CaptureWheel(LONG delta){
 const bool wheelFocused=phone_context::Foreground();AcquireSRWLockExclusive(&lock);const bool owns=wheelFocused&&WheelLease(GetTickCount());
 if(owns)wheel=static_cast<LONG>((std::max)(-3840LL,(std::min)(3840LL,static_cast<long long>(wheel)+delta)));else wheel=0;
 ReleaseSRWLockExclusive(&lock);return owns;
}
static LONG TakeWheel(DWORD epoch){
 const bool wheelFocused=phone_context::Foreground();AcquireSRWLockExclusive(&lock);const LONG value=wheelFocused&&epoch==state.generation&&WheelLease(GetTickCount())?wheel:0;wheel=0;ReleaseSRWLockExclusive(&lock);return value;
}
static bool OwnsPaint(std::uintptr_t root)noexcept{
 AcquireSRWLockShared(&lock);const bool owns=ui_paint_owner::Visual(root,previous.root,enabled,(state.flags&(ControlsVisible|ControlsActionsProven))==(ControlsVisible|ControlsActionsProven),ready,state.generation,readyGeneration,state.tick,readyTick,GetTickCount());ReleaseSRWLockShared(&lock);return owns;
}
static bool OwnsPointer()noexcept{
 AcquireSRWLockShared(&lock);const bool owns=ui_paint_owner::Fresh(previous.root,previous.root,enabled,(state.flags&(ControlsVisible|ControlsActionsProven))==(ControlsVisible|ControlsActionsProven),ready,state.generation,readyGeneration,state.tick,readyTick,GetTickCount());ReleaseSRWLockShared(&lock);return owns;
}
static bool ConsumesInput(){
 if(!phone_context::Foreground())return false;AcquireSRWLockExclusive(&lock);const auto now=GetTickCount();
 if(state.flags&ControlsCaptureInput){closing=false;ReleaseSRWLockExclusive(&lock);return false;}
 bool consumes=enabled&&ready&&(state.flags&(ControlsVisible|ControlsActionsProven))==(ControlsVisible|ControlsActionsProven)&&state.generation==readyGeneration&&now-state.tick<=250&&now-readyTick<=250;
 if(!consumes&&closing){if(now-closingTick<80||(GetAsyncKeyState(VK_RETURN)&0x8000)||(GetAsyncKeyState(VK_ESCAPE)&0x8000)||(GetAsyncKeyState(VK_LBUTTON)&0x8000))consumes=true;else closing=false;}ReleaseSRWLockExclusive(&lock);return consumes;
}
static void Tick(){
 ControlsStateV1 next{};Owner owner{};Capture(next,owner);Permissions(next);AcquireSRWLockExclusive(&lock);
 if(memcmp(&owner,&previous,sizeof owner)){previous=owner;++generation;ready=false;wheel=0;}
 const auto request=queued;queued={};const bool allowed=request.action&&(next.flags&ControlsVisible)&&!(next.flags&(ControlsBusy|ControlsCaptureInput))&&ready&&request.generation==generation&&readyGeneration==generation&&GetTickCount()-readyTick<=250;
 const auto sentBefore=state.dispatches,rejectedBefore=state.rejected;if(request.action){ready=false;closing=true;closingTick=GetTickCount();}ReleaseSRWLockExclusive(&lock);
 bool sent=false;if(allowed&&phone_context::Foreground()){__try{sent=Dispatch(reinterpret_cast<unsigned char*>(owner.movie),next,request);}__except(EXCEPTION_EXECUTE_HANDLER){sent=false;}}
 if(request.action){Capture(next,owner);Permissions(next);AcquireSRWLockExclusive(&lock);if(memcmp(&owner,&previous,sizeof owner)){previous=owner;++generation;}ReleaseSRWLockExclusive(&lock);}
 next.tick=GetTickCount();next.generation=generation;next.dispatches=sentBefore+(sent?1:0);next.rejected=rejectedBefore+(request.action&&!sent?1:0);AcquireSRWLockExclusive(&lock);state=next;if(!(next.flags&ControlsVisible))ready=false;ReleaseSRWLockExclusive(&lock);
}
