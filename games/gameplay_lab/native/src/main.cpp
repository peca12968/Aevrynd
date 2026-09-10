#include "GameplayLabGame.h"
#include "GameplayUxReplayProduction.h"
#include <array>
#include <fstream>
#include <memory>
#include <stdexcept>

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR commandLine,int showCommand){
    const std::wstring arguments=commandLine?commandLine:L"";
    if(Aevrynd::CommandLineHasOption(commandLine?commandLine:L"",L"--ux-replay")){
      std::array<wchar_t,32768> executable{};const DWORD length=GetModuleFileNameW(nullptr,executable.data(),DWORD(executable.size()));
      if(!length||length>=executable.size())return 80;
      return RunGameplayUxReplayProduction(std::filesystem::path(std::wstring_view(executable.data(),length)).parent_path()/"gameplay-ux-replay-receipt.json");
    }
    std::filesystem::path captures;std::uint32_t temporalFixtureWarmup{};bool temporalFixtureCloudOff{};
    if(Aevrynd::CommandLineHasOption(commandLine?commandLine:L"",L"--capture-demo")){std::array<wchar_t,32768> path{};const DWORD n=GetModuleFileNameW(nullptr,path.data(),DWORD(path.size()));const bool gpuValidation=GetEnvironmentVariableW(L"AEVRYND_GPU_VALIDATION",nullptr,0)>0;if(n&&n<path.size())captures=std::filesystem::path(std::wstring_view(path.data(),n)).parent_path()/(gpuValidation?L"gameplay-captures-gbv":L"gameplay-captures-normal");}
    const auto fixture=arguments.find(L"--temporal-fixture");
    if(fixture!=std::wstring::npos){
      // 測試入口只接受正式 third-day 路由與三個明確 warmup 值，未知輸入一律關閉失敗。
      if(arguments==L"--temporal-fixture third-day --warmup 3")temporalFixtureWarmup=3;
      else if(arguments==L"--temporal-fixture third-day --warmup 4")temporalFixtureWarmup=4;
      else if(arguments==L"--temporal-fixture third-day --warmup 5")temporalFixtureWarmup=5;
      else if(arguments==L"--temporal-fixture third-day --warmup 3 --isolation cloud-off"){temporalFixtureWarmup=3;temporalFixtureCloudOff=true;}
      else return 81;
      std::array<wchar_t,32768> path{};const DWORD n=GetModuleFileNameW(nullptr,path.data(),DWORD(path.size()));const bool gpuValidation=GetEnvironmentVariableW(L"AEVRYND_GPU_VALIDATION",nullptr,0)>0;
      if(!n||n>=path.size())return 85;
      captures=std::filesystem::path(std::wstring_view(path.data(),n)).parent_path()/std::format(L"temporal-fixture-third-day-warmup-{}-{}{}",temporalFixtureWarmup,gpuValidation?L"gbv":L"normal",temporalFixtureCloudOff?L"-cloud-off":L"");
    }
    const auto fatal=[&](const char* value){if(captures.empty())return;std::filesystem::create_directories(captures);std::ofstream output(captures/"gameplay-fatal-error.txt",std::ios::trunc);output<<value<<'\n';output.flush();};
    try{
        Aevrynd::EngineConfig config{L"Aevrynd Gameplay Lab",1280,720,1.f/60.f,true,false};
        // Formal capture owns a deterministic multi-frame MSAA/TAA sequence.  Automatic
        // pressure trimming happens after Render and can otherwise release those live
        // resources before the game consumes the completed-frame receipt on the next tick.
        config.enableAutomaticMemoryTrim=captures.empty();
        Aevrynd::Engine engine(config);
        return engine.Run(std::make_unique<GameplayLabGame>(captures,temporalFixtureWarmup,temporalFixtureCloudOff),instance,showCommand);
    }catch(const std::exception& error){fatal(error.what());return 70;}catch(...){fatal("unknown exception");return 71;}
}
