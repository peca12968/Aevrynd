#include "FrontierWorldGame.h"
#include <array>
#include <filesystem>
#include <memory>

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR commandLine,int showCommand){const bool enableDLSS=Aevrynd::IsDLSSStartupRequested(commandLine);Aevrynd::Engine engine({L"Aevrynd Frontier World",1280,720,1.f/60.f,true,enableDLSS});std::filesystem::path captureDirectory;if(Aevrynd::CommandLineHasOption(commandLine?commandLine:L"",L"--capture-demo")){std::array<wchar_t,32768> executable{};const DWORD length=GetModuleFileNameW(nullptr,executable.data(),DWORD(executable.size()));if(length&&length<executable.size())captureDirectory=std::filesystem::path(std::wstring_view(executable.data(),length)).parent_path()/L"frontier-captures";}return engine.Run(std::make_unique<FrontierWorldGame>(captureDirectory),instance,showCommand);}
