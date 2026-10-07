#include <Aevrynd/NativeMaterialDds3D.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <chrono>
#include <limits>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace Aevrynd;
namespace {
void Need(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
// 持有普通父目錄且不共享寫入或刪除，測試寫入期間拒絕 junction／reparse 置換；只關閉 handle，不刪證據。
class OrdinaryDirectoryLocks {
    std::vector<HANDLE> handles;
public:
    OrdinaryDirectoryLocks()=default;
    OrdinaryDirectoryLocks(const OrdinaryDirectoryLocks&)=delete;
    OrdinaryDirectoryLocks& operator=(const OrdinaryDirectoryLocks&)=delete;
    ~OrdinaryDirectoryLocks(){for(const auto handle:handles)CloseHandle(handle);}
    void Lock(const std::filesystem::path& path){
        const HANDLE handle=CreateFileW(path.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ,nullptr,
            OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
        Need(handle!=INVALID_HANDLE_VALUE,"ordinary directory open failed");
        BY_HANDLE_FILE_INFORMATION info{};
        const bool valid=GetFileInformationByHandle(handle,&info)&&
            (info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&!(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT);
        if(!valid){CloseHandle(handle);throw std::runtime_error("nonordinary/reparse directory rejected");}
        handles.push_back(handle);
    }
    void Ancestors(const std::filesystem::path& absolute){
        Need(absolute.is_absolute(),"absolute ordinary directory required");
        auto current=absolute.root_path();Lock(current);
        for(const auto& part:absolute.relative_path()){current/=part;Lock(current);}
    }
};
std::filesystem::path FreshRoot(int argc,char** argv,OrdinaryDirectoryLocks& locks){
    Need(argc==1||argc==2,"zero or one fresh CPU evidence output argument");
    std::filesystem::path root;
    if(argc==1){
        const auto working=std::filesystem::absolute(std::filesystem::current_path()).lexically_normal();
        locks.Ancestors(working);const auto evidence=working/"evidence";
        if(!CreateDirectoryW(evidence.c_str(),nullptr))Need(GetLastError()==ERROR_ALREADY_EXISTS,"evidence directory creation failed");
        locks.Lock(evidence);
        const auto tick=std::chrono::high_resolution_clock::now().time_since_epoch().count();
        root=evidence/("native-rgba8-dds-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(tick));
    }else{
        Need(argv[1]&&*argv[1],"empty fresh CPU output argument");
        root=std::filesystem::absolute(std::filesystem::path{argv[1]}).lexically_normal();
        locks.Ancestors(root.parent_path());
    }
    // 最終目錄必須本次原子新建；既有同名項立即失敗，不重試、不覆寫、不清理。
    Need(CreateDirectoryW(root.c_str(),nullptr)!=0,"fresh output creation failed or already exists");
    locks.Lock(root);std::cout<<"RGBA8_DDS_EVIDENCE "<<root.string()<<'\n';return root;
}
void Put(std::vector<std::uint8_t>& bytes,std::size_t at,std::uint32_t value){Need(at+4<=bytes.size(),"header offset");std::memcpy(bytes.data()+at,&value,4);}
std::uint32_t Mips(std::uint32_t width,std::uint32_t height){
    std::uint32_t count=1;while(width>1||height>1){width=(std::max)(1u,width/2);height=(std::max)(1u,height/2);++count;}return count;
}
std::vector<std::uint8_t> Make(std::uint32_t width,std::uint32_t height){
    std::vector<std::uint8_t> bytes(148,0);
    Put(bytes,0,0x20534444u);Put(bytes,4,124);Put(bytes,8,0x2100fu);Put(bytes,12,height);Put(bytes,16,width);Put(bytes,20,width*4u);Put(bytes,28,Mips(width,height));
    Put(bytes,76,32);Put(bytes,80,4);Put(bytes,84,0x30315844u);Put(bytes,108,0x401008u);Put(bytes,128,29);Put(bytes,132,3);Put(bytes,140,1);
    const auto count=Mips(width,height);
    for(std::uint32_t mip=0;mip<count;){
        for(std::uint32_t y=0;y<height;++y)for(std::uint32_t x=0;x<width;++x){
            bytes.push_back(static_cast<std::uint8_t>((x*17u+y*23u+mip*7u)%256u));
            bytes.push_back(static_cast<std::uint8_t>((x*13u+y*5u+53u)%256u));
            bytes.push_back(static_cast<std::uint8_t>((x*3u+y*29u+89u)%256u));
            const std::uint8_t alpha[]{0,255,127,128};bytes.push_back(alpha[(x+y)%4u]);
        }
        if(width==1&&height==1)break;width=(std::max)(1u,width/2);height=(std::max)(1u,height/2);++mip;
    }
    return bytes;
}
void Write(const std::filesystem::path& path,const std::vector<std::uint8_t>& bytes){
    // 原子 CREATE_NEW：即使同名項被插入也不得截斷或跟隨既有檔案；失敗時保留原證據。
    Need(bytes.size()<=(std::numeric_limits<DWORD>::max)(),"bounded test DDS write");
    const HANDLE handle=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
    Need(handle!=INVALID_HANDLE_VALUE,"unique test file creation failed");
    struct CloseFile{HANDLE handle;~CloseFile(){CloseHandle(handle);}} close{handle};
    DWORD written{};
    Need(WriteFile(handle,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr)&&
        written==bytes.size()&&FlushFileBuffers(handle),"test DDS write");
}
void Positive(const std::filesystem::path& root,std::uint32_t width,std::uint32_t height){
    const auto bytes=Make(width,height);const auto path=root/(std::to_string(width)+"x"+std::to_string(height)+".dds");Write(path,bytes);
    NativeMaterialDds3D actual{};NativeMaterialDdsError3D error{};
    Need(LoadNativeMaterialDds3D(path,NativeMaterialDdsRole3D::BaseColor,2048,true,actual,&error),"actual RGBA8sRGB detailed load");
    Need(error==NativeMaterialDdsError3D::None&&actual.format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB&&actual.width==width&&actual.height==height&&actual.mipCount==Mips(width,height)&&actual.mips.size()==actual.mipCount&&actual.sha256.size()==64,"actual format/fullmip/identity");
    std::size_t offset=148;auto w=width,h=height;
    for(const auto& mip:actual.mips){
        Need(mip.width==w&&mip.height==h&&mip.rowBytes==std::size_t(w)*4&&mip.pixels.size()==std::size_t(w)*h*4,"RGBA rowpitch/nonblock mip size");
        Need(std::equal(mip.pixels.begin(),mip.pixels.end(),bytes.begin()+static_cast<std::ptrdiff_t>(offset)),"every actual RGBA byte exact, including asymmetric alpha0/255/127/128");
        offset+=mip.pixels.size();w=(std::max)(1u,w/2);h=(std::max)(1u,h/2);
    }
    Need(offset==bytes.size()&&actual.payloadBytes==bytes.size()-148,"whole payload bytes");
    NativeMaterialDds3D probe{};
    Need(LoadNativeMaterialDds3D(path,NativeMaterialDdsRole3D::BaseColor,2048,false,probe,&error,actual.sha256,bytes.size(),true)&&probe.mips.empty()&&probe.payloadBytes==actual.payloadBytes,"true integrity-only native probe");
    Need(!LoadNativeMaterialDds3D(path,NativeMaterialDdsRole3D::BaseColor,2048,false,probe,&error,std::string(64,'0'),bytes.size(),true)&&error==NativeMaterialDdsError3D::HashMismatch,"strict SHA unchanged");
    Need(!LoadNativeMaterialDds3D(path,NativeMaterialDdsRole3D::BaseColor,2048,false,probe,&error,actual.sha256,bytes.size()-1,true)&&error==NativeMaterialDdsError3D::HashMismatch,"strict bytes unchanged");
    std::cout<<"RGBA8_SRGB "<<width<<'x'<<height<<" mips="<<actual.mipCount<<" bytes="<<actual.payloadBytes<<" exact=1\n";
}
void Reject(const std::filesystem::path& root,const char* name,std::vector<std::uint8_t> bytes,NativeMaterialDdsRole3D role,NativeMaterialDdsError3D expected){
    const auto path=root/name;Write(path,bytes);NativeMaterialDds3D image{};NativeMaterialDdsError3D error{};
    Need(!LoadNativeMaterialDds3D(path,role,2048,true,image,&error)&&error==expected&&image.format==DXGI_FORMAT_UNKNOWN&&image.mips.empty(),"actual hostile DDS rejection");
}
}
int main(int argc,char** argv){
    try{
        OrdinaryDirectoryLocks locks;const auto root=FreshRoot(argc,argv,locks);
        // 非4倍數／非正方形與512眉睫尺寸，實際核每個mip的RGBA像素與列跨度。
        Positive(root,3,5);Positive(root,4,4);Positive(root,512,512);
        const auto good=Make(3,5);auto bad=good;Put(bad,128,28);Reject(root,"linear28.dds",bad,NativeMaterialDdsRole3D::BaseColor,NativeMaterialDdsError3D::UnsupportedFormat);
        Reject(root,"wrong-surface.dds",good,NativeMaterialDdsRole3D::Surface,NativeMaterialDdsError3D::UnsupportedFormat);
        Reject(root,"wrong-normal.dds",good,NativeMaterialDdsRole3D::Normal,NativeMaterialDdsError3D::UnsupportedFormat);
        bad=good;bad.pop_back();Reject(root,"last-mip-short.dds",bad,NativeMaterialDdsRole3D::BaseColor,NativeMaterialDdsError3D::InvalidPayload);
        bad=good;bad.resize(148+60);Reject(root,"tail-mips-missing.dds",bad,NativeMaterialDdsRole3D::BaseColor,NativeMaterialDdsError3D::InvalidPayload);
        bad=good;bad.push_back(0);Reject(root,"trailing.dds",bad,NativeMaterialDdsRole3D::BaseColor,NativeMaterialDdsError3D::InvalidPayload);
        bad=good;Put(bad,28,2);Reject(root,"mip-count.dds",bad,NativeMaterialDdsRole3D::BaseColor,NativeMaterialDdsError3D::InvalidMipChain);
        bad=good;Put(bad,20,16);Reject(root,"block-pitch.dds",bad,NativeMaterialDdsRole3D::BaseColor,NativeMaterialDdsError3D::InvalidHeader);
        bad=good;Put(bad,8,0x21007u);Reject(root,"missing-pitch-flag.dds",bad,NativeMaterialDdsRole3D::BaseColor,NativeMaterialDdsError3D::InvalidHeader);
        bad=good;Put(bad,140,2);Reject(root,"array.dds",bad,NativeMaterialDdsRole3D::BaseColor,NativeMaterialDdsError3D::UnsupportedResource);
        bad=good;Put(bad,16,0xffffffffu);Reject(root,"dimensions.dds",bad,NativeMaterialDdsRole3D::BaseColor,NativeMaterialDdsError3D::InvalidDimensions);
        std::cout<<"RGBA8_SRGB positives=3 negatives=11 strictIntegrity=6 originalNativeDDSsmokeStillRequired=1\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
