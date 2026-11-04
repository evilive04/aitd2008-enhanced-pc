#pragma once
#include "inventory_popup_state.h"
#include "ui_art.h"
namespace inventory_popup_art {
ui_art::Bounds Row(unsigned)noexcept;
struct Hit{InventoryPopupAction action{};DWORD row{};};
Hit HitTest(const InventoryPopupStateV1&,float,float)noexcept;
HRESULT Build(const InventoryPopupStateV1&,ui_art::Scene&)noexcept;
}
