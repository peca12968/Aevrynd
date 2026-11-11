#include "Aevrynd/FrameCapture3D.h"
#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <bcrypt.h>
#include <ranges>
#include <chrono>

using Microsoft::WRL::ComPtr;
namespace Aevrynd {
namespace {
// 空指標路徑不讀取時鐘；離開函式（含失敗）時才完成總時間。
struct CaptureCostScope3D {
    using Clock=std::chrono::steady_clock;
    FrameCaptureCost3D* cost{};
    Clock::time_point start{},phaseStart{};
    std::uint64_t FrameCaptureCost3D::* phase{&FrameCaptureCost3D::setupNanoseconds};
    explicit CaptureCostScope3D(FrameCaptureCost3D* value):cost(value) {
        if(cost){*cost={};start=phaseStart=Clock::now();}
    }
    void Next(std::uint64_t FrameCaptureCost3D::* next) {
        if(!cost)return;
        const auto now=Clock::now();
        if(phase)cost->*phase+=std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(now-phaseStart).count());
        phaseStart=now;phase=next;
    }
    ~CaptureCostScope3D() {
        if(!cost)return;
        const auto now=Clock::now();
        if(phase)cost->*phase+=std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(now-phaseStart).count());
        cost->totalNanoseconds=std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(now-start).count());
    }
};
struct CaptureComApartment3D {HRESULT result{CoInitializeEx(nullptr,COINIT_MULTITHREADED)};~CaptureComApartment3D(){if(result==S_OK||result==S_FALSE)CoUninitialize();}};
double SrgbToLinear3D(double value){return value<=.04045?value/12.92:std::pow((value+.055)/1.055,2.4);}
double LuminancePercentile3D(const std::array<std::size_t,256>& histogram,std::size_t samples,double percentile){const std::size_t rank=(std::max)(std::size_t{1},std::size_t(std::ceil(percentile*samples)));std::size_t accumulated{};for(std::size_t bin=0;bin<histogram.size();++bin){accumulated+=histogram[bin];if(accumulated>=rank)return double(bin)/255.0;}return 1;}
struct CaptureFileIdentity3D {
    FILE_ID_INFO extended{};
    BY_HANDLE_FILE_INFORMATION legacy{};
    bool useLegacy{};
};
bool ReadCaptureFileIdentity3D(HANDLE file,CaptureFileIdentity3D& identity){
    identity={};
    if(GetFileInformationByHandleEx(file,FileIdInfo,&identity.extended,sizeof(identity.extended)))return true;
    const DWORD error=GetLastError();
    if(error!=ERROR_INVALID_PARAMETER&&error!=ERROR_NOT_SUPPORTED&&error!=ERROR_INVALID_FUNCTION)return false;
    // FAT 系列不提供 128 位檔案 ID；只在明確辨識的檔案系統使用其正式 64 位身分。
    // NTFS、ReFS 與未知檔案系統仍保留原本的 128 位驗證，不接受降級。
    std::array<wchar_t,32> filesystem{};
    if(!GetVolumeInformationByHandleW(file,nullptr,0,nullptr,nullptr,nullptr,filesystem.data(),DWORD(filesystem.size())))return false;
    if(_wcsicmp(filesystem.data(),L"exFAT")&&_wcsicmp(filesystem.data(),L"FAT32")&&_wcsicmp(filesystem.data(),L"FAT"))return false;
    identity.useLegacy=true;
    return GetFileInformationByHandle(file,&identity.legacy)!=FALSE;
}
bool SameFileIdentity3D(const CaptureFileIdentity3D& left,const CaptureFileIdentity3D& right){
    if(left.useLegacy!=right.useLegacy)return false;
    if(!left.useLegacy)return left.extended.VolumeSerialNumber==right.extended.VolumeSerialNumber&&
        std::ranges::equal(left.extended.FileId.Identifier,right.extended.FileId.Identifier);
    const auto& a=left.legacy;const auto& b=right.legacy;
    return a.dwVolumeSerialNumber==b.dwVolumeSerialNumber&&a.nFileIndexHigh==b.nFileIndexHigh&&a.nFileIndexLow==b.nFileIndexLow&&
        a.nFileSizeHigh==b.nFileSizeHigh&&a.nFileSizeLow==b.nFileSizeLow&&
        a.ftLastWriteTime.dwHighDateTime==b.ftLastWriteTime.dwHighDateTime&&a.ftLastWriteTime.dwLowDateTime==b.ftLastWriteTime.dwLowDateTime;
}
bool PinAndHashFinalFile3D(const std::filesystem::path& path,PublishedFrameCapture3D& publication){
    publication={};const HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN,nullptr);if(file==INVALID_HANDLE_VALUE)return false;
    CaptureFileIdentity3D identityBefore{},identityAfter{};FILE_STANDARD_INFO sizeBefore{},sizeAfter{};BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};bool valid=ReadCaptureFileIdentity3D(file,identityBefore)&&GetFileInformationByHandleEx(file,FileStandardInfo,&sizeBefore,sizeof(sizeBefore))&&sizeBefore.EndOfFile.QuadPart>0&&BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0&&BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0;
    std::array<unsigned char,64*1024> bytes{};std::uint64_t consumed{};while(valid){DWORD count{};if(!ReadFile(file,bytes.data(),DWORD(bytes.size()),&count,nullptr)){valid=false;break;}if(!count)break;consumed+=count;if(BCryptHashData(hash,bytes.data(),count,0)<0)valid=false;}
    valid=valid&&consumed==std::uint64_t(sizeBefore.EndOfFile.QuadPart)&&BCryptFinishHash(hash,reinterpret_cast<unsigned char*>(publication.finalFileSha256.data()),ULONG(publication.finalFileSha256.size()),0)>=0&&ReadCaptureFileIdentity3D(file,identityAfter)&&GetFileInformationByHandleEx(file,FileStandardInfo,&sizeAfter,sizeof(sizeAfter))&&SameFileIdentity3D(identityBefore,identityAfter)&&sizeBefore.EndOfFile.QuadPart==sizeAfter.EndOfFile.QuadPart;
    if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);CloseHandle(file);if(!valid){publication={};return false;}publication.finalFileBytes=consumed;return true;
}
}

FrameCaptureAnalysis3D AnalyzeFrameCaptureRgba8(unsigned width,unsigned height,std::size_t rowPitch,std::span<const std::uint8_t> pixels){
    FrameCaptureAnalysis3D result{};if(!width||!height||width>MaximumFrameCaptureDimension3D||height>MaximumFrameCaptureDimension3D||rowPitch<std::size_t(width)*4||std::size_t(height-1)>(std::numeric_limits<std::size_t>::max)()/rowPitch)return result;const std::size_t required=std::size_t(height-1)*rowPitch+std::size_t(width)*4;if(required>pixels.size())return result;
    const std::size_t pixelCount=std::size_t(width)*height,sampleStride=(std::max)(std::size_t{1},(pixelCount+MaximumFrameAnalysisSamples3D-1)/MaximumFrameAnalysisSamples3D);std::size_t dark{},bright{};double mean{},m2{},linearSum{};std::array<std::size_t,256> linearHistogram{};
    for(std::size_t index=0;index<pixelCount;index+=sampleStride){const std::size_t y=index/width,x=index-y*width,offset=y*rowPitch+x*4;const double red=pixels[offset]/255.0,green=pixels[offset+1]/255.0,blue=pixels[offset+2]/255.0,luminance=.2126*red+.7152*green+.0722*blue,linearLuminance=.2126*SrgbToLinear3D(red)+.7152*SrgbToLinear3D(green)+.0722*SrgbToLinear3D(blue);dark+=luminance<.02;bright+=luminance>.98;linearSum+=linearLuminance;++linearHistogram[std::size_t(std::clamp(std::lround(linearLuminance*255),0L,255L))];++result.sampledPixels;const double delta=luminance-mean;mean+=delta/double(result.sampledPixels);m2+=delta*(luminance-mean);}
    if(!result.sampledPixels)return result;result.valid=true;result.averageLuminance=mean;result.luminanceVariance=result.sampledPixels>1?m2/double(result.sampledPixels-1):0;result.darkPixelFraction=double(dark)/result.sampledPixels;result.brightPixelFraction=double(bright)/result.sampledPixels;result.linearAverageLuminance=linearSum/result.sampledPixels;result.linearLuminanceP05=LuminancePercentile3D(linearHistogram,result.sampledPixels,.05);result.linearLuminanceP50=LuminancePercentile3D(linearHistogram,result.sampledPixels,.50);result.linearLuminanceP95=LuminancePercentile3D(linearHistogram,result.sampledPixels,.95);result.linearLuminanceP99=LuminancePercentile3D(linearHistogram,result.sampledPixels,.99);return result;
}

bool EncodeFrameCapturePng3D(const std::filesystem::path& path,unsigned width,unsigned height,std::size_t rowPitch,std::span<const std::uint8_t> pixels,PublishedFrameCapture3D& publication,FrameCaptureCost3D* cost){
    CaptureCostScope3D timing(cost);
    publication={};
    if(path.empty()||!width||!height||width>MaximumFrameCaptureDimension3D||height>MaximumFrameCaptureDimension3D||rowPitch<std::size_t(width)*4||rowPitch>(std::numeric_limits<UINT>::max)())return false;
    const std::size_t lastRowBytes=std::size_t(width)*4;if(std::size_t(height-1)>(std::numeric_limits<std::size_t>::max)()/rowPitch)return false;const std::size_t required=std::size_t(height-1)*rowPitch+lastRowBytes;if(required>pixels.size()||pixels.size()>(std::numeric_limits<UINT>::max)())return false;
    std::error_code error;const std::filesystem::path parent=path.parent_path();if(!parent.empty()){std::filesystem::create_directories(parent,error);if(error)return false;}std::filesystem::path temporary=path;temporary+=L"."+std::to_wstring(GetCurrentProcessId())+L".tmp";std::filesystem::remove(temporary,error);error.clear();
    CaptureComApartment3D apartment;ComPtr<IWICImagingFactory> factory;ComPtr<IWICStream> stream;ComPtr<IWICBitmapEncoder> encoder;ComPtr<IWICBitmapFrameEncode> frame;ComPtr<IPropertyBag2> properties;ComPtr<IWICBitmap> bitmap;bool success=false;
    if(SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))&&SUCCEEDED(factory->CreateStream(&stream))&&SUCCEEDED(stream->InitializeFromFilename(temporary.c_str(),GENERIC_WRITE))&&SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder))&&SUCCEEDED(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache))&&SUCCEEDED(encoder->CreateNewFrame(&frame,&properties))&&SUCCEEDED(frame->Initialize(properties.Get()))&&SUCCEEDED(frame->SetSize(width,height))&&SUCCEEDED(factory->CreateBitmapFromMemory(width,height,GUID_WICPixelFormat32bppRGBA,UINT(rowPitch),UINT(pixels.size()),const_cast<BYTE*>(pixels.data()),&bitmap))){WICPixelFormatGUID format=GUID_WICPixelFormat32bppRGBA;if(SUCCEEDED(frame->SetPixelFormat(&format))){
        // COM/WIC 建立與像素格式設定屬 setup；WriteSource/Commit 與 stream 關閉屬編碼及寫入。
        timing.Next(&FrameCaptureCost3D::encodeWriteNanoseconds);
        if(SUCCEEDED(frame->WriteSource(bitmap.Get(),nullptr))&&SUCCEEDED(frame->Commit())&&SUCCEEDED(encoder->Commit()))success=true;
    }}
    bitmap.Reset();frame.Reset();properties.Reset();encoder.Reset();stream.Reset();factory.Reset();if(!success){std::filesystem::remove(temporary,error);return false;}timing.Next(&FrameCaptureCost3D::publishNanoseconds);if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){std::filesystem::remove(temporary,error);return false;}timing.Next(&FrameCaptureCost3D::verifyHashNanoseconds);const bool verified=PinAndHashFinalFile3D(path,publication);timing.Next(nullptr);if(cost)cost->complete=verified;return verified;
}
bool EncodeFrameCapturePng3D(const std::filesystem::path& path,unsigned width,unsigned height,std::size_t rowPitch,std::span<const std::uint8_t> pixels){PublishedFrameCapture3D publication{};return EncodeFrameCapturePng3D(path,width,height,rowPitch,pixels,publication);}
bool ValidatePublishedFrameCapture3D(const std::filesystem::path& path,const PublishedFrameCapture3D& publication){if(!publication.Complete())return false;PublishedFrameCapture3D actual{};return PinAndHashFinalFile3D(path,actual)&&actual.finalFileBytes==publication.finalFileBytes&&actual.finalFileSha256==publication.finalFileSha256;}
}
