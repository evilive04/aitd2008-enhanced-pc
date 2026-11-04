#pragma once
#include "bleeding_ui_state.h"
#include "ui_art.h"
namespace bleeding_ui_art{
bool Valid(const BleedingUiValue&)noexcept;
HRESULT Append(const BleedingUiValue&,ui_art::Scene&,float x=860,float y=80)noexcept;
HRESULT Build(const BleedingUiStateV1&,ui_art::Scene&)noexcept;
}
