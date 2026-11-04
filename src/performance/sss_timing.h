#pragma once
#include <d3d9.h>
namespace sss_timing {
enum class GpuStatus {Unknown,Available,Unavailable,Error};
struct Summary {
 unsigned frames{},operationFrames{},cpuSamples{},gpuSamples{},pendingFrames{},droppedCpuFrames{},droppedGpuFrames{},unavailableGpuFrames{},allocatedQueries{};
 double cpuMeanMs{},cpuP95Ms{},gpuMeanMs{},gpuP95Ms{};
 GpuStatus gpuStatus=GpuStatus::Unknown;HRESULT lastGpuError=S_OK;
};
// Single render thread. No logging, flushing, blocking, or native-time ranges.
class Meter {
 struct Storage;Storage* data=nullptr;
public:
 Meter() noexcept=default;~Meter();Meter(const Meter&)=delete;Meter& operator=(const Meter&)=delete;
 void BeginFrame(IDirect3DDevice9*,bool enabled) noexcept;
 // Non-nested: call EndOperation only when BeginOperation returns true.
 bool BeginOperation() noexcept;
 void EndOperation() noexcept;
 void EndFrame() noexcept;
 Summary GetSummary() const noexcept;
 // Call before Reset and when switching OFF. Frees all query references.
 void Release() noexcept;
};
}
