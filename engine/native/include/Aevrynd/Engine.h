#pragma once
#include <windows.h>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include "Audio.h"
#include "Game.h"
#include "Input.h"
#include "Memory.h"
#include "Renderer.h"
#include "Scene2D.h"

namespace Aevrynd {
#ifndef AEVRYND_ENGINE_VERSION
#define AEVRYND_ENGINE_VERSION "development"
#endif
inline constexpr std::string_view EngineVersion{AEVRYND_ENGINE_VERSION};
[[nodiscard]] bool CommandLineHasOption(std::wstring_view commandLine,std::wstring_view option);
[[nodiscard]] bool IsDLSSStartupRequested(const wchar_t* commandLine);
struct Mesh3D;
struct MeshResource3D;
struct EngineConfig { std::wstring title=L"Aevrynd Engine"; unsigned width=1280,height=720; float fixedStep=1.f/60.f; bool resizable=true,enableDLSS=false,enableAutomaticMemoryTrim=true; unsigned memoryPressureThresholdPercent=85; float memoryTrimCooldownSeconds=10.f; size_t minimumAutomaticMemoryReclaimBytes=512*1024; };
struct EngineMemoryStats {
    ProcessMemoryStats process{};
    TransientMemoryStats draw{};
    TransientMemoryStats renderer{};
    ReusableMemoryStats game{};
    size_t lastReleasedBytes{};
    uint64_t trimCount{};
    uint64_t automaticTrimCount{};
};
class Engine {
public:
    explicit Engine(EngineConfig config={}); ~Engine();
    int Run(std::unique_ptr<IGame> game,HINSTANCE instance,int showCommand);
    DrawList& Draw(){return draw_;}
    Input& GetInput(){return input_;}
    AudioEngine& Audio(){return audio_;}
    const AudioEngine& Audio()const{return audio_;}
    unsigned Width()const{return config_.width;}
    unsigned Height()const{return config_.height;}
    float Time()const{return time_;}
    void Quit(){running_=false;}
    void SetTitle(const std::wstring& title);
    void SetClearColor(Color color){clearColor_=color;}
    Color ClearColor()const{return clearColor_;}
    std::filesystem::path ExecutableDirectory()const;
    HWND Window()const{return hwnd_;}
    bool SupportsDLSS()const{return renderer_->SupportsDLSS();}
    bool DLSSModuleRequested()const{return renderer_->DLSSModuleRequested();}
    bool DLSSOn()const{return renderer_->DLSSOn();}
    void ToggleDLSS(){renderer_->ToggleDLSS();}
    MeshHandle3D CreateMesh3D(const Mesh3D& mesh);
    MeshResource3D CreateMeshResource3D(const Mesh3D& mesh);
    void DestroyMesh3D(MeshHandle3D mesh){renderer_->DestroyMesh(mesh);}
    void SetMeshMemoryBudget(size_t bytes){renderer_->SetMeshMemoryBudget(bytes);}
    MaterialTextureSetHandle3D CreateMaterialTextureSet3D(const MaterialTextureSetDefinition3D& definition){return renderer_->CreateMaterialTextureSet(definition);}
    void DestroyMaterialTextureSet3D(MaterialTextureSetHandle3D textures){renderer_->DestroyMaterialTextureSet(textures);}
    void SetMaterialTextureMemoryBudget(size_t bytes){renderer_->SetMaterialTextureMemoryBudget(bytes);}
    size_t TrimUnusedMemory();
    [[nodiscard]] EngineMemoryStats GetMemoryStats()const;
    RendererStats GetRendererStats()const{return renderer_->GetStats();}
    unsigned RenderWidth()const{return renderer_->RenderWidth();}
    unsigned RenderHeight()const{return renderer_->RenderHeight();}
private:
    static LRESULT CALLBACK WindowProc(HWND,UINT,WPARAM,LPARAM);
    LRESULT HandleMessage(UINT,WPARAM,LPARAM);
    EngineConfig config_; HWND hwnd_{}; bool running_{true},resizing_{},minimized_{},trimPending_{},automaticTrimmed_{}; float time_{},lastMemorySampleTime_{-1.f};
    Input input_; AudioEngine audio_; DrawList draw_; std::unique_ptr<D3D12Renderer> renderer_; std::unique_ptr<IGame> game_;
    std::unordered_map<uint32_t,std::weak_ptr<const Mesh3D>> meshCpuCache_;
    ProcessMemoryStats processMemoryStats_{};size_t lastReleasedBytes_{};uint64_t trimCount_{},automaticTrimCount_{};float lastAutomaticTrimTime_{};
    Color clearColor_{.018f,.045f,.055f,1};
};
}
