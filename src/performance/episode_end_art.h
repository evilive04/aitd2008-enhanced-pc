#pragma once
#include "episode_end_state.h"
#include "ui_art.h"
namespace episode_end_art {
ui_art::Bounds Row(unsigned)noexcept;
HRESULT Build(const EpisodeEndStateV1&,ui_art::Scene&)noexcept;
}
