#pragma once
#include "Mesh3D.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Aevrynd {
struct HeightFieldSettings3D {Vec3 origin{};unsigned width{2},depth{2};float cellSize{1.f};};
struct HeightFieldMeshSettings3D {Color lowColor{.12f,.28f,.1f,1},highColor{.42f,.5f,.32f,1},steepColor{.32f,.3f,.27f,1};float steepNormalY{.72f};};

class HeightField3D {
public:
    explicit HeightField3D(HeightFieldSettings3D settings={}){Configure(settings);}
    void Configure(HeightFieldSettings3D settings);
    void Fill(float height);
    void Generate(uint32_t seed,float amplitude=6.f,float frequency=.025f,unsigned octaves=4);
    bool SetHeight(unsigned x,unsigned z,float height);
    float Height(unsigned x,unsigned z)const;
    Vec3 SamplePosition(unsigned x,unsigned z)const;
    bool Contains(float x,float z)const;
    float SampleHeight(float x,float z)const;
    Vec3 SampleNormal(float x,float z)const;
    void FlattenCircle(Vec3 center,float innerRadius,float targetHeight,float blendDistance=0.f);
    Mesh3D CreateMesh(HeightFieldMeshSettings3D settings={})const;
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
