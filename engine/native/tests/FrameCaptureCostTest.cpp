#include <Aevrynd/FrameCapture3D.h>
#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;
using namespace Aevrynd;
namespace {
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Apartment {
    HRESULT result{CoInitializeEx(nullptr,COINIT_MULTITHREADED)};
    ~Apartment(){if(result==S_OK||result==S_FALSE)CoUninitialize();}
};
struct OwnedDirectory {
    std::filesystem::path path;
    OwnedDirectory(){
        GUID id{};Require(SUCCEEDED(CoCreateGuid(&id)),"Unique test identity");
        std::array<wchar_t,40> text{};Require(StringFromGUID2(id,text.data(),int(text.size()))>0,"GUID conversion");
        path=std::filesystem::temp_directory_path()/(std::wstring(L"AevryndFrameCaptureCost-")+text.data());
        Require(std::filesystem::create_directory(path),"Exclusive temporary directory");
    }
    ~OwnedDirectory(){
        // 只回收本次唯一目錄內明列的測試檔，不遞迴刪除其他目錄。
        std::error_code error;
        for(const char* name:{"timed.png","null.png","default.png","legacy.png","invalid.png","parent-file","blocked"}){
            std::filesystem::remove(path/name,error);
            const auto temporary=std::filesystem::path((path/name).wstring()+L"."+std::to_wstring(GetCurrentProcessId())+L".tmp");
            std::filesystem::remove(temporary,error);
        }
        std::filesystem::remove(path,error);
    }
};
std::vector<std::uint8_t> Decode(const std::filesystem::path& path,unsigned expectedWidth,unsigned expectedHeight){
    ComPtr<IWICImagingFactory> factory;ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> converter;
    Require(SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))),"Decode factory");
    Require(SUCCEEDED(factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder)),"Decode PNG");
    Require(SUCCEEDED(decoder->GetFrame(0,&frame)),"Decode frame");
    UINT width{},height{};Require(SUCCEEDED(frame->GetSize(&width,&height))&&width==expectedWidth&&height==expectedHeight,"Decoded dimensions");
    Require(SUCCEEDED(factory->CreateFormatConverter(&converter)),"Decode converter");
    Require(SUCCEEDED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)),"Decode RGBA");
    std::vector<std::uint8_t> pixels(std::size_t(width)*height*4);
    Require(SUCCEEDED(converter->CopyPixels(nullptr,width*4,UINT(pixels.size()),pixels.data())),"Decoded pixels");
    return pixels;
}
void TimingAccounting(const FrameCaptureCost3D& cost){
    // 只驗證計帳一致性；零值合法，不設絕對時間或效能門檻。
    std::uint64_t sum{};
    for(const auto duration:{cost.setupNanoseconds,cost.encodeWriteNanoseconds,cost.publishNanoseconds,cost.verifyHashNanoseconds}){
        Require(duration<=cost.totalNanoseconds,"Individual stage exceeds total");
        Require(duration<=(std::numeric_limits<std::uint64_t>::max)()-sum,"Stage sum overflow");
        sum+=duration;
    }
    Require(sum<=cost.totalNanoseconds,"Non-overlapping CPU stages exceed total");
}
void PngSignature(const std::filesystem::path& path){
    std::ifstream file(path,std::ios::binary);std::array<unsigned char,8> actual{};
    file.read(reinterpret_cast<char*>(actual.data()),std::streamsize(actual.size()));
    const std::array<unsigned char,8> expected{137,80,78,71,13,10,26,10};
    Require(bool(file)&&actual==expected,"PNG signature");
}
}

int main(){
    try{
        Apartment apartment;Require(SUCCEEDED(apartment.result),"COM apartment");
        OwnedDirectory directory;
        constexpr unsigned width=128,height=64;
        constexpr std::size_t rowPitch=std::size_t(width)*4+16;
        std::vector<std::uint8_t> pixels(rowPitch*height,0xCD),expected(std::size_t(width)*height*4);
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){
            const std::array<std::uint8_t,4> value{std::uint8_t(x*2),std::uint8_t(y*4),std::uint8_t((x+y)*3),255};
            for(std::size_t channel=0;channel<4;++channel){
                pixels[std::size_t(y)*rowPitch+std::size_t(x)*4+channel]=value[channel];
                expected[(std::size_t(y)*width+x)*4+channel]=value[channel];
            }
        }
        const auto timedPath=directory.path/"timed.png",nullPath=directory.path/"null.png";
        PublishedFrameCapture3D timedPublication{},nullPublication{},defaultPublication{};
        FrameCaptureCost3D cost{};
        Require(EncodeFrameCapturePng3D(timedPath,width,height,rowPitch,pixels,timedPublication,&cost),"Instrumented encode");
        Require(cost.complete&&timedPublication.Complete(),"Instrumented publication completion");
        TimingAccounting(cost);const FrameCaptureCost3D successCost=cost;
        Require(ValidatePublishedFrameCapture3D(timedPath,timedPublication),"Instrumented SHA/identity validation");
        Require(EncodeFrameCapturePng3D(nullPath,width,height,rowPitch,pixels,nullPublication,nullptr),"Explicit nullptr encode");
        Require(nullPublication.Complete()&&ValidatePublishedFrameCapture3D(nullPath,nullPublication),"Nullptr publication validation");
        Require(EncodeFrameCapturePng3D(directory.path/"default.png",width,height,rowPitch,pixels,defaultPublication),"Compatible default argument");
        Require(defaultPublication.Complete()&&ValidatePublishedFrameCapture3D(directory.path/"default.png",defaultPublication),"Default publication validation");
        Require(EncodeFrameCapturePng3D(directory.path/"legacy.png",width,height,rowPitch,pixels),"Compatible five-argument delegation");
        for(const char* name:{"timed.png","null.png","default.png","legacy.png"}){
            PngSignature(directory.path/name);
            Require(Decode(directory.path/name,width,height)==expected,"All overloads preserve exact decoded RGBA and row pitch");
        }

        unsigned negatives{};
        const auto Reject=[&](const std::filesystem::path& path,unsigned w,unsigned h,std::size_t pitch,std::span<const std::uint8_t> data){
            // 先污染輸出，證明失敗會清除上一張影像的成功狀態。
            cost={11,22,33,44,110,true};PublishedFrameCapture3D publication=timedPublication;
            Require(!EncodeFrameCapturePng3D(path,w,h,pitch,data,publication,&cost),"Invalid capture accepted");
            Require(!cost.complete&&!publication.Complete(),"Failure retained successful result");
            TimingAccounting(cost);++negatives;
        };
        const auto invalid=directory.path/"invalid.png";
        Reject(invalid,0,height,rowPitch,pixels);
        Reject(invalid,width,0,rowPitch,pixels);
        Reject(invalid,MaximumFrameCaptureDimension3D+1,height,rowPitch,pixels);
        Reject(invalid,width,MaximumFrameCaptureDimension3D+1,rowPitch,pixels);
        Reject(invalid,width,height,std::size_t(width)*4-1,pixels);
        Reject(invalid,width,height,rowPitch,std::span<const std::uint8_t>(pixels).first(16));
        Reject({},width,height,rowPitch,pixels);
        if constexpr(sizeof(std::size_t)>sizeof(UINT)){
            Reject(invalid,width,height,std::size_t((std::numeric_limits<UINT>::max)())+1,pixels);
        }
        Require(!std::filesystem::exists(invalid),"Rejected input created PNG");
        Reject(timedPath,width,height,std::size_t(width)*4-1,pixels);
        Require(ValidatePublishedFrameCapture3D(timedPath,timedPublication),"Rejected input changed previous valid publication");

        {std::ofstream file(directory.path/"parent-file");file<<"not a directory";Require(bool(file),"Create path-failure fixture");}
        Reject(directory.path/"parent-file"/"child.png",width,height,rowPitch,pixels);
        Require(std::filesystem::create_directory(directory.path/"blocked"),"Create publication-failure fixture");
        Reject(directory.path/"blocked",width,height,rowPitch,pixels);
        Require(std::filesystem::is_directory(directory.path/"blocked"),"Failed publication replaced directory");
        Require(cost.verifyHashNanoseconds==0,"Failed publication ran final-file hash stage");

        // 真實檔案竄改仍須被原正式雜湊驗證拒絕，計時成功不能取代驗證。
        {std::ofstream file(timedPath,std::ios::binary|std::ios::app);file.put('X');Require(bool(file),"Tamper fixture write");}
        Require(!ValidatePublishedFrameCapture3D(timedPath,timedPublication),"Tampered publication accepted");++negatives;
        Require(EncodeFrameCapturePng3D(timedPath,width,height,rowPitch,pixels,timedPublication,&cost)&&cost.complete,"Recovery after failed cost reuse");
        Require(ValidatePublishedFrameCapture3D(timedPath,timedPublication)&&Decode(timedPath,width,height)==expected,"Recovery publication content");
        TimingAccounting(cost);
        std::cout<<"FRAME_CAPTURE_COST_PASS width="<<width<<" height="<<height<<" negatives="<<negatives
                 <<" setup_ns="<<successCost.setupNanoseconds<<" encode_write_ns="<<successCost.encodeWriteNanoseconds
                 <<" publish_ns="<<successCost.publishNanoseconds<<" verify_hash_ns="<<successCost.verifyHashNanoseconds
                 <<" total_ns="<<successCost.totalNanoseconds<<" performance_threshold=false\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<"FRAME_CAPTURE_COST_FAIL "<<error.what()<<'\n';return 1;}
}
