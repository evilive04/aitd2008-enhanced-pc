#pragma once
#include "main_popup_state.h"
#include "ui_art.h"
namespace main_popup_art {
ui_art::Bounds Button(MainPopupKind,unsigned)noexcept;
HRESULT Build(const MainPopupStateV1&,unsigned selected,ui_art::Scene&)noexcept;
}
