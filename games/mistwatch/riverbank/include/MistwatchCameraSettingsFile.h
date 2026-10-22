#pragma once
#include "MistwatchCameraSettings.h"
#include <Windows.h>
#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>

namespace Mistwatch {
inline bool ReadCameraSettingsFile(const std::filesystem::path& file,CameraSettings& settings) {
    std::ifstream in(file,std::ios::binary);std::array<char,129> text{};
    in.read(text.data(),std::streamsize(text.size()));
    const auto bytes=in.gcount();
    if(in.bad()||bytes<=0||bytes>128)return false;
    return settings.Restore(std::string_view(text.data(),std::size_t(bytes)));
}
inline bool WriteCameraSettingsFile(const std::filesystem::path& file,const CameraSettings& settings) {
    const auto text=settings.Serialize();
    if(file.empty()||text.empty())return false;
    std::error_code error;
    if(!file.parent_path().empty())std::filesystem::create_directories(file.parent_path(),error);
    if(error)return false;
    // 各程序／各次寫入使用獨立候選；不可截斷另一個仍在寫入的 .tmp。
    static std::atomic<unsigned long long> sequence{};
    auto temporary=file;
    temporary+=L".tmp-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(sequence.fetch_add(1,std::memory_order_relaxed));
    const HANDLE handle=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(handle==INVALID_HANDLE_VALUE)return false;
    DWORD written{};
    const bool content=WriteFile(handle,text.data(),DWORD(text.size()),&written,nullptr)&&written==text.size();
    const bool flushed=content&&FlushFileBuffers(handle);
    const bool closed=CloseHandle(handle)!=0;
    if(flushed&&closed&&MoveFileExW(temporary.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return true;
    // 只清除本次 CREATE_NEW 成功取得的候選；既有設定始終保留到原子替換成功。
    DeleteFileW(temporary.c_str());return false;
}
}
