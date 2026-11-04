#pragma once
#include "recap_state.h"
#include "ui_art.h"
namespace recap_art {
// Authored source movie is1280x720. This is the untouched video rectangle
// converted to the same1920x1080 canvas; no texture/geometry is stretched.
inline ui_art::Bounds Video()noexcept{return {283.95f,247.125f,660.025f,372.028f};}
ui_art::Bounds Button(unsigned action)noexcept;
unsigned Hit(float,float)noexcept;
void Highlight(ui_art::Scene&,unsigned selected)noexcept;
HRESULT Build(const RecapStateV1&,unsigned selected,ui_art::Scene&)noexcept;
}
