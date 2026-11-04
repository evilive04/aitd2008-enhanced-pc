#pragma once
#include <windows.h>
#include <cstdint>
#include <vector>

namespace ui_vector_text {
struct Point { float x, y; };
struct Triangle { Point a, b, c; };
struct Bounds { float left{}, top{}, right{}, bottom{}; };
struct InlineSpan{unsigned start{},length=1,id{};float width{},height{},baseline{};};
struct InlinePlacement{unsigned id{};float x{},y{},width{},height{};};
struct Mesh {
    std::vector<Triangle> triangles;
    Bounds ink{};
    float advanceWidth{}, layoutHeight{};
    unsigned lineCount{};
    std::vector<InlinePlacement> inlineObjects;
};
struct Options {
    const wchar_t* family=L"Georgia";
    float size=36.f;
    float width=1920.f, height=1080.f;
    // Flattening error in DIPs: at 2x pixel scale use half the DIP tolerance.
    float tolerance=.15f;
    bool bold=false, italic=false;
    unsigned maxTriangles=262144;
    const InlineSpan* inlineSpans=nullptr;unsigned inlineCount{};
};
// Complete UTF-16 layout through DirectWrite. Local D2D single-thread factory;
// no retained COM state, font files, textures, GPU resources or COM apartment.
// On error output is cleared. Caller transforms returned DIP triangles to D3D9.
HRESULT Build(const wchar_t* text, std::uint32_t length,
              const Options& options, Mesh& output) noexcept;
}
