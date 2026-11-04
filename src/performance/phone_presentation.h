#pragma once
#include "phone_state.h"
#include <d3d9.h>
namespace ui_art {class Assets;}
namespace phone_presentation {
bool LoadUiImage(IDirect3DDevice9*,ui_art::Assets&,const wchar_t*,unsigned,unsigned,unsigned slot=0);
class Presentation {
public:
 Presentation();~Presentation();
 Presentation(const Presentation&)=delete;Presentation& operator=(const Presentation&)=delete;
 bool Draw(IDirect3DDevice9*,const PhoneStateV1&,PhoneSender)noexcept;
 void ReleaseDeviceResources()noexcept;
 bool OwnsDevice(IDirect3DDevice9*)const noexcept;
private:struct Impl;Impl* impl;
};
}
