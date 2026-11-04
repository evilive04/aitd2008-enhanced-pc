#pragma once
#include "ui_art.h"
namespace ui_rich_keycaps {
// Native color/epc_key markup becomes own vector text/keycaps, never an atlas
// bitmap. Unsupported bitmap markup fails rather than leaking old pad icons.
HRESULT Build(const wchar_t* text,float size,float width,float height,ui_art::Scene&,float x=0,float y=0,D3DCOLOR ink=0xffe9e5dc,float* layoutHeight=nullptr)noexcept;
}
