#include <Aevrynd/FrameCapture3D.h>
#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>

using Microsoft::WRL::ComPtr;
using namespace Aevrynd;

int main(){
    const std::filesystem::path root=std::filesystem::temp_directory_path()/"Aevrynd"/"FrameCapture3DTest",output=root/"capture.png",invalid=root/"invalid.png";std::error_code error;std::filesystem::remove_all(root,error);const std::array<std::uint8_t,32> pixels={0,0,255,255,0,255,0,255,99,99,99,99,99,99,99,99,255,0,0,255,255,255,255,255,88,88,88,88,88,88,88,88};
    const FrameCaptureAnalysis3D analysis=AnalyzeFrameCaptureRgba8(2,2,16,pixels);if(!analysis.valid||analysis.sampledPixels!=4||analysis.averageLuminance<.49||analysis.averageLuminance>.51||analysis.darkPixelFraction!=0||analysis.brightPixelFraction<.24||analysis.brightPixelFraction>.26||analysis.linearAverageLuminance<.49||analysis.linearAverageLuminance>.51||analysis.linearLuminanceP05<.06||analysis.linearLuminanceP05>.08||analysis.linearLuminanceP50<.20||analysis.linearLuminanceP50>.22||analysis.linearLuminanceP95!=1||analysis.linearLuminanceP99!=1){std::cerr<<"Frame exposure analysis failed\n";return 5;}if(AnalyzeFrameCaptureRgba8(2,2,7,pixels).valid){std::cerr<<"Invalid frame analysis input was accepted\n";return 6;}
    if(!EncodeFrameCapturePng3D(output,2,2,16,pixels)||!std::filesystem::is_regular_file(output)){std::cerr<<"PNG frame capture failed\n";return 1;}std::ifstream file(output,std::ios::binary);std::array<unsigned char,8> signature{};file.read(reinterpret_cast<char*>(signature.data()),signature.size());const std::array<unsigned char,8> expected={137,80,78,71,13,10,26,10};if(signature!=expected){std::cerr<<"PNG signature failed\n";return 2;}
    const HRESULT apartment=CoInitializeEx(nullptr,COINIT_MULTITHREADED);ComPtr<IWICImagingFactory> factory;ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> converter;UINT width{},height{};std::array<std::uint8_t,16> decodedPixels{};const bool decoded=SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))&&SUCCEEDED(factory->CreateDecoderFromFilename(output.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder))&&SUCCEEDED(decoder->GetFrame(0,&frame))&&SUCCEEDED(frame->GetSize(&width,&height))&&SUCCEEDED(factory->CreateFormatConverter(&converter))&&SUCCEEDED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom))&&SUCCEEDED(converter->CopyPixels(nullptr,8,UINT(decodedPixels.size()),decodedPixels.data()));converter.Reset();frame.Reset();decoder.Reset();factory.Reset();if(apartment==S_OK||apartment==S_FALSE)CoUninitialize();if(!decoded||width!=2||height!=2||decodedPixels[0]!=0||decodedPixels[1]!=0||decodedPixels[2]!=255||decodedPixels[4]!=0||decodedPixels[5]!=255||decodedPixels[6]!=0){std::cerr<<"PNG decode validation failed\n";return 3;}
    if(EncodeFrameCapturePng3D(invalid,2,2,7,pixels)||std::filesystem::exists(invalid)){std::cerr<<"Invalid row pitch was accepted\n";return 4;}std::cout<<"captureBytes="<<std::filesystem::file_size(output)<<" width="<<width<<" height="<<height<<'\n';std::filesystem::remove_all(root,error);return 0;
}
