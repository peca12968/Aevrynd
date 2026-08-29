#include "GameplayLabGame.h"
#include <array>
#include <memory>

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR commandLine,int showCommand){
    Aevrynd::Engine engine({L"Aevrynd Gameplay Lab",1280,720,1.f/60.f,true,false});std::filesystem::path captures;
    if(Aevrynd::CommandLineHasOption(commandLine?commandLine:L"",L"--capture-demo")){std::array<wchar_t,32768> path{};const DWORD n=GetModuleFileNameW(nullptr,path.data(),DWORD(path.size()));const bool gpuValidation=GetEnvironmentVariableW(L"AEVRYND_GPU_VALIDATION",nullptr,0)>0;if(n&&n<path.size())captures=std::filesystem::path(std::wstring_view(path.data(),n)).parent_path()/(gpuValidation?L"gameplay-captures-gbv":L"gameplay-captures-normal");}
    return engine.Run(std::make_unique<GameplayLabGame>(captures),instance,showCommand);
}
