#pragma once
#include "pause_state.h"
#include "ui_art.h"
namespace pause_art {
// Native pause routes retain save/load/confirm semantics. Order only the view.
inline constexpr unsigned Slots[]={1,3,2,4,5,6};
struct Labels {ui_theme::Label title,rows[6],enter,select,escape,resume,episode,sequence,synopsis;};
ui_art::Bounds Row(unsigned index)noexcept;
unsigned Hit(float x,float y)noexcept;
unsigned Navigate(unsigned selected,int direction,DWORD enabledMask)noexcept;
HRESULT Prepare(const PauseStateV1&,Labels&)noexcept;
void Build(const PauseStateV1&,const Labels&,unsigned selected,ui_art::Scene&);
}
