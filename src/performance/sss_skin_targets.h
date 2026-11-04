#pragma once
#include <d3d9.h>
#include <cstdint>
#include <memory>
namespace sss_skin_targets {
// Hook/vtable indices. Geometry replay is AFTER a successful original draw.
// Light replay can explicitly precede original when native stencil is destructive.
// Caller holds its recursion guard and separately authenticates actual skin textures.
struct Draw {
    UINT kind=81;
    D3DPRIMITIVETYPE type=D3DPT_TRIANGLELIST;
    UINT startVertex{},primitiveCount{};
    INT baseVertexIndex{};
    UINT minVertexIndex{},numVertices{},startIndex{};
    const void* vertexData=nullptr;
    const void* indexData=nullptr;
    UINT stride{};
    D3DFORMAT indexFormat=D3DFMT_UNKNOWN;
};
enum class Status { Disabled, Unapproved, Unsupported, NativeState, Matrices,
    FrameMismatch, ResourceFailure, ReplayFailure, Ready };
class Targets {
public:
    Targets();
    ~Targets();
    Targets(const Targets&)=delete;
    Targets& operator=(const Targets&)=delete;
    // Release before native Reset. Off releases all DEFAULT pool allocations.
    void BeginFrame(IDirect3DDevice9*,std::uint64_t serial,bool enabled) noexcept;
    void Release() noexcept;
    bool Replay(IDirect3DDevice9*,const Draw&,bool explicitApproved,BYTE group,bool knownUvMask=false) noexcept;
    // Once-per-texture cached READONLY exact catalog mip identity. Unknown materials
    // fail closed. Optional group receives stable catalog profile index+1.
    // Original alpha-test must be OFF; per-draw groups may differ within a frame.
    bool ApproveKnownSkin(IDirect3DDevice9*,BYTE* group=nullptr) noexcept;
    // Caller approval MUST mean the current whole-bytecode hash+size is an
    // independently reviewed diffuse-only light PS. This method does not infer
    // diffuse semantics from missing pow instructions or CTAB names. For a
    // Captured destructive EQUAL/ref1 KEEP-INCR-DECR profile requires beforeNative;
    // own replay temporarily uses KEEP/writeMask0, original consumes stencil once.
    // For a reviewed mixed light shader, approval must also attest the exact optional
    // specular register. Only copied RGB is zeroed; W and all other uniforms stay.
    bool ReplayDiffuseLight(IDirect3DDevice9*,const Draw&,bool explicitApproved,UINT sceneDepthSampler,UINT zeroSpecularRgbRegister=~0u,bool beforeNative=false) noexcept;
    IDirect3DTexture9* CombinedDiffuse() const noexcept;
    bool MatchesBaseRadiance(IDirect3DBaseTexture9*) const noexcept;
    // Borrowed until BeginFrame/Release. Null unless successfully written THIS frame.
    IDirect3DTexture9* Mask() const noexcept;
    IDirect3DTexture9* ForwardDiffuse() const noexcept;
    IDirect3DTexture9* Depth() const noexcept;
    std::uint64_t FrameSerial() const noexcept;
    UINT Width() const noexcept;
    UINT Height() const noexcept;
    float ProjectionScaleX() const noexcept;
    float ProjectionScaleY() const noexcept;
    float DepthSign() const noexcept;
    bool Ready() const noexcept;
    Status LastStatus() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
// Pure matrix verification, shared with tests. Rows match native dp4 layout.
bool DeriveProjection(const float wvp[16],const float worldView[12],
                      float& scaleX,float& scaleY,float& depthSign) noexcept;
}
