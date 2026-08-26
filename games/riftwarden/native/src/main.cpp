#include <windows.h>
#include <Rift/Engine.h>
#include "RiftwardenGame.h"
#include <exception>

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show){
    try{
        Rift::Engine engine({L"裂境守望 · Rift Engine / DirectX 12",1280,720,1.f/60.f});
        return engine.Run(std::make_unique<RiftwardenGame>(),instance,show);
    }catch(const std::exception& e){MessageBoxA(nullptr,e.what(),"Rift Engine fatal error",MB_OK|MB_ICONERROR);return 1;}
}
