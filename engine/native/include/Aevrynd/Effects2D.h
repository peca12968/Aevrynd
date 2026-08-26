#pragma once
#include "Memory.h"
#include "Renderer.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Aevrynd {
enum class ParticleShape2D : uint8_t {Circle,Diamond,Spark};

struct ParticleBurst2D {
    float minimumSpeed{35.f},maximumSpeed{90.f};
    float minimumLife{.3f},maximumLife{.8f};
    float minimumSize{2.f},maximumSize{5.f},endSizeScale{.15f};
    Vec2 velocityBias{};
    float gravity{28.f},drag{.8f};
    ParticleShape2D shape{ParticleShape2D::Spark};
};

struct ParticleSystem2DStats {
    std::size_t emitted{},recycled{},dropped{},peakActive{};
    std::size_t rendered{},visibilityCulled{},renderCalls{},visibilityRenderCalls{};
    std::size_t lastSimulationCulled{},totalSimulationCulled{},updateCalls{},boundedUpdateCalls{};
};

class ParticleSystem2D {
public:
    explicit ParticleSystem2D(std::size_t capacity=2048,std::size_t retainedCapacity=128);
    void Clear(bool resetStats=true);
    void EmitBurst(Vec2 origin,Color color,std::size_t count,ParticleBurst2D settings={},uint32_t seed=1);
    void EmitRing(Vec2 origin,Color color,std::size_t count,float speed,ParticleBurst2D settings={},float phase=0.f);
    void Update(float deltaTime);
    void Update(float deltaTime,Bounds2D simulationBounds);
    void Render(DrawList& draw)const;
    void Render(DrawList& draw,Bounds2D worldBounds)const;
    std::size_t ActiveCount()const{return particles_.size();}
    std::size_t Capacity()const{return capacity_;}
    ReusableMemoryStats MemoryStats()const;
    std::size_t TrimUnusedMemory();
    ParticleSystem2DStats Stats()const{return stats_;}
private:
    struct Particle {Vec2 position{},velocity{};Color color{};float remaining{},lifetime{},startSize{},endSize{},gravity{},drag{};ParticleShape2D shape{ParticleShape2D::Circle};};
    void Emit(Vec2 origin,Vec2 velocity,Color color,float life,float size,const ParticleBurst2D& settings);
    void UpdateInternal(float deltaTime,const Bounds2D* simulationBounds);
    void RenderInternal(DrawList& draw,const Bounds2D* worldBounds)const;
    std::vector<Particle> particles_;
    std::size_t capacity_{},retainedCapacity_{},lastReleasedBytes_{};
    std::uint64_t trimCount_{};
    mutable ParticleSystem2DStats stats_{};
};

class ScreenShake2D {
public:
    void Reset(){remaining_=duration_=strength_=phase_=0;}
    void AddImpulse(float strength,float duration);
    void Update(float deltaTime);
    Vec2 Offset()const;
    float Amplitude()const;
    bool Active()const{return remaining_>0.f;}
private:
    float remaining_{},duration_{},strength_{},phase_{};
};
}
