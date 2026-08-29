#include "EngineLabGame.h"
#include <array>
#include <filesystem>
#include <memory>

namespace {
std::filesystem::path ExecutableDirectory(){std::array<wchar_t,32768> executable{};const DWORD length=GetModuleFileNameW(nullptr,executable.data(),DWORD(executable.size()));return length&&length<executable.size()?std::filesystem::path(std::wstring_view(executable.data(),length)).parent_path():std::filesystem::path{};}
std::filesystem::path EnvironmentPath(const wchar_t* name){std::array<wchar_t,32768> value{};const DWORD length=GetEnvironmentVariableW(name,value.data(),DWORD(value.size()));return length&&length<value.size()?std::filesystem::path(std::wstring_view(value.data(),length)):std::filesystem::path{};}
}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR commandLine,int showCommand){
    const bool enableDLSS=Aevrynd::IsDLSSStartupRequested(commandLine);
    Aevrynd::Engine engine({L"Aevrynd Engine Lab",1280,720,1.f/60.f,true,enableDLSS});
    const std::wstring_view arguments=commandLine?commandLine:L"";const bool shadowCaptureMode=Aevrynd::CommandLineHasOption(arguments,L"--capture-shadow-demo");std::filesystem::path captureDirectory;
    if(shadowCaptureMode){captureDirectory=EnvironmentPath(L"AEVRYND_ENGINE_LAB_SHADOW_CAPTURE_DIRECTORY");if(captureDirectory.empty())captureDirectory=ExecutableDirectory()/L"shadow-captures";}
    else if(Aevrynd::CommandLineHasOption(arguments,L"--capture-demo"))captureDirectory=ExecutableDirectory()/L"captures";
    return engine.Run(std::make_unique<EngineLabGame>(captureDirectory,shadowCaptureMode),instance,showCommand);
}
