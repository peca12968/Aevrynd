#include "CharacterLabGame.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR commandLine,int showCommand){
    std::filesystem::path executableDirectory;
    try{
        std::array<wchar_t,32768> executable{};const DWORD length=GetModuleFileNameW(nullptr,executable.data(),DWORD(executable.size()));if(length&&length<executable.size())executableDirectory=std::filesystem::path(std::wstring_view(executable.data(),length)).parent_path();
        const bool enableDLSS=Aevrynd::IsDLSSStartupRequested(commandLine);
        Aevrynd::Engine engine({L"Aevrynd Character Lab",1280,720,1.f/60.f,true,enableDLSS});
        std::filesystem::path captureDirectory;if(Aevrynd::CommandLineHasOption(commandLine?commandLine:L"",L"--capture-demo"))captureDirectory=executableDirectory/L"character-captures";
        return engine.Run(std::make_unique<CharacterLabGame>(captureDirectory),instance,showCommand);
    }catch(const std::exception& error){
        // 無人測試也要留下可機讀的啟動失敗原因，避免只看到 Windows fast-fail 代碼。
        if(!executableDirectory.empty())std::ofstream(executableDirectory/"character-lab-error.txt",std::ios::binary|std::ios::trunc)<<error.what()<<'\n';
        return 1;
    }catch(...){
        if(!executableDirectory.empty())std::ofstream(executableDirectory/"character-lab-error.txt",std::ios::binary|std::ios::trunc)<<"Unknown native failure\n";
        return 2;
    }
}
