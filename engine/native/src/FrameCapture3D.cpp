#include "Aevrynd/FrameCapture3D.h"
#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <limits>

using Microsoft::WRL::ComPtr;
namespace Aevrynd {
namespace {
struct CaptureComApartment3D {HRESULT result{CoInitializeEx(nullptr,COINIT_MULTITHREADED)};~CaptureComApartment3D(){if(result==S_OK||result==S_FALSE)CoUninitialize();}};
}

FrameCaptureAnalysis3D AnalyzeFrameCaptureRgba8(unsigned width,unsigned height,std::size_t rowPitch,std::span<const std::uint8_t> pixels){
    FrameCaptureAnalysis3D result{};if(!width||!height||width>MaximumFrameCaptureDimension3D||height>MaximumFrameCaptureDimension3D||rowPitch<std::size_t(width)*4||std::size_t(height-1)>(std::numeric_limits<std::size_t>::max)()/rowPitch)return result;const std::size_t required=std::size_t(height-1)*rowPitch+std::size_t(width)*4;if(required>pixels.size())return result;
    const std::size_t pixelCount=std::size_t(width)*height,sampleStride=(std::max)(std::size_t{1},(pixelCount+MaximumFrameAnalysisSamples3D-1)/MaximumFrameAnalysisSamples3D);std::size_t dark{},bright{};double mean{},m2{};
    for(std::size_t index=0;index<pixelCount;index+=sampleStride){const std::size_t y=index/width,x=index-y*width,offset=y*rowPitch+x*4;const double red=pixels[offset]/255.0,green=pixels[offset+1]/255.0,blue=pixels[offset+2]/255.0,luminance=.2126*red+.7152*green+.0722*blue;dark+=luminance<.02;bright+=luminance>.98;++result.sampledPixels;const double delta=luminance-mean;mean+=delta/double(result.sampledPixels);m2+=delta*(luminance-mean);}
    if(!result.sampledPixels)return result;result.valid=true;result.averageLuminance=mean;result.luminanceVariance=result.sampledPixels>1?m2/double(result.sampledPixels-1):0;result.darkPixelFraction=double(dark)/result.sampledPixels;result.brightPixelFraction=double(bright)/result.sampledPixels;return result;
}

bool EncodeFrameCapturePng3D(const std::filesystem::path& path,unsigned width,unsigned height,std::size_t rowPitch,std::span<const std::uint8_t> pixels){
    if(path.empty()||!width||!height||width>MaximumFrameCaptureDimension3D||height>MaximumFrameCaptureDimension3D||rowPitch<std::size_t(width)*4||rowPitch>(std::numeric_limits<UINT>::max)())return false;
    const std::size_t lastRowBytes=std::size_t(width)*4;if(std::size_t(height-1)>(std::numeric_limits<std::size_t>::max)()/rowPitch)return false;const std::size_t required=std::size_t(height-1)*rowPitch+lastRowBytes;if(required>pixels.size()||pixels.size()>(std::numeric_limits<UINT>::max)())return false;
    std::error_code error;const std::filesystem::path parent=path.parent_path();if(!parent.empty()){std::filesystem::create_directories(parent,error);if(error)return false;}std::filesystem::path temporary=path;temporary+=L"."+std::to_wstring(GetCurrentProcessId())+L".tmp";std::filesystem::remove(temporary,error);error.clear();
    CaptureComApartment3D apartment;ComPtr<IWICImagingFactory> factory;ComPtr<IWICStream> stream;ComPtr<IWICBitmapEncoder> encoder;ComPtr<IWICBitmapFrameEncode> frame;ComPtr<IPropertyBag2> properties;ComPtr<IWICBitmap> bitmap;bool success=false;
    if(SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))&&SUCCEEDED(factory->CreateStream(&stream))&&SUCCEEDED(stream->InitializeFromFilename(temporary.c_str(),GENERIC_WRITE))&&SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder))&&SUCCEEDED(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache))&&SUCCEEDED(encoder->CreateNewFrame(&frame,&properties))&&SUCCEEDED(frame->Initialize(properties.Get()))&&SUCCEEDED(frame->SetSize(width,height))&&SUCCEEDED(factory->CreateBitmapFromMemory(width,height,GUID_WICPixelFormat32bppRGBA,UINT(rowPitch),UINT(pixels.size()),const_cast<BYTE*>(pixels.data()),&bitmap))){WICPixelFormatGUID format=GUID_WICPixelFormat32bppRGBA;if(SUCCEEDED(frame->SetPixelFormat(&format))&&SUCCEEDED(frame->WriteSource(bitmap.Get(),nullptr))&&SUCCEEDED(frame->Commit())&&SUCCEEDED(encoder->Commit()))success=true;}
    bitmap.Reset();frame.Reset();properties.Reset();encoder.Reset();stream.Reset();factory.Reset();if(!success){std::filesystem::remove(temporary,error);return false;}if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){std::filesystem::remove(temporary,error);return false;}return true;
}
}
