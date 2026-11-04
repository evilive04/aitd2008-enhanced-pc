#pragma once
#include "pause_state.h"
#include <d3d9.h>
namespace pause_presentation {
class Presentation {
public:
    Presentation();~Presentation();
    Presentation(const Presentation&)=delete;Presentation& operator=(const Presentation&)=delete;
    bool Draw(IDirect3DDevice9*,const PauseStateV1&,PauseSender)noexcept;
    void ReleaseDeviceResources()noexcept;
    bool OwnsDevice(IDirect3DDevice9*)const noexcept;
    HRESULT BuildStatus()const noexcept;
private:struct Impl;Impl* impl;
};
}
