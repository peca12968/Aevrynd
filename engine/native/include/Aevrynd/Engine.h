#pragma once
#include <windows.h>
#include <filesystem>
#include <memory>
#include <string>
#include "Game.h"
#include "Input.h"
#include "Renderer.h"
#include "Scene2D.h"

namespace Aevrynd {
struct Mesh3D;
struct MeshResource3D;
struct EngineConfig { std::wstring title=L"Aevrynd Engine"; unsigned width=1280,height=720; float fixedStep=1.f/60.f; };
class Engine {
public:
    explicit Engine(EngineConfig config={}); ~Engine();
    int Run(std::unique_ptr<IGame> game,HINSTANCE instance,int showCommand);
    DrawList& Draw(){return draw_;}
    Input& GetInput(){return input_;}
    unsigned Width()const{return config_.width;}
    unsigned Height()const{return config_.height;}
    float Time()const{return time_;}
    void Quit(){running_=false;}
    void SetTitle(const std::wstring& title);
    std::filesystem::path ExecutableDirectory()const;
    HWND Window()const{return hwnd_;}
    bool SupportsDLSS()const{return renderer_->SupportsDLSS();}
    bool DLSSOn()const{return renderer_->DLSSOn();}
    void ToggleDLSS(){renderer_->ToggleDLSS();}
    MeshHandle3D CreateMesh3D(const Mesh3D& mesh);
    MeshResource3D CreateMeshResource3D(const Mesh3D& mesh);
    void DestroyMesh3D(MeshHandle3D mesh){renderer_->DestroyMesh(mesh);}
    RendererStats GetRendererStats()const{return renderer_->GetStats();}
    unsigned RenderWidth()const{return renderer_->RenderWidth();}
    unsigned RenderHeight()const{return renderer_->RenderHeight();}
private:
    static LRESULT CALLBACK WindowProc(HWND,UINT,WPARAM,LPARAM);
    LRESULT HandleMessage(UINT,WPARAM,LPARAM);
    EngineConfig config_; HWND hwnd_{}; bool running_{true},resizing_{}; float time_{};
    Input input_; DrawList draw_; std::unique_ptr<D3D12Renderer> renderer_; std::unique_ptr<IGame> game_;
};
}
