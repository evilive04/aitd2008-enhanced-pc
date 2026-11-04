#pragma once
#include "phone_content_state.h"
#include "ui_art.h"
namespace phone_content_art {
struct Prepared{ui_art::Scene chrome,body;float bodyHeight{};};
ui_art::Bounds Row(unsigned index)noexcept;
ui_art::Bounds DialKey(unsigned index)noexcept;
HRESULT Prepare(const PhoneContentView&,Prepared&)noexcept;
// Body geometry is clipped to its own viewport; long messages scroll without
// shrinking fonts or changing background alpha. scroll is canonical DIPs.
void Build(const PhoneContentView&,const Prepared&,float scroll,ui_art::Scene&);
bool PlainBody(const wchar_t* rich,wchar_t(&plain)[4096])noexcept;
}
