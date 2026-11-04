#pragma once
#include <d3d9.h>
#include <cstdint>
#include <vector>
namespace sss_native_recipes {
inline constexpr UINT MissingRegister=~0u;
struct ReplayInfo {
    std::uint64_t originalFnv{};
    UINT originalBytes{},materialBaseRegister{},specularStrengthRegister{};
    UINT reflectionRegister=MissingRegister;
    // CTAB calls this gMaterialEmissive. Its CPU meaning may include authored
    // base/ambient contributions: preserve it unless separately authenticated.
    UINT materialEmissionRegister{},controlRegister=223;
    bool hasCharacterLighting=false,preservesNativeAlpha=false;
    // The RGBE exponent calculation remains, but zeroing spec/reflection can
    // change its value. Accept only draws whose ORIGINAL alpha test is off;
    // authored shader texkill remains exact. Do not silently disable an
    // original fixed-function alpha test and broaden geometry coverage.
    bool requiresNativeAlphaTestOff=true;
};
// Exact original whole-bytecode FNV+size allowlist. No device/state mutation.
// S_OK: own clone ready; S_FALSE: unsupported source, keep original rendering.
// Caller supplies c223=(1,approvedGroupNormalized,0,0) only during own replay.
// RT0: pre-RGBE linear RGB, retained packed-alpha calculation.
// RT1:1,group,exact original native RT3.x,0.
// Zero specularStrengthRegister.x / optional reflectionRegister.x on the
// replay's COPIED constants; preserve all light and emission/base uniforms.
// hasCharacterLighting=false is not an established lit-diffuse component.
// Targets also requires copied material emission/base RGB to be exactly zero
// before using this component; unknown nonzero terms select mask-only replay.
HRESULT BuildReplayShader(const DWORD* original,UINT bytes,std::vector<DWORD>& clone,ReplayInfo&)noexcept;
struct GeometryInfo {UINT colorSampler=MissingRegister,uvSource{},materialBaseRegister{},materialEmissionRegister{},depthSemantic{},depthKind{},uvInstruction{};};
struct ViewInfo {UINT worldView{},worldViewProjection{};};
// Exact native skeletal material family. depthKind=0: signed view Z;
// depthKind=1: original effective Z = view Z + .1*native bump.
// Clones preserve the exact native expression at its original execution point.
bool KnownSkinGeometry(std::uint64_t hash,UINT bytes,GeometryInfo&)noexcept;
bool KnownSkinVertex(std::uint64_t hash,UINT bytes,ViewInfo&)noexcept;
bool KnownSkinPair(std::uint64_t psHash,UINT psBytes,std::uint64_t vsHash,UINT vsBytes,ViewInfo&)noexcept;
// Original CTAB gColorMapSampler at its proved native varying UV.
// Returns MissingRegister for every other whole-bytecode identity.
UINT KnownSkinColorSampler(std::uint64_t hash,UINT bytes)noexcept;
// Historical API name: adds any catalog-authenticated own skin UV mask.
// Caller authenticates the exact profile texture identity. Reserve unused s15
// and a genuinely unused temporary; c223 is reserved by our replay.
// maskOutput=0 for mask-only clones, 1 for forward diffuse MRT clones.
HRESULT AddEdwardUvMask(const DWORD* original,UINT bytes,std::vector<DWORD>& clone,UINT maskOutput=1)noexcept;
}
