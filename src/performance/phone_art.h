#pragma once
#include "phone_state.h"
#include "ui_art.h"
namespace phone_art {
struct Labels {ui_theme::Label title,objectiveTitle,objective,items[6],close,open,escape,enter;};
HRESULT Prepare(const PhoneStateV1&,Labels&)noexcept;
ui_art::Bounds Item(PhoneItemKind)noexcept;
unsigned Hit(const PhoneStateV1&,float,float)noexcept;
unsigned Navigate(const PhoneStateV1&,unsigned,int,int)noexcept;
void Build(const PhoneStateV1&,const Labels&,unsigned,ui_art::Scene&,bool imageIcons=false);
}
