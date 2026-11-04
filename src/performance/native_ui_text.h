#pragma once
#include "ui_vector_text.h"
#include <cstddef>
#include <cstdint>
#include "ui_typeface.h"
#include <vector>

namespace native_ui_text {
struct Bytes { const std::uint8_t* data{}; std::size_t size{}; };
struct Matrix {
    float xx=1, yx=0, xy=0, yy=1, tx=0, ty=0;
    ui_vector_text::Point Apply(ui_vector_text::Point p) const noexcept;
};
struct Glyph {
    std::uint32_t codepoint{}, nativeColor{};
    std::uint16_t font{};
    std::uint8_t nativeIndex{};
    std::int16_t advance{};
    // Native authored pen and glyph scale, not DirectWrite reflow/kerning.
    Matrix transform;
};
using FontResolver = bool (*)(void*, std::uint16_t, Bytes&) noexcept;
struct Capture { std::vector<Glyph> glyphs; };
// Decodes a bounded snapshot of the exact common 0x928410 command stream.
// Font snapshots are complete native type-10 records, not atlas pixels.
// No process pointers are dereferenced. Failure atomically clears output.
HRESULT Decode(Bytes text, FontResolver resolver, void* user, Capture& output) noexcept;
struct LayoutRecord { std::int32_t codepoint{},advance{},x{},y{}; };
struct LayoutLine { unsigned first{},last{}; };
// Reflow completed native lines with the shared body typeface's natural advances; one uniform
// scale per field, never independent glyph stretching. Native records are read-only.
HRESULT Reflow(const std::vector<LayoutRecord>& records,const std::vector<LayoutLine>& lines,
               float left,float right,float sx,float sy,unsigned alignment,
               std::vector<Matrix>& drawableTransforms) noexcept;
struct Rect { float left{}, top{}, right{}, bottom{}; };
struct Vertex { float x{}, y{}; std::uint32_t color{}; };
struct Geometry { std::vector<Vertex> triangles; };
struct RenderContext {
    Matrix parent; Rect clip;
    // Must be positively established by the runtime adapter. A rectangular
    // scissor is insufficient for native polygon mask stacks.
    bool rectangularClipProven=false;
    bool colorResolved=false;
};
using ColorResolver = bool (*)(void*, std::uint32_t native, std::uint32_t& argb) noexcept;
// Builds installed-font outlines at the native 1024-unit em. Original advance,
// baseline, rotation and authored glyph scales are retained. Caller supplies
// resolved color/parent transform/clip; unsupported state returns E_NOTIMPL.
HRESULT Build(const Capture& text, const RenderContext& context,
              ColorResolver colors, void* user, Geometry& output,
              const wchar_t* family=ui_typeface::Body) noexcept;
}
