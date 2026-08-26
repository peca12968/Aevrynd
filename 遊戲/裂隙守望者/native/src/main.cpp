#include <windows.h>
#include <Aevrynd/Engine.h>
#include "RiftwardenGame.h"
#include <exception>

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show){
    try{
        Aevrynd::Engine engine({L"裂境守望 · Aevrynd Engine / DirectX 12",1280,720,1.f/60.f});
        return engine.Run(std::make_unique<RiftwardenGame>(),instance,show);
    }catch(const std::exception& e){MessageBoxA(nullptr,e.what(),"Aevrynd Engine fatal error",MB_OK|MB_ICONERROR);return 1;}
}
