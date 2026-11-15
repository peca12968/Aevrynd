#pragma once
#include "MistwatchSaveGame.h"
#include "MistwatchSettingsFile.h"
#include <Windows.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <optional>

namespace Mistwatch {

// 備用目錄已有存檔或設定時才保留優先權；只建立過啟動目錄不會遮住恢復後的原存檔。
inline std::filesystem::path SelectRiverbankSaveDirectory(const std::filesystem::path& local) {
    namespace fs=std::filesystem;
    std::error_code error;
    if(!local.is_absolute()||!fs::is_directory(ProfileIoPath(local),error))return {};
    const auto readable=[](const fs::path& directory) {
        std::error_code readError;
        // 直接開啟目錄才能辨別掛載已失效的 junction。
        static_cast<void>(fs::directory_iterator(ProfileIoPath(directory),readError));
        return !readError;
    };
    const auto alternate=local/"MistwatchRiverbankPlaytest"/"Aevrynd"/"Mistwatch";
    for(fs::directory_iterator entry(ProfileIoPath(alternate),error),end;!error&&entry!=end;entry.increment(error)){
        const auto name=entry->path().filename().native();
        const bool checkpoint=name==L"save.txt"||(name.starts_with(L"save-")&&name.ends_with(L".txt"));
        if(checkpoint||name==L"audio-settings.txt"||name==L"camera-settings.txt"||name==L"graphics-settings.txt"){
            std::error_code typeError;
            if(entry->is_regular_file(typeError))return alternate;
        }
    }
    const auto primary=local/"Aevrynd"/"Mistwatch";
    fs::create_directories(ProfileIoPath(primary),error);
    if(!error&&readable(primary))return primary;
    fs::create_directories(ProfileIoPath(alternate),error);
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
        const auto ioTarget=ProfileIoPath(target);
        if(!ioTarget.parent_path().empty())std::filesystem::create_directories(ioTarget.parent_path(),error);
        if(error)return SaveFileResult::DirectoryUnavailable;
        static std::atomic<unsigned long long> sequence{};
        // 舊程序可能在暫存檔完成前結束，而 Windows 會重用程序識別碼。
        const auto prefix=L"."+std::to_wstring(GetCurrentProcessId())+L".";
        constexpr int maxTemporaryAttempts=8;
        for(int attempt=0;attempt<maxTemporaryAttempts;++attempt){
            const auto suffix=prefix+std::to_wstring(++sequence)+L".tmp";
            temporary=target;
            temporary+=suffix;
            if(temporary.native().size()>=MAX_PATH){
                // 目標檔名合法但追加尾碼後可能超過 Win32 上限，改用同目錄的短暫存名稱。
                temporary=target.parent_path()/(L".mw-save"+suffix);
                if(CompareStringOrdinal(temporary.filename().c_str(),-1,target.filename().c_str(),-1,TRUE)==CSTR_EQUAL)continue;
            }
            temporary=ProfileIoPath(temporary);
            handle=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(handle!=INVALID_HANDLE_VALUE)break;
            const DWORD failure=GetLastError();
            if(failure!=ERROR_FILE_EXISTS&&failure!=ERROR_ALREADY_EXISTS)
                return SaveFileResult::TemporaryUnavailable;
        }
        if(handle==INVALID_HANDLE_VALUE)return SaveFileResult::TemporaryUnavailable;
        ownsTemporary=true;
        DWORD written{};
        if(!WriteFile(handle,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr)||written!=bytes.size()){
            cleanup();return SaveFileResult::WriteFailed;
        }
        if(!FlushFileBuffers(handle)){cleanup();return SaveFileResult::FlushFailed;}
        const BOOL closed=CloseHandle(handle);handle=INVALID_HANDLE_VALUE;
        if(!closed){cleanup();return SaveFileResult::CloseFailed;}
        if(!MoveFileExW(temporary.c_str(),ioTarget.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
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
        std::ifstream input(ProfileIoPath(target),std::ios::binary);
        if(!input)return false;
        return candidate.RestoreStream(input);
    }catch(...){return false;}
}

// 只有備用目錄缺少本世界的確切存檔，才讓已恢復的主目錄進度重新可見。
// 設定仍由原本的位置載入與寫入。兩份存檔均不搬移、不改名。
inline std::optional<std::filesystem::path> RecoveredRiverbankCheckpoint(
    const std::filesystem::path& local,const std::filesystem::path& selected,std::uint64_t expectedWorld) {
    namespace fs=std::filesystem;
    const auto fallback=local/"MistwatchRiverbankPlaytest"/"Aevrynd"/"Mistwatch";
    if(!local.is_absolute()||selected.parent_path()!=fallback||!selected.has_filename()||!expectedWorld)
        return std::nullopt;
    std::error_code error;
    if(fs::symlink_status(ProfileIoPath(selected),error).type()!=fs::file_type::not_found)return std::nullopt;
    const auto primary=local/"Aevrynd"/"Mistwatch"/selected.filename();
    SaveGame candidate;
    if(!ReadSaveFile(primary,candidate)||candidate.worldIdentity!=expectedWorld)return std::nullopt;
    return primary;
}

}
