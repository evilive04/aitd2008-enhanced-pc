#pragma once
#include <d3d9.h>
#include <memory>
#include "menu_state.h"
#include "menu_video.h"
namespace menu_presentation {
struct Diagnostic {
    menu_video::Diagnostic media;
    DWORD stage{},exceptionKind{},exceptionCode{};
    HRESULT error{S_OK};
    char detail[160]{};
};
// Explicitly managed lifetime: never allocate/destroy or Start/Shutdown in DllMain.
// Construct once outside the loader lock; retain through native menu subpages.
class Presentation {
public:
    Presentation();~Presentation();
    Presentation(const Presentation&)=delete;Presentation& operator=(const Presentation&)=delete;
    bool Start(const wchar_t* localVideoPath);
    // Call with false when a caller skips Draw entirely outside native menus.
    void SetVisible(bool visible)noexcept;
    HRESULT SetCredits(const wchar_t* completeOriginalAndModCredits);
    // Does not restart/stop media decoding. Returns true only after a complete
    // frame and its interactive text are drawn and all native D3D state restored.
    bool Draw(IDirect3DDevice9* device,const MenuStateV1& state,MenuActionSender sender,bool backdropOnly=false)noexcept;
    // Call before native Reset and before releasing its owning device.
    void ReleaseDeviceResources()noexcept;
    void Shutdown();
    bool Ready()const noexcept;
    HRESULT Error()const noexcept;
    Diagnostic Diagnostics()const noexcept;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
