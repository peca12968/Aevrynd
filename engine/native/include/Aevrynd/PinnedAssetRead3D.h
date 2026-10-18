#pragma once
#include <Windows.h>
#include <cstddef>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <vector>

namespace Aevrynd {
// 在來源雜湊、CPU解析與GPU上傳期間持有同一唯讀鎖；不得改寫或刪除來源。
class PinnedAssetRead3D final {
    HANDLE handle_{INVALID_HANDLE_VALUE};
    std::vector<std::byte> bytes_;
public:
    PinnedAssetRead3D(const std::filesystem::path& path,std::size_t expectedBytes){
        if(!path.is_absolute()||path.native().find(L'\0')!=std::wstring::npos||!expectedBytes||expectedBytes>16u*1024u*1024u)
            throw std::invalid_argument("PINNED_ASSET_INPUT");
        handle_=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        try{
            LARGE_INTEGER length{};
            if(handle_==INVALID_HANDLE_VALUE||!GetFileSizeEx(handle_,&length)||length.QuadPart!=static_cast<LONGLONG>(expectedBytes))
                throw std::runtime_error("PINNED_ASSET_LENGTH");
            bytes_.resize(expectedBytes);DWORD read{};
            if(!ReadFile(handle_,bytes_.data(),static_cast<DWORD>(bytes_.size()),&read,nullptr)||read!=bytes_.size())
                throw std::runtime_error("PINNED_ASSET_READ");
        }catch(...){if(handle_!=INVALID_HANDLE_VALUE)CloseHandle(handle_);handle_=INVALID_HANDLE_VALUE;throw;}
    }
    ~PinnedAssetRead3D(){if(handle_!=INVALID_HANDLE_VALUE)CloseHandle(handle_);}
    PinnedAssetRead3D(const PinnedAssetRead3D&)=delete;
    PinnedAssetRead3D& operator=(const PinnedAssetRead3D&)=delete;
    [[nodiscard]] std::span<const std::byte> Bytes()const noexcept{return bytes_;}
};
}
