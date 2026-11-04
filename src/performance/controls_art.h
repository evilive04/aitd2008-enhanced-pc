#pragma once
#include "ui_art.h"
#include <cstdint>
#include <type_traits>
// Pure artwork boundary. Every caption, binding, device name, lock and gauge
// comes from an authenticated native snapshot. No process pointers or defaults.
namespace controls_art {
enum class Phase:std::uint32_t{Bindings=1,Settings=2,ReleaseKeys=3,CaptureKey=4,Conflict=5,Forbidden=6};
enum class Action:std::uint32_t{None,Focus,Activate,Category,Previous,Next,Gauge,Back,Confirm,Cancel,Reset,PagePrevious,PageNext,MovePrevious,MoveNext};
enum class Kind:std::uint32_t{Binding=1,Selector=2,Gauge=3};
struct Row{
 wchar_t caption[128]{},value[96]{},device[64]{};
 Kind kind=Kind::Binding;std::uint32_t enabled=1,locked{},warning{};
 double current{},minimum{},maximum{};
};
struct Footer{wchar_t caption[64]{},key[24]{};Action action=Action::None;std::uint32_t enabled{};};
struct View{
 std::uint32_t version=1;Phase phase=Phase::Bindings;
 wchar_t title[96]{},categories[4][64]{};
 std::uint32_t category{},count{},total{},offset{},selected{};
 Row rows[10]{};Footer footer[4]{};
 wchar_t modalTitle[96]{},modalBody[512]{};
};
static_assert(std::is_standard_layout<View>::value&&std::is_trivially_copyable<View>::value,"Copied controls artwork input");
struct Region{ui_art::Bounds bounds{};Action action=Action::None;std::int32_t row=-1;};
struct Layout{Region regions[64]{};std::uint32_t count{};};
struct Hit{Action action=Action::None;std::int32_t row=-1;double normalized{};};
ui_art::Bounds Category(unsigned)noexcept;
ui_art::Bounds Binding(unsigned)noexcept;
ui_art::Bounds Setting(unsigned)noexcept;
HRESULT Build(const View&,ui_art::Scene&,Layout&)noexcept;
Hit HitTest(const Layout&,float x,float y)noexcept;
}
