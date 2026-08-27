#pragma once
#include "Mesh3D.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Aevrynd {
struct HeightFieldSettings3D {Vec3 origin{};unsigned width{2},depth{2};float cellSize{1.f};};
struct HeightFieldNaturalSettings3D {float amplitude{18.f},frequency{.012f},ridgeStrength{.58f},domainWarp{22.f};unsigned octaves{6};};
struct HeightFieldUltraSettings3D {
    float amplitude{24.f},frequency{.009f},ridgeStrength{.62f},valleyStrength{.28f};
    float domainWarp{34.f},warpFrequencyScale{.31f};
    unsigned octaves{7},depositionIterations{3};
    float talusSlope{.72f},depositionStrength{.18f};
    float maximumAbsoluteCoordinate{1000000.f};
    std::size_t maximumSamples{4194304},maximumScratchBytes{64u*1024u*1024u};
};
struct HeightFieldMeshSettings3D {Color lowColor{.12f,.28f,.1f,1},highColor{.42f,.5f,.32f,1},steepColor{.32f,.3f,.27f,1};float steepNormalY{.72f};Color shoreColor{.24f,.18f,.1f,1};float shoreHeight{-10000.f},shoreBlend{0.f};};
struct HeightFieldLodMeshSettings3D {
    unsigned sampleStride{1};
    bool includeSkirt{};
    float skirtDepth{2.f};
    std::size_t maximumVertices{4194304},maximumIndices{25165824};
};

class HeightField3D {
public:
    explicit HeightField3D(HeightFieldSettings3D settings={}){Configure(settings);}
    void Configure(HeightFieldSettings3D settings);
    void Fill(float height);
    void Generate(uint32_t seed,float amplitude=6.f,float frequency=.025f,unsigned octaves=4);
    void GenerateNatural(uint32_t seed,HeightFieldNaturalSettings3D settings={});
    // 失敗時保留原高度資料，避免容量或輸入錯誤留下半成品。
    bool GenerateUltra(uint32_t seed,HeightFieldUltraSettings3D settings={});
    bool SetHeight(unsigned x,unsigned z,float height);
    float Height(unsigned x,unsigned z)const;
    Vec3 SamplePosition(unsigned x,unsigned z)const;
    bool Contains(float x,float z)const;
    float SampleHeight(float x,float z)const;
    Vec3 SampleNormal(float x,float z)const;
    void FlattenCircle(Vec3 center,float innerRadius,float targetHeight,float blendDistance=0.f);
    Mesh3D CreateMesh(HeightFieldMeshSettings3D settings={})const;
    Mesh3D CreateLodMesh(HeightFieldLodMeshSettings3D lodSettings,HeightFieldMeshSettings3D meshSettings={})const;
    float MinimumHeight()const{return minimumHeight_;}
    float MaximumHeight()const{return maximumHeight_;}
    const HeightFieldSettings3D& Settings()const{return settings_;}
    std::size_t SampleCount()const{return heights_.size();}
private:
    void RefreshRange();
    std::size_t Index(unsigned x,unsigned z)const{return std::size_t(z)*settings_.width+x;}
    HeightFieldSettings3D settings_{};
    std::vector<float> heights_;
    float minimumHeight_{},maximumHeight_{};
};
}
