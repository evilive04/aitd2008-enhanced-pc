#pragma once
#include "ui_art.h"
#include "favorites_state.h"
namespace favorites_art {
struct Labels {
    ui_theme::Label title[2],center[2],number[6],name[10],button[3],hint[3],notice[6];
    ui_art::Scene wheelHint;
};
HRESULT Prepare(Labels&) noexcept;
bool Valid(const FavoritesStateV1&) noexcept;
// Canonical1080-high canvas, center960/496.8, identical to favorites_policy.
bool Build(const FavoritesStateV1&,const Labels&,ui_art::Scene&) noexcept;
void Cursor(const FavoritesStateV1&,unsigned width,unsigned height,ui_art::Scene&);
}
