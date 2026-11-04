#pragma once
#include "ui_art.h"
#include <cstdint>
#include <type_traits>

// Caller-fed drawing boundary only: no native pointers, native reads or writes.
namespace options_children_art {
enum class Page : std::uint32_t { Audio=1, Languages=2, Display=3, PhoneParameters=4, PhoneMessages=5, PhoneContacts=6 };
enum class Kind : std::uint32_t { Selector=1, Gauge=2, CheckBox=3, Navigation=4 };
enum class Action : std::uint32_t { None, FocusRow, Previous, Next, Gauge, Confirm, Cancel, Reset, Calibration, Activate };
struct Row {
    wchar_t label[128]{},value[128]{}; // Navigation value is its real native route ID.
    Kind kind=Kind::Selector;
    std::uint32_t enabled=1,locked=0,checked=0;
    double current{},minimum{},maximum{}; // Actual finite native domain; never a guessed 0..100.
};
struct Footer {
    wchar_t label[64]{},key[24]{}; // Localized action and actual Enhanced PC binding.
    Action action=Action::None;
    std::uint32_t enabled=1;
};
struct View {
    std::uint32_t version=1;
    Page page=Page::Audio;
    wchar_t title[96]{},infoTitle[96]{},information[768]{};
    std::uint32_t count=3;
    std::int32_t selected=-1;
    Row rows[7]{};
    Footer footer[4]{};
};
static_assert(std::is_standard_layout<View>::value&&std::is_trivially_copyable<View>::value,"Owned pointer-free snapshot");
struct Region { ui_art::Bounds bounds{};Action action=Action::None;std::int32_t row=-1; };
struct TextSizes { float title{},infoTitle{},information{},row[7]{},value[7]{},footer[4]{},key[4]{}; };
struct GaugeKnob { std::uint32_t present{},batch{},firstVertex{};float trackX{},trackWidth{},centerY{}; };
struct Layout { Region regions[40]{};std::uint32_t count{};TextSizes text{};GaugeKnob knobs[7]{}; };
struct Hit { Action action=Action::None;std::int32_t row=-1;float normalized{}; };
// Builds coherent canonical1920x1080 artwork transactionally. On failure both outputs clear.
// Cache per snapshot generation; outline text is not intended to rebuild every render tick.
HRESULT Build(const View&,ui_art::Scene&,Layout&) noexcept;
// Use only with the Scene/Layout built for oldView. No allocations or glyph work.
// Returns false without mutation for any non-current change or stale knob geometry.
// On success caller replaces its cached oldView with newView; otherwise full Build.
bool UpdateGaugeCurrents(const View& oldView,const View& newView,ui_art::Scene&,const Layout&) noexcept;
Hit HitTest(const Layout&,float canonicalX,float canonicalY) noexcept;
ui_art::Bounds RowBounds(Page,unsigned index) noexcept;
ui_art::Bounds RosterPortrait()noexcept;
}
