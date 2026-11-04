#pragma once
#include <d3d9.h>
namespace ui_backdrop {
// Draw the authenticated menu video, or leave the real gameplay target intact.
// False aborts presentation; never cover missing content with an opaque plate.
using Draw = bool (*)(IDirect3DDevice9*);
}
