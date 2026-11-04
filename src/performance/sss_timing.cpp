#define NOMINMAX
#include "sss_timing.h"
#include <algorithm>
#include <new>
namespace sss_timing {
namespace {
constexpr unsigned Operations=32,Slots=4,History=128;
struct Pair {IDirect3DQuery9* begin=nullptr;IDirect3DQuery9* end=nullptr;};
struct Slot {IDirect3DQuery9* disjoint=nullptr;IDirect3DQuery9* frequency=nullptr;Pair pairs[Operations]{};ULONGLONG sequence{};unsigned operations{};bool pending{},valid{};};
struct Frame {ULONGLONG sequence{};double cpu{},gpu{};unsigned operations{};bool cpuValid=true,gpuValid{},pending{},gpuDropped{},gpuUnavailable{};};
template<class T>void Free(T*& object){if(object){object->Release();object=nullptr;}}
}
struct Meter::Storage {
 IDirect3DDevice9* device=nullptr;Slot slots[Slots]{};Frame history[History]{};unsigned used{},slotIndex{},allocatedQueries{};ULONGLONG sequence{};LARGE_INTEGER frequency{},cpuStart{};Frame current{};
 bool open{},operation{},gpuAllowed{},gpuStarted{},gpuOperation{},gpuValid=true;GpuStatus status=GpuStatus::Unknown;HRESULT error=S_OK;
 Storage(){if(!QueryPerformanceFrequency(&frequency))frequency.QuadPart=0;}
 ~Storage(){for(auto& s:slots){Free(s.disjoint);Free(s.frequency);for(auto& p:s.pairs){Free(p.begin);Free(p.end);}}}
 Frame* Find(ULONGLONG id){if(!id)return nullptr;auto& frame=history[(id-1)%History];return frame.sequence==id?&frame:nullptr;}
 void Failure(HRESULT hr){error=hr;status=hr==D3DERR_NOTAVAILABLE?GpuStatus::Unavailable:GpuStatus::Error;gpuAllowed=false;}
 bool Query(IDirect3DQuery9*& query,D3DQUERYTYPE type){if(query)return true;const auto hr=device->CreateQuery(type,&query);if(FAILED(hr)||!query){Failure(FAILED(hr)?hr:E_FAIL);return false;}++allocatedQueries;return true;}
 // Each call only polls once, always flags=0. A pending ring slot is never reused.
 void Poll(){for(auto& s:slots){if(!s.pending)continue;BOOL disjoint=FALSE;ULONGLONG frequencyGpu=0;auto hr=s.disjoint->GetData(&disjoint,sizeof disjoint,0);if(hr==S_FALSE)continue;
   if(FAILED(hr)){if(auto* f=Find(s.sequence)){f->pending=false;f->gpuDropped=true;}s.pending=false;Failure(hr);continue;}
   hr=s.frequency->GetData(&frequencyGpu,sizeof frequencyGpu,0);if(hr==S_FALSE)continue;if(FAILED(hr)){if(auto* f=Find(s.sequence)){f->pending=false;f->gpuDropped=true;}s.pending=false;Failure(hr);continue;}
   bool ready=true,valid=s.valid&&!disjoint&&frequencyGpu;double total=0;
   if(valid)for(unsigned n=0;n<s.operations;++n){ULONGLONG begin=0,end=0;const auto a=s.pairs[n].begin->GetData(&begin,sizeof begin,0),b=s.pairs[n].end->GetData(&end,sizeof end,0);if(a==S_FALSE||b==S_FALSE){ready=false;break;}if(FAILED(a)||FAILED(b)||end<begin){valid=false;break;}total+=static_cast<double>(end-begin)*1000./static_cast<double>(frequencyGpu);}
   if(!ready)continue;if(auto* f=Find(s.sequence)){f->pending=false;f->gpuValid=valid;f->gpuDropped=!valid;f->gpu=valid?total:0;}s.pending=false;
  }}
};
Meter::~Meter(){Release();}
void Meter::Release() noexcept {delete data;data=nullptr;}
void Meter::BeginFrame(IDirect3DDevice9* device,bool enabled) noexcept {
 if(!enabled){Release();return;}if(data&&data->open)EndFrame();if(data&&data->device!=device)Release();if(!data)data=new(std::nothrow)Storage;if(!data)return;
 auto& d=*data;d.device=device;d.Poll();d.current={};d.current.sequence=++d.sequence;d.slotIndex=static_cast<unsigned>((d.sequence-1)%Slots);d.open=true;d.operation=false;d.gpuStarted=false;d.gpuOperation=false;d.gpuValid=true;
 d.gpuAllowed=device&&(d.status==GpuStatus::Unknown||d.status==GpuStatus::Available);if(!device){d.status=GpuStatus::Unavailable;d.error=D3DERR_NOTAVAILABLE;}
 if(d.slots[d.slotIndex].pending){d.gpuAllowed=false;d.current.gpuDropped=true;}
}
bool Meter::BeginOperation() noexcept {
 if(!data||!data->open)return false;auto& d=*data;if(d.operation){d.current.cpuValid=false;d.gpuValid=false;return false;}if(d.current.operations>=Operations){d.current.cpuValid=false;d.gpuValid=false;d.current.gpuDropped=true;return false;}
 d.gpuOperation=false;auto& slot=d.slots[d.slotIndex];
 if(d.gpuAllowed){
  if(!d.gpuStarted){if(d.Query(slot.disjoint,D3DQUERYTYPE_TIMESTAMPDISJOINT)&&d.Query(slot.frequency,D3DQUERYTYPE_TIMESTAMPFREQ)){const auto hr=slot.disjoint->Issue(D3DISSUE_BEGIN);if(SUCCEEDED(hr))d.gpuStarted=true;else d.Failure(hr);}}
  if(d.gpuStarted&&d.gpuAllowed){auto& p=slot.pairs[d.current.operations];if(d.Query(p.begin,D3DQUERYTYPE_TIMESTAMP)&&d.Query(p.end,D3DQUERYTYPE_TIMESTAMP)){const auto hr=p.begin->Issue(D3DISSUE_END);if(SUCCEEDED(hr)){d.gpuOperation=true;d.status=GpuStatus::Available;}else d.Failure(hr);}}
  if(!d.gpuOperation)d.gpuValid=false;
 }
 ++d.current.operations;d.operation=true;if(!d.frequency.QuadPart||!QueryPerformanceCounter(&d.cpuStart))d.current.cpuValid=false;return true;
}
void Meter::EndOperation() noexcept {
 if(!data||!data->open||!data->operation)return;auto& d=*data;LARGE_INTEGER end{};if(d.current.cpuValid&&QueryPerformanceCounter(&end)&&end.QuadPart>=d.cpuStart.QuadPart)d.current.cpu+=static_cast<double>(end.QuadPart-d.cpuStart.QuadPart)*1000./static_cast<double>(d.frequency.QuadPart);else d.current.cpuValid=false;
 if(d.gpuOperation){const auto hr=d.slots[d.slotIndex].pairs[d.current.operations-1].end->Issue(D3DISSUE_END);if(FAILED(hr)){d.gpuValid=false;d.Failure(hr);}}d.operation=false;d.gpuOperation=false;
}
void Meter::EndFrame() noexcept {
 if(!data||!data->open)return;auto& d=*data;if(d.operation){EndOperation();d.current.cpuValid=false;d.gpuValid=false;}auto& slot=d.slots[d.slotIndex];
 if(d.gpuStarted){const auto a=slot.disjoint->Issue(D3DISSUE_END),b=slot.frequency->Issue(D3DISSUE_END);if(SUCCEEDED(a)&&SUCCEEDED(b)){slot.pending=true;slot.valid=d.gpuValid;slot.sequence=d.current.sequence;slot.operations=d.current.operations;d.current.pending=d.gpuValid;if(!d.gpuValid)d.current.gpuDropped=true;}else{d.current.gpuDropped=true;d.Failure(FAILED(a)?a:b);}}
 else if(!d.current.operations&&d.status==GpuStatus::Available){d.current.gpuValid=true;d.current.gpuDropped=false;}
 else if(!d.current.gpuDropped)d.current.gpuUnavailable=true;
 d.history[(d.current.sequence-1)%History]=d.current;if(d.used<History)++d.used;d.open=false;
}
Summary Meter::GetSummary() const noexcept {
 Summary out{};if(!data)return out;const auto& d=*data;out.frames=d.used;out.gpuStatus=d.status;out.lastGpuError=d.error;out.allocatedQueries=d.allocatedQueries;double cpu[History]{},gpu[History]{};
 for(const auto& f:d.history){if(!f.sequence)continue;if(f.operations)++out.operationFrames;if(f.cpuValid){cpu[out.cpuSamples++]=f.cpu;out.cpuMeanMs+=f.cpu;}else ++out.droppedCpuFrames;if(f.gpuValid){gpu[out.gpuSamples++]=f.gpu;out.gpuMeanMs+=f.gpu;}if(f.pending)++out.pendingFrames;if(f.gpuDropped)++out.droppedGpuFrames;if(f.gpuUnavailable)++out.unavailableGpuFrames;}
 if(out.cpuSamples){out.cpuMeanMs/=out.cpuSamples;std::sort(cpu,cpu+out.cpuSamples);out.cpuP95Ms=cpu[(out.cpuSamples*95+99)/100-1];}if(out.gpuSamples){out.gpuMeanMs/=out.gpuSamples;std::sort(gpu,gpu+out.gpuSamples);out.gpuP95Ms=gpu[(out.gpuSamples*95+99)/100-1];}return out;
}
}
