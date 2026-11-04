#pragma once
#include <d3d9.h>
#include "dialog_context.h"
#include "ui_backdrop.h"
namespace dialog_presentation {
class Presentation {
    struct Impl;Impl* impl;
public:
    Presentation();~Presentation();
    Presentation(const Presentation&)=delete;Presentation& operator=(const Presentation&)=delete;
    bool Draw(IDirect3DDevice9*,const DialogStateV1&,DialogActionSender,ui_backdrop::Draw=nullptr) noexcept;
    void Hide() noexcept;
    void ReleaseDeviceResources() noexcept;
    bool OwnsDevice(IDirect3DDevice9*)const noexcept;
};
}
