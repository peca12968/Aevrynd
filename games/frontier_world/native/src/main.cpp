#include "FrontierWorldGame.h"
#include <memory>

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int showCommand){Aevrynd::Engine engine({L"Aevrynd Frontier World",1280,720,1.f/60.f});return engine.Run(std::make_unique<FrontierWorldGame>(),instance,showCommand);}
