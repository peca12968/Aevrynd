#include "GameplayLabGame.h"
#include <array>
#include <fstream>
#include <memory>
#include <stdexcept>

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR commandLine,int showCommand){
    std::filesystem::path captures;
    if(Aevrynd::CommandLineHasOption(commandLine?commandLine:L"",L"--capture-demo")){std::array<wchar_t,32768> path{};const DWORD n=GetModuleFileNameW(nullptr,path.data(),DWORD(path.size()));const bool gpuValidation=GetEnvironmentVariableW(L"AEVRYND_GPU_VALIDATION",nullptr,0)>0;if(n&&n<path.size())captures=std::filesystem::path(std::wstring_view(path.data(),n)).parent_path()/(gpuValidation?L"gameplay-captures-gbv":L"gameplay-captures-normal");}
    const auto fatal=[&](const char* value){if(captures.empty())return;std::filesystem::create_directories(captures);std::ofstream output(captures/"gameplay-fatal-error.txt",std::ios::trunc);output<<value<<'\n';output.flush();};
    try{Aevrynd::Engine engine({L"Aevrynd Gameplay Lab",1280,720,1.f/60.f,true,false});return engine.Run(std::make_unique<GameplayLabGame>(captures),instance,showCommand);}catch(const std::exception& error){fatal(error.what());return 70;}catch(...){fatal("unknown exception");return 71;}
}
