#include "FrontierWorldGame.h"
#include <memory>

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR commandLine,int showCommand){const bool enableDLSS=Aevrynd::IsDLSSStartupRequested(commandLine);Aevrynd::Engine engine({L"Aevrynd Frontier World",1280,720,1.f/60.f,true,enableDLSS});return engine.Run(std::make_unique<FrontierWorldGame>(),instance,showCommand);}
