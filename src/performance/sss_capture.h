#pragma once
#include <d3d9.h>
namespace sss_capture {
// Render-thread only. Never changes device state or performs GPU readback.
// Known facial PS samplers2..5, or targeted water samplers0..3, may copy one
// bounded MANAGED CPU mip read-only. Cube previews contain six native faces.
// Call EndFrame then BeginFrame at Present, before custom overlays.
class Recorder {
 struct Storage;Storage* data=nullptr;IDirect3DDevice9* device=nullptr;
 bool previousRequest=false,armed=false,active=false,suspended=false;
 bool previousWaterRequest=false,waterArmed=false,waterEligible=false,waterPrepared=false,lastWater=false;
 struct Probe {ULONG_PTR object{};bool water{};};Probe probes[128]{};unsigned probeUsed{},probeNext{};
 DWORD generation{},error{};wchar_t directory[1024]{};
 bool Prepare() noexcept;void Start(IDirect3DDevice9*,bool water) noexcept;bool WaterTarget(IDirect3DDevice9*) noexcept;
public:
 Recorder() noexcept=default;~Recorder();Recorder(const Recorder&)=delete;Recorder& operator=(const Recorder&)=delete;
 // Third flag gates normal SSS only. Parent gates waterRequested by supported
 // executable/foreground; water then waits for exact PS and main target.
 // If both arm together, targeted water wins and consumes this one request.
 void BeginFrame(IDirect3DDevice9*,bool captureRequested,bool eligibleWorld,bool waterRequested=false) noexcept;
 void BeforeDraw(IDirect3DDevice9*,const char* kind,D3DPRIMITIVETYPE,unsigned primitiveCount,unsigned vertexCount=0) noexcept;
 void EndFrame() noexcept;
 void Suspend(bool value) noexcept {suspended=value;}
 bool Active() const noexcept {return active&&!suspended;}
 bool LastWasWater() const noexcept {return lastWater;}
 bool WaitingForWater() const noexcept {return waterArmed;}
 DWORD LastError() const noexcept {return error;}
 const wchar_t* LastDirectory() const noexcept {return directory;}
};
}
