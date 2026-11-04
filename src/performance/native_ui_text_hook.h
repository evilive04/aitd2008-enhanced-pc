#pragma once
#include "native_ui_text.h"
namespace native_ui_text_hook {
struct DrawRequest {
    unsigned size=sizeof(DrawRequest);
    const native_ui_text::Vertex* vertices{};
    unsigned vertexCount{};
    unsigned codepoint{}, finalArgb{};
    std::uintptr_t nativeTexture{}, nativeGlobal{}, nativeBatch{};
};
// Executed on the native UI thread inside 92B170. Return true only after all
// vertices were appended successfully to the native queued triangle batch.
// A direct D3D draw here is not ordered against native display-list jobs.
// Callback must preflight native capacities and validate an opaque solid texel
// for nativeTexture before writing a single vertex. False preserves the glyph.
using Emit = bool (__cdecl*)(const DrawRequest*) noexcept;
bool Configure(void* textTrampoline,void* glyphTrampoline,Emit emit,bool preparedNativeContext=false) noexcept;
bool Install(bool authenticatedR29) noexcept;
// A fresh presenter may own the complete visual layer of one exact native root.
// This callback never suppresses update, input, AVM, mask construction or queries.
using PaintOwner = bool (__cdecl*)(std::uintptr_t nativeRoot) noexcept;
void SetPaintOwner(PaintOwner) noexcept;
// Narrow exception for authenticated original content / authored native markers.
// True keeps this exact shape's original draw; never used for masks or queries.
using ContentPaint = bool (__cdecl*)(std::uintptr_t nativeRoot,std::uintptr_t nativeShape) noexcept;
void SetContentPaint(ContentPaint) noexcept;
//925410 supplies the actual sprite instance as the third shape-paint argument.
// Use its ancestry for content shared by multiple instances of one definition.
using ContentInstancePaint = bool (__cdecl*)(std::uintptr_t nativeRoot,std::uintptr_t nativeShape,std::uintptr_t instance) noexcept;
void SetContentInstancePaint(ContentInstancePaint) noexcept;
// Native context+14 contains xx,yy,yx,xy,tx,ty. Only a positively authenticated
// visual subtree may remap it; non-paint queries retain their original context.
using ContentTransform = bool (__cdecl*)(std::uintptr_t nativeRoot,std::uintptr_t nativeShape,std::uintptr_t instance,float matrix[6]) noexcept;
void SetContentTransform(ContentTransform) noexcept;
using PointerOwner = void (__cdecl*)() noexcept;
void SetPointerOwner(PointerOwner) noexcept;
void* ShapeEntry() noexcept;
struct PaintStatistics {unsigned staticText{},dynamicText{},shapes{};};
PaintStatistics GetPaintStatistics() noexcept;
struct Statistics {unsigned textCalls{},acceptedTexts{},glyphsDrawn{},verticesDrawn{},budgetRejected{},geometryRejected{},emitFailed{};};
Statistics GetStatistics() noexcept;
void Disable() noexcept;
// Install using the existing authenticated r29 hook system. Full first
// instructions: text 55 8B EC 83 E4 F8 (6), glyph 8B 54 24 04 83 EC 2C (7).
void* TextEntry() noexcept;
void* GlyphEntry() noexcept;
void* MaskedEntry() noexcept;
void* DynamicEntry() noexcept;
void* LayoutEntry() noexcept;
void ConfigureDynamic(void* dynamicTrampoline) noexcept;
void CaptureNativeLayout(std::uintptr_t layout,std::uintptr_t text,std::uintptr_t context) noexcept;
}
