using Action=chapters_art::Action;
static DWORD dispatches{},rejected{};
static bool Notify(unsigned char* movie,const char* name){
 ULONGLONG hash{};for(unsigned n=0;n<8&&name[n];++n)hash=(hash<<8)|static_cast<BYTE>(name[n]);
#ifdef AITD_CHAPTERS_ACTIONS_TEST
 return TestNotify(movie,name);
#else
 using Native=unsigned char(__thiscall*)(void*,void*,DWORD,DWORD,const char*);return reinterpret_cast<Native>(0x53dde0)(reinterpret_cast<void*>(0xfe15f8),movie,static_cast<DWORD>(hash),static_cast<DWORD>(hash>>32),name)!=0;
#endif
}
static bool Frame(unsigned char* movie,const char* path,const char* label){return menu_context::Timeline(movie,path,label);}
static bool Focus(unsigned char* movie,const ChaptersStateV1& s,DWORD row){
 if(row>=s.count)return false;auto* handle=reinterpret_cast<unsigned char*>(Word(movie,0x24));char path[112]{};
 sprintf_s(path,"/menu/sequences/line%lu",s.selected+1);if(!Frame(movie,path,s.rows[s.selected].locked?"out_lock":"out"))return false;
 options_context::NativeInteger(handle,"/menu/sequences/:cur_list_line",row+1);if(!Notify(movie,"sequroll"))return false;
 options_context::NativeInteger(handle,"/:lock",s.rows[row].locked);options_context::NativeInteger(handle,"/menu/sequences/info_lock/:info_active",s.rows[row].locked);
 sprintf_s(path,"/menu/sequences/line%lu",row+1);return Frame(movie,path,s.rows[row].locked?"on_lock":"on");
}
static bool Page(unsigned char* movie,const ChaptersStateV1& s,bool next,bool last){
 const DWORD offset=next?s.offset+s.count:s.offset>5?s.offset-5:0;if(offset>=s.total||offset==s.offset)return false;auto* handle=reinterpret_cast<unsigned char*>(Word(movie,0x24));
 options_context::NativeInteger(handle,"/menu/sequences/:start_line",offset);options_context::NativeInteger(handle,"/menu/sequences/:cur_line",offset+1);options_context::NativeInteger(handle,"/menu/sequences/:cur_list_line",1);
 if(!Notify(movie,"opensequ"))return false;ChaptersStateV1 page{};Owner owner{};if(!Capture(page,owner))return false;
 return Focus(movie,page,last?page.count-1:0);
}
static bool CanAct(const ChaptersStateV1& s,DWORD code,LONG row){
 if(code<1||code>static_cast<DWORD>(Action::PageNext))return false;const auto a=static_cast<Action>(code);
 if(a==Action::ChapterCard)return (s.flags&ChaptersCanChange)&&row>=0&&static_cast<DWORD>(row)<s.chapters&&static_cast<DWORD>(row)+1!=s.chapter;
 if(a==Action::PreviousChapter||a==Action::NextChapter)return (s.flags&ChaptersCanChange)&&row==-1;
 if(a==Action::Focus)return row>=0&&static_cast<DWORD>(row)<s.count;
 if(a==Action::Activate)return row>=0&&static_cast<DWORD>(row)<s.count&&!s.rows[row].locked;
 if(row!=-1)return false;
 if(a==Action::PagePrevious)return s.offset>0;if(a==Action::PageNext)return s.offset+s.count<s.total;
 if(a==Action::MovePrevious)return s.offset+s.selected>0;if(a==Action::MoveNext)return s.offset+s.selected+1<s.total;return a==Action::Back;
}
static bool Dispatch(unsigned char* movie,const ChaptersStateV1& s,const Request& r){
 if(!CanAct(s,r.action,r.row))return false;const auto a=static_cast<Action>(r.action);auto* handle=reinterpret_cast<unsigned char*>(Word(movie,0x24));
 if(a==Action::Focus)return Focus(movie,s,r.row);
 if(a==Action::MovePrevious)return s.selected?Focus(movie,s,s.selected-1):Page(movie,s,false,true);
 if(a==Action::MoveNext)return s.selected+1<s.count?Focus(movie,s,s.selected+1):Page(movie,s,true,false);
 if(a==Action::PagePrevious||a==Action::PageNext)return Page(movie,s,a==Action::PageNext,false);
 if(a==Action::Back){options_context::NativeInteger(handle,"/:back",1);options_context::NativeInteger(handle,"/:release_button",1);return Frame(movie,"/menu","close");}
 if(a==Action::Activate){
  if(!Focus(movie,s,r.row))return false;options_context::NativeInteger(handle,"/:lock_cur_sequ",0);options_context::NativeInteger(handle,"/:back",0);options_context::NativeInteger(handle,"/:release_button",1);
  char path[100]{};sprintf_s(path,"/menu/sequences/line%ld",r.row+1);return Frame(movie,path,"valid");
 }
 const int delta=a==Action::PreviousChapter?-1:a==Action::NextChapter?1:static_cast<int>(r.row)+1-static_cast<int>(s.chapter);
 const DWORD target=(static_cast<int>(s.chapter)-1+delta+static_cast<int>(s.chapters))%s.chapters+1;
 char prefix[192]{};static char number[256]{};if(!options_children_context::RawText(Word(handle),Word(handle)+200,"m_chapter",prefix,sizeof prefix)||sprintf_s(number,"%s %lu",prefix,target)<0)return false;
 if(!options_children_context::NativeString(movie,"/:new_chapternumber",number))return false;
 options_context::NativeInteger(handle,"/:ChangeChapter",1);
 options_context::NativeInteger(handle,"/:new_ChapterIndex",target);if(delta<0)options_context::NativeInteger(handle,"/:c_ChapterIndex",target);
 options_context::NativeInteger(handle,"/:release_button",1);return Frame(movie,"/menu",delta<0?"left":"right");
}
static BOOL GetState(void* output,DWORD bytes){if(!output||bytes!=sizeof state)return FALSE;ChaptersStateV1 copy{};AcquireSRWLockShared(&lock);copy=state;ReleaseSRWLockShared(&lock);if(GetTickCount()-copy.tick>250)return FALSE;__try{memcpy(output,&copy,sizeof copy);}__except(EXCEPTION_EXECUTE_HANDLER){return FALSE;}return TRUE;}
static void SetReady(BOOL value,DWORD epoch){AcquireSRWLockExclusive(&lock);if(value>=ChaptersInteractive&&value<=ChaptersPaintOnly&&presenterEnabled&&(state.flags&ChaptersVisible)&&state.generation==epoch&&GetTickCount()-state.tick<=250){ready=true;interactiveReady=value==ChaptersInteractive;readyGeneration=epoch;readyTick=GetTickCount();}else if(readyGeneration==epoch)ready=interactiveReady=false;ReleaseSRWLockExclusive(&lock);}
static BOOL Queue(DWORD action,LONG row,DWORD epoch){
 if(!phone_context::Foreground())return FALSE;AcquireSRWLockExclusive(&lock);const auto now=GetTickCount();const bool accepted=enabled&&presenterEnabled&&ready&&interactiveReady&&(state.flags&ChaptersVisible)&&!(state.flags&ChaptersBusy)&&state.generation==epoch&&readyGeneration==epoch&&now-state.tick<=250&&now-readyTick<=250&&!queued.action&&CanAct(state,action,row);if(accepted)queued={action,epoch,row};ReleaseSRWLockExclusive(&lock);return accepted?TRUE:FALSE;
}
static bool OwnsPaint(std::uintptr_t root)noexcept{AcquireSRWLockShared(&lock);const bool yes=ui_paint_owner::Visual(root,previous.root,presenterEnabled,(state.flags&ChaptersVisible)!=0,ready,state.generation,readyGeneration,state.tick,readyTick,GetTickCount());ReleaseSRWLockShared(&lock);return yes;}
static bool OwnsPointer()noexcept{AcquireSRWLockShared(&lock);const bool yes=ui_paint_owner::Fresh(previous.root,previous.root,presenterEnabled,(state.flags&ChaptersVisible)!=0,ready,state.generation,readyGeneration,state.tick,readyTick,GetTickCount());ReleaseSRWLockShared(&lock);return yes;}
static bool ConsumesInput(){if(!phone_context::Foreground())return false;AcquireSRWLockExclusive(&lock);const auto now=GetTickCount();bool yes=presenterEnabled&&ready&&(state.flags&ChaptersVisible)&&state.generation==readyGeneration&&now-state.tick<=250&&now-readyTick<=250;if(!yes&&closing){if(now-closingTick<80||(GetAsyncKeyState(VK_RETURN)&0x8000)||(GetAsyncKeyState(VK_ESCAPE)&0x8000)||(GetAsyncKeyState(VK_LBUTTON)&0x8000))yes=true;else closing=false;}ReleaseSRWLockExclusive(&lock);return yes;}
static void Tick(){
 ChaptersStateV1 next{};Owner owner{};if(!CapturePresentation(next,owner)){next={};owner={};}AcquireSRWLockExclusive(&lock);if(memcmp(&owner,&previous,sizeof owner)){previous=owner;++generation;ready=interactiveReady=false;}
 const auto request=queued;queued={};const bool allowed=request.action&&presenterEnabled&&ready&&interactiveReady&&(next.flags&ChaptersVisible)&&!(next.flags&ChaptersBusy)&&request.generation==generation&&readyGeneration==generation&&GetTickCount()-readyTick<=250;ReleaseSRWLockExclusive(&lock);
 bool sent=false;if(allowed&&phone_context::Foreground()){__try{sent=Dispatch(reinterpret_cast<unsigned char*>(owner.movie),next,request);}__except(EXCEPTION_EXECUTE_HANDLER){sent=false;}}
 if(request.action){if(!CapturePresentation(next,owner)){next={};owner={};}AcquireSRWLockExclusive(&lock);if(memcmp(&owner,&previous,sizeof owner)){previous=owner;++generation;}ready=interactiveReady=false;closing=true;closingTick=GetTickCount();ReleaseSRWLockExclusive(&lock);if(sent)++dispatches;else ++rejected;}
 next.tick=GetTickCount();next.generation=generation;AcquireSRWLockExclusive(&lock);state=next;if(!(next.flags&ChaptersVisible))ready=false;ReleaseSRWLockExclusive(&lock);
}
