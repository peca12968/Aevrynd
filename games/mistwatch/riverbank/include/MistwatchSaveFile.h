#pragma once
#include "MistwatchSaveGame.h"
#include <Windows.h>
#include <atomic>
#include <filesystem>
#include <fstream>

namespace Mistwatch {

// 河岸版優先保留已使用的本機備用存檔。原磁碟恢復後也不會自動切換或搬移進度。
inline std::filesystem::path SelectRiverbankSaveDirectory(const std::filesystem::path& local) {
    namespace fs=std::filesystem;
    std::error_code error;
    if(!local.is_absolute()||!fs::is_directory(local,error))return {};
    const auto readable=[](const fs::path& directory) {
        std::error_code readError;
        // 直接開啟目錄才能辨別掛載已失效的 junction。
        static_cast<void>(fs::directory_iterator(directory,readError));
        return !readError;
    };
    const auto alternate=local/"MistwatchRiverbankPlaytest"/"Aevrynd"/"Mistwatch";
    if(readable(alternate))return alternate;
    const auto primary=local/"Aevrynd"/"Mistwatch";
    fs::create_directories(primary,error);
    if(!error&&readable(primary))return primary;
    fs::create_directories(alternate,error);
    if(!error&&readable(alternate))return alternate;
    return {};
}

enum class SaveFileResult { Saved, InvalidPath, InvalidSnapshot, DirectoryUnavailable,
    TemporaryUnavailable, WriteFailed, FlushFailed, CloseFailed, ReplaceFailed, Exception };

// 先驗證完整快照，再於相同目錄建立獨占暫存檔；任何失敗均不以空資料取代舊存檔。
inline SaveFileResult WriteSaveFile(const std::filesystem::path& target,const SaveGame& game) noexcept {
    HANDLE handle=INVALID_HANDLE_VALUE;
    std::filesystem::path temporary;
    bool ownsTemporary=false;
    const auto cleanup=[&]() noexcept {
        if(handle!=INVALID_HANDLE_VALUE){CloseHandle(handle);handle=INVALID_HANDLE_VALUE;}
        if(ownsTemporary)DeleteFileW(temporary.c_str());
    };
    try {
        if(target.empty()||!target.has_filename()||target.filename()==L"."||target.filename()==L"..")
            return SaveFileResult::InvalidPath;
        const auto bytes=game.Serialize();
        if(bytes.empty()||bytes.size()>SaveGame::MaximumBytes)return SaveFileResult::InvalidSnapshot;
        std::error_code error;
        if(!target.parent_path().empty())std::filesystem::create_directories(target.parent_path(),error);
        if(error)return SaveFileResult::DirectoryUnavailable;
        static std::atomic<unsigned long long> sequence{};
        temporary=target;
        temporary+=L"."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(++sequence)+L".tmp";
        handle=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(handle==INVALID_HANDLE_VALUE)return SaveFileResult::TemporaryUnavailable;
        ownsTemporary=true;
        DWORD written{};
        if(!WriteFile(handle,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr)||written!=bytes.size()){
            cleanup();return SaveFileResult::WriteFailed;
        }
        if(!FlushFileBuffers(handle)){cleanup();return SaveFileResult::FlushFailed;}
        const BOOL closed=CloseHandle(handle);handle=INVALID_HANDLE_VALUE;
        if(!closed){cleanup();return SaveFileResult::CloseFailed;}
        if(!MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
            cleanup();return SaveFileResult::ReplaceFailed;
        }
        ownsTemporary=false;
        return SaveFileResult::Saved;
    }catch(...){cleanup();return SaveFileResult::Exception;}
}

// 讀檔只寫入解析成功的候選；世界／控制器／鏡頭預檢仍由整合層負責。
inline bool ReadSaveFile(const std::filesystem::path& target,SaveGame& candidate) noexcept {
    try {
        if(target.empty())return false;
        std::ifstream input(target,std::ios::binary);
        if(!input)return false;
        return candidate.RestoreStream(input);
    }catch(...){return false;}
}
}
