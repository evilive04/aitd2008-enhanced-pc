#pragma once
#include "loading_state.h"
#include "ui_art.h"
namespace loading_art {
struct Prepared {ui_art::Scene scene;bool full{},information{};unsigned picture{};ui_art::Bounds imageBounds{};};
HRESULT Prepare(const LoadingStateV3&,Prepared&)noexcept;
void Build(const Prepared&,DWORD elapsed,ui_art::Scene&);
}
