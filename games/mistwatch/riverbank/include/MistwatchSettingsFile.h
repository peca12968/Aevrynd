#pragma once
#include <Windows.h>
#include <atomic>
#include <filesystem>
#include <string>
#include <string_view>

namespace Mistwatch {
inline bool WriteSettingsFile(const std::filesystem::path& file,std::string_view text) {
    if(file.empty()||text.empty()||text.size()>128)return false;
    std::error_code error;
    if(!file.parent_path().empty())std::filesystem::create_directories(file.parent_path(),error);
    if(error)return false;
    // 各程序／各次寫入使用獨立候選；不可截斷另一個仍在寫入的 .tmp。
    static std::atomic<unsigned long long> sequence{};
    const auto processId=std::to_wstring(GetCurrentProcessId());
    std::filesystem::path temporary;
    HANDLE handle=INVALID_HANDLE_VALUE;
    constexpr int maximumAttempts=8;
    for(int attempt=0;attempt<maximumAttempts;++attempt){
        const auto id=std::to_wstring(sequence.fetch_add(1,std::memory_order_relaxed));
        temporary=file;
        temporary+=L".tmp-"+processId+L"-"+id;
        if(temporary.native().size()>=MAX_PATH){
            // 設定與結案報告可寫入有效的長路徑。只縮短同目錄暫存名稱。
            temporary=file.parent_path()/(L".mw-profile."+processId+L"."+id+L".tmp");
            if(CompareStringOrdinal(temporary.filename().c_str(),-1,file.filename().c_str(),-1,TRUE)==CSTR_EQUAL)continue;
        }
        handle=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(handle!=INVALID_HANDLE_VALUE)break;
        const auto reason=GetLastError();
        if(reason!=ERROR_FILE_EXISTS&&reason!=ERROR_ALREADY_EXISTS)return false;
    }
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
