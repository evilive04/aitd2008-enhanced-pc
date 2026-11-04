#pragma once
#include <d3d9.h>
#include <vector>
#include "ui_vector_text.h"
#include "ui_typeface.h"
namespace ui_theme {
struct Point{float x,y;};
struct Vertex{float x,y,z,rhw;D3DCOLOR color;};
using Mesh=std::vector<Vertex>;
struct Label{ui_vector_text::Mesh mesh;struct Edge{Point a,b;};std::vector<Edge> edges;};
void Triangle(Mesh&,Point,Point,Point,D3DCOLOR);
void Rect(Mesh&,float,float,float,float,D3DCOLOR);
// Smoke panel with a real alpha falloff; the legacy bool is ABI-only, never roots.
void Panel(Mesh&,float,float,float,float,bool legacyDecoration=false);
void Button(Mesh&,float,float,float,float,bool selected,bool enabled);
HRESULT BuildLabel(const wchar_t*,float,float,float,Label&,const wchar_t* family=ui_typeface::Body,unsigned triangleBudget=196608);
HRESULT BuildRichLabel(const wchar_t*,const ui_vector_text::Options&,Label&);
void Text(Mesh&,const Label&,float,float,D3DCOLOR);
void Cursor(Mesh&,float,float);
// Complete state restoration. Caller must be outside a scene; leaves no GPU resources.
bool Draw(IDirect3DDevice9*,const Mesh&,float scale,float offsetX,float offsetY);
}
