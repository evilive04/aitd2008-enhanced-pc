#pragma once
#include "options_children_context.h"
#include "menu_presentation_policy.h"
#include <algorithm>
#include <cmath>
namespace options_children_controls {
using options_children_art::Action;using options_children_art::Hit;using options_children_art::Layout;
enum Key : unsigned { Mouse=1,Enter=2,Up=4,Down=8,Tab=16,Escape=32,Left=64,Right=128,Reset=256,Calibration=512 };
struct Input { unsigned held{};DWORD tick{};bool mouse{};float x{},y{}; };
struct Request { Action action=Action::None;LONG row=-1;double normalized{}; };
class Controls {
    menu_presentation_policy::KeyEdges keys;
    DWORD generation{},repeatAt{},dragAt{};unsigned repeatKey{};
    bool visible{},haveMouse{},dragging{};float previousX{},previousY{},lastDrag=-1;
    LONG dragRow=-1;Hit pressed{};
    static bool Row(const OptionsChildrenStateV1& s,LONG n){return n>=0&&static_cast<unsigned>(n)<s.view.count&&s.view.rows[n].enabled&&!s.view.rows[n].locked;}
    static bool Allowed(const OptionsChildrenStateV1& s,Action action){return (s.actions&(1u<<static_cast<unsigned>(action)))!=0;}
    static bool Footer(const OptionsChildrenStateV1& s,Action action){for(const auto& f:s.view.footer)if(f.action==action&&f.enabled)return Allowed(s,action);return false;}
public:
    void Hide(unsigned held=0){visible=false;haveMouse=false;dragging=false;dragRow=-1;pressed={};repeatKey=0;keys.Reset(held);}
    Request Read(const OptionsChildrenStateV1& s,const Layout& l,const Input& in){
        if(!visible||generation!=s.generation){Hide(in.held);visible=true;generation=s.generation;previousX=in.x;previousY=in.y;haveMouse=in.mouse;return {};}
        const auto edge=keys.Read(in.held);const bool moved=in.mouse&&haveMouse&&(in.x!=previousX||in.y!=previousY);
        previousX=in.x;previousY=in.y;haveMouse=in.mouse;
        const auto hit=in.mouse?options_children_art::HitTest(l,in.x,in.y):Hit{};
        if((s.flags&OptionsChildBusy)||!(s.flags&OptionsChildActionsProven)){pressed={};if(!(in.held&Mouse))dragging=false;repeatKey=0;keys.Reset(in.held);return {};}
        Request request;
        if(edge.pressed&Mouse){pressed=hit;if(hit.action==Action::Gauge){dragging=true;dragRow=hit.row;lastDrag=-1;dragAt=in.tick;}}
        if(dragging){
            if(!Row(s,dragRow)||!Allowed(s,Action::Gauge)){dragging=false;}
            else if(in.mouse&&(in.held&Mouse||edge.released&Mouse))for(unsigned n=0;n<l.count;++n){const auto& r=l.regions[n];if(r.action==Action::Gauge&&r.row==dragRow&&r.bounds.w>0){
                const float t=std::max(0.f,std::min(1.f,(in.x-r.bounds.x)/r.bounds.w));
                if(lastDrag<0||((in.tick-dragAt>=40||edge.released&Mouse)&&std::fabs(t-lastDrag)>.001f)){request={Action::Gauge,dragRow,t};lastDrag=t;dragAt=in.tick;}break;
            }}
            if(!(in.held&Mouse)){dragging=false;pressed={};}
        }else if(edge.released&Mouse){if(hit.action!=Action::None&&hit.action==pressed.action&&hit.row==pressed.row&&Allowed(s,hit.action))request={hit.action,hit.row,hit.normalized};pressed={};}
        if(edge.released&Enter){if(Footer(s,Action::Confirm))request={Action::Confirm};}
        if(edge.released&Escape){if(Footer(s,Action::Cancel))request={Action::Cancel};}
        if(edge.released&Reset){if(Footer(s,Action::Reset))request={Action::Reset};}
        if(edge.released&Calibration){if(Footer(s,Action::Calibration))request={Action::Calibration};}
        if(request.action!=Action::None)return request;
        const unsigned navigation=in.held&(Up|Down|Tab|Left|Right);
        unsigned pulse=edge.pressed&(Up|Down|Tab|Left|Right);
        if(pulse){repeatKey=pulse&(~pulse+1u);repeatAt=in.tick+320;}
        else if(repeatKey&&(navigation&repeatKey)&&static_cast<LONG>(in.tick-repeatAt)>=0){pulse=repeatKey;repeatAt=in.tick+85;}
        else if(!(navigation&repeatKey))repeatKey=0;
        if(pulse&(Up|Down|Tab)){const LONG direction=pulse&Up?-1:1;LONG row=s.view.selected;
            for(unsigned n=0;n<s.view.count;++n){row=(row+direction+static_cast<LONG>(s.view.count))%static_cast<LONG>(s.view.count);if(Row(s,row)&&Allowed(s,Action::FocusRow))return {Action::FocusRow,row};}
        }
        if((pulse&(Left|Right))&&Row(s,s.view.selected)){const auto action=pulse&Left?Action::Previous:Action::Next;if(Allowed(s,action))return {action,s.view.selected};}
        if(moved&&!(in.held&Mouse)&&hit.row!=s.view.selected&&Row(s,hit.row)&&Allowed(s,Action::FocusRow))return {Action::FocusRow,hit.row};
        return {};
    }
};
// A click on an unfocused native row must complete its original focus lifecycle
// before touching its selector/widget. Keep only the newest unaccepted intent.
class Requests {
    Request pending{};DWORD generation{};
public:
    void Clear(){pending={};generation=0;}
    void Offer(DWORD current,const Request& request){if(request.action!=Action::None){pending=request;generation=current;}}
    Request Next(const OptionsChildrenStateV1& state){
        if(generation!=state.generation){Clear();return {};}
        if((state.flags&OptionsChildBusy)||!(state.flags&OptionsChildActionsProven))return {};
        if(pending.action==Action::None)return {};
        if(!(state.actions&(1u<<static_cast<unsigned>(pending.action)))){Clear();return {};}
        if(pending.row>=0){
            const auto n=static_cast<unsigned>(pending.row);
            if(n>=state.view.count||!state.view.rows[n].enabled||state.view.rows[n].locked){Clear();return {};}
            if(pending.row!=state.view.selected&&pending.action!=Action::FocusRow){if(state.actions&(1u<<static_cast<unsigned>(Action::FocusRow)))return {Action::FocusRow,pending.row};Clear();return {};}
        }else{bool enabled=false;for(const auto& f:state.view.footer)enabled|=f.action==pending.action&&f.enabled;if(!enabled){Clear();return {};}}
        return pending;
    }
    void Complete(const Request& sent,bool accepted){if(accepted&&sent.action==pending.action&&sent.row==pending.row)Clear();}
};
}
