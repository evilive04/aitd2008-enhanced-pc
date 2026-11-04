#pragma once
#include <cstddef>
#include <cstdint>
namespace skin_material_catalog {
// Exact native D3D descriptor plus logical packed mip bytes (pitch excluded).
// FNV includes the original encoded texels, not a rendered/decoded image.
struct Identity {
 unsigned width{},height{},format{},pool{},mip{},mipWidth{},mipHeight{},bytes{};
 std::uint64_t fnv1a64{};
};
enum class Kind {AuthoredUv,ProvenWholeSkin};
struct Profile {unsigned index{};const char* name=nullptr;Kind kind=Kind::AuthoredUv;};
// Returns approved profiles only. Unknown/explicitly excluded assets fail closed.
const Profile* Match(const Identity&) noexcept;
float Strength(const Profile*,float u,float v) noexcept;
// Own A8 mask, no source texture pixels. Profiles have stable index<=254;
// caller may use group=index+1. UV edges and excluded islands stay conservative.
bool BuildMask(const Profile*,unsigned width,unsigned height,unsigned char* output,std::size_t bytes) noexcept;
unsigned ProfileCount() noexcept;
unsigned ApprovedIdentityCount() noexcept;
}
