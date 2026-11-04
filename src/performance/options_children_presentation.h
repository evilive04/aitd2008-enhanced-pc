#pragma once
#include <d3d9.h>
#include "options_children_context.h"
#include "ui_backdrop.h"
#include "phone_portraits_state.h"
namespace options_children_presentation {
class Presentation {
    struct Impl;Impl* impl;
public:
    Presentation();~Presentation();
    Presentation(const Presentation&)=delete;Presentation& operator=(const Presentation&)=delete;
    bool Draw(IDirect3DDevice9*,const OptionsChildrenStateV1&,OptionsChildrenSender,const wchar_t*,ui_backdrop::Draw=nullptr,PhonePortraitGetter=nullptr)noexcept;
    void ReleaseDeviceResources()noexcept;
    bool OwnsDevice(IDirect3DDevice9*)const noexcept;
};
}
