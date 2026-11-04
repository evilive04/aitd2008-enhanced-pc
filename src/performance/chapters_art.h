#pragma once
#include "chapters_state.h"
#include "ui_art.h"
namespace chapters_art {
enum class Action{None,Back,PreviousChapter,NextChapter,ChapterCard,Focus,Activate,MovePrevious,MoveNext,PagePrevious,PageNext};
struct Hit{Action action=Action::None;LONG row=-1;};
ui_art::Bounds Preview(unsigned)noexcept;
ui_art::Bounds Row(unsigned)noexcept;
unsigned NativePreview(unsigned visual,unsigned selectedCard)noexcept;
Hit HitTest(const ChaptersStateV1&,float,float,unsigned selectedCard=1)noexcept;
// Image rectangles intentionally have no bitmap/placeholder fill. The final
// presenter must supply authentic original preview content before ownership.
HRESULT Build(const ChaptersStateV1&,ui_art::Scene&,unsigned selectedCard=1)noexcept;
}
