#include "menu_video.h"
#include "menu_video_ipc.h"
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mftransform.h>
#include <codecapi.h>
#include <shlwapi.h>
#include <propvarutil.h>
#include <wrl/client.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <system_error>
#include <new>

namespace menu_video {
using Microsoft::WRL::ComPtr;
struct Decoder::Impl {
    ComPtr<IMFSourceReader> reader;
    unsigned width{}, height{};
    LONG stride{};
    std::int64_t duration{416667};
    bool mf{};
    DWORD stage{};
    unsigned matrix{},range{},chroma{},primaries{},transfer{};
    HRESULT Format() {
        ComPtr<IMFMediaType> type;
        HRESULT hr=reader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),&type);
        if(FAILED(hr))return hr;
        GUID subtype{};hr=type->GetGUID(MF_MT_SUBTYPE,&subtype);
        if(FAILED(hr)||subtype!=MFVideoFormat_NV12)return MF_E_INVALIDMEDIATYPE;
        hr=MFGetAttributeSize(type.Get(),MF_MT_FRAME_SIZE,&width,&height);
        if(FAILED(hr)||!width||!height||(width&3)||(height&1)||width>4096||height>4096||std::uint64_t(width)*height>16777216)return MF_E_INVALIDMEDIATYPE;
        UINT32 value=0;hr=type->GetUINT32(MF_MT_DEFAULT_STRIDE,&value);
        if(FAILED(hr)){hr=MFGetStrideForBitmapInfoHeader(MFVideoFormat_NV12.Data1,width,&stride);if(FAILED(hr))return hr;}
        else stride=static_cast<LONG>(value);
        if(stride<=0||static_cast<unsigned>(stride)<width)return MF_E_INVALIDMEDIATYPE;
        matrix=range=chroma=primaries=transfer=0;
        type->GetUINT32(MF_MT_YUV_MATRIX,&matrix);type->GetUINT32(MF_MT_VIDEO_NOMINAL_RANGE,&range);
        type->GetUINT32(MF_MT_VIDEO_CHROMA_SITING,&chroma);type->GetUINT32(MF_MT_VIDEO_PRIMARIES,&primaries);type->GetUINT32(MF_MT_TRANSFER_FUNCTION,&transfer);
        UINT32 rate=0,scale=0;
        if(SUCCEEDED(MFGetAttributeRatio(type.Get(),MF_MT_FRAME_RATE,&rate,&scale))&&rate&&scale)
            duration=static_cast<std::int64_t>(10000000ull*scale/rate);
        return S_OK;
    }
    HRESULT BoundDecoder(){
        // Configure the real decoder, before its first input sample. Automatic
        // threading allocates a large 4K per-thread picture pool in 32-bit hosts.
        // Do not alter resolution, frame rate or the system color converter.
        stage=11;
        ComPtr<IMFSourceReaderEx> extended;HRESULT hr=reader.As(&extended);if(FAILED(hr))return hr;
        ComPtr<IMFTransform> decoder;GUID category{};
        hr=extended->GetTransformForStream(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),0,&category,&decoder);
        if(FAILED(hr))return hr;
        if(category!=MFT_CATEGORY_VIDEO_DECODER)return MF_E_INVALIDMEDIATYPE;
        ComPtr<IMFAttributes> attributes;hr=decoder->GetAttributes(&attributes);if(FAILED(hr))return hr;
        hr=attributes->SetUINT32(CODECAPI_AVDecNumWorkerThreads,2);if(FAILED(hr))return hr;
        // Prerecorded H264 may contain B-frames. Low-latency mode exposes decode
        // order and would break presentation timestamps; retain normal reorder.
        hr=attributes->SetUINT32(CODECAPI_AVLowLatencyMode,FALSE);if(FAILED(hr))return hr;
        UINT32 threads=0,low=0;
        hr=attributes->GetUINT32(CODECAPI_AVDecNumWorkerThreads,&threads);if(FAILED(hr)||threads!=2)return E_UNEXPECTED;
        hr=attributes->GetUINT32(CODECAPI_AVLowLatencyMode,&low);return SUCCEEDED(hr)&&low==FALSE?S_OK:E_UNEXPECTED;
    }
};
Decoder::Decoder():impl_(new Impl){}
Decoder::~Decoder(){Close();}
void Decoder::Close(){impl_->reader.Reset();if(impl_->mf){MFShutdown();impl_->mf=false;}}
HRESULT Decoder::Open(const wchar_t* path){
    Close();
    impl_->stage=10;
    // No URL/network input; this component only reads a local existing regular file.
    if(!path||!*path||PathIsURLW(path)||PathIsUNCW(path))return E_INVALIDARG;
    const DWORD attr=GetFileAttributesW(path);
    if(attr==INVALID_FILE_ATTRIBUTES)return HRESULT_FROM_WIN32(GetLastError());
    if(attr&FILE_ATTRIBUTE_DIRECTORY)return E_INVALIDARG;
    HRESULT hr=MFStartup(MF_VERSION,MFSTARTUP_FULL);if(FAILED(hr))return hr;impl_->mf=true;
    ComPtr<IMFAttributes> attributes;hr=MFCreateAttributes(&attributes,2);
    if(SUCCEEDED(hr))hr=attributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING,FALSE);
    if(SUCCEEDED(hr))hr=attributes->SetUINT32(MF_READWRITE_DISABLE_CONVERTERS,FALSE);
    if(SUCCEEDED(hr))hr=MFCreateSourceReaderFromURL(path,attributes.Get(),&impl_->reader);
    // Critical: do not decode, create a sink for, or play any audio stream.
    if(SUCCEEDED(hr))hr=impl_->reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS),FALSE);
    if(SUCCEEDED(hr))hr=impl_->reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),TRUE);
    ComPtr<IMFMediaType> output;
    if(SUCCEEDED(hr))hr=MFCreateMediaType(&output);
    if(SUCCEEDED(hr))hr=output->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Video);
    if(SUCCEEDED(hr))hr=output->SetGUID(MF_MT_SUBTYPE,MFVideoFormat_NV12);
    if(SUCCEEDED(hr))hr=impl_->reader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),nullptr,output.Get());
    if(SUCCEEDED(hr))hr=impl_->BoundDecoder();
    if(SUCCEEDED(hr))hr=impl_->Format();
    if(FAILED(hr))Close();
    return hr;
}
HRESULT Decoder::Rewind(){
    impl_->stage=30;
    if(!impl_->reader)return MF_E_NOT_INITIALIZED;
    PROPVARIANT zero{};InitPropVariantFromInt64(0,&zero);
    return impl_->reader->SetCurrentPosition(GUID_NULL,zero);
}
HRESULT Decoder::Read(Frame& frame,bool& end){
    end=false;if(!impl_->reader)return MF_E_NOT_INITIALIZED;
    for(unsigned attempts=0;attempts<32;++attempts){
        ComPtr<IMFSample> sample;DWORD flags=0;LONGLONG time=0;
        impl_->stage=20;HRESULT hr=impl_->reader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),0,nullptr,&flags,&time,&sample);
        if(FAILED(hr))return hr;
        if(flags&MF_SOURCE_READERF_ERROR)return E_FAIL;
        if(flags&MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED){hr=impl_->Format();if(FAILED(hr))return hr;}
        if(flags&MF_SOURCE_READERF_ENDOFSTREAM){end=true;return S_FALSE;}
        if(!sample)continue;
        impl_->stage=21;ComPtr<IMFMediaBuffer> buffer;hr=sample->ConvertToContiguousBuffer(&buffer);if(FAILED(hr))return hr;
        ComPtr<IMF2DBuffer> twoD;buffer.As(&twoD);
        const auto needed=static_cast<std::size_t>(impl_->width)*impl_->height*3/2;
        impl_->stage=23;
        try{frame.pixels.resize(needed);}catch(const std::bad_alloc&){return E_OUTOFMEMORY;}catch(...){return E_UNEXPECTED;}
        impl_->stage=24;
        if(twoD){
            DWORD contiguous=0;hr=twoD->GetContiguousLength(&contiguous);if(FAILED(hr))return hr;
            if(contiguous!=needed)return MF_E_INVALIDMEDIATYPE;
            hr=twoD->ContiguousCopyTo(frame.pixels.data(),static_cast<DWORD>(needed));if(FAILED(hr))return hr;
        }else{
            BYTE* pixels=nullptr;DWORD current=0;hr=buffer->Lock(&pixels,nullptr,&current);if(FAILED(hr))return hr;
            const bool valid=pixels&&current>=needed;if(valid)std::memcpy(frame.pixels.data(),pixels,needed);
            buffer->Unlock();if(!valid)return MF_E_BUFFERTOOSMALL;
        }
        frame.width=impl_->width;frame.height=impl_->height;frame.timestamp100ns=time;
        frame.matrix=impl_->matrix;frame.nominalRange=impl_->range;frame.chromaSiting=impl_->chroma;frame.primaries=impl_->primaries;frame.transfer=impl_->transfer;
        LONGLONG duration=0;
        frame.duration100ns=SUCCEEDED(sample->GetSampleDuration(&duration))&&duration>0?duration:impl_->duration;
        return S_OK;
    }
    return MF_E_INVALID_STREAM_DATA;
}
DWORD Decoder::Stage()const noexcept{return impl_->stage;}
#include "menu_video_client.inl"
}
