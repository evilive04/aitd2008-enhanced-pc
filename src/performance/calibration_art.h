#pragma once
#include "calibration_state.h"
#include "ui_art.h"
namespace calibration_art {
struct Prepared{ui_art::Scene scene;};
ui_art::Bounds Tab(unsigned)noexcept;
ui_art::Bounds Slider(unsigned row,unsigned page)noexcept;
ui_art::Bounds Footer(bool accept)noexcept;
HRESULT Prepare(const CalibrationStateV1&,Prepared&)noexcept;
bool Build(const CalibrationStateV1&,const Prepared&,ui_art::Scene&)noexcept;
}
