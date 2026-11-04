#pragma once
#include "native_ui_text.h"
namespace native_ui_text_mask {
struct Snapshot {bool active=false;std::vector<ui_vector_text::Triangle> triangles;};
// Bounded snapshot of global+1C4's float stack, with count from global+1BC.
// The final float is the current mask's starting float-word index. A true
// global+2F flag disables masking in the original renderer.
HRESULT Decode(const float* words,unsigned count,bool disabled,Snapshot& output) noexcept;
// Clip outline triangles in movie coordinates, before viewport scale/offset.
// Native 92E5E0 intersects each source triangle with each current mask triangle.
// No rectangle substitution and no alteration of the live mask stack.
HRESULT Clip(const std::vector<ui_vector_text::Triangle>& glyph,
             const native_ui_text::Matrix& glyphToMovie,const Snapshot& mask,
             unsigned maxTriangles,std::vector<ui_vector_text::Triangle>& output) noexcept;
}
