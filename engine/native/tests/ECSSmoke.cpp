#include "Aevrynd/ECS.h"
#include <array>
#include <iostream>
#include <vector>

using namespace Aevrynd;

struct Position {float x{},y{};};
struct Velocity {float x{},y{};};

int main(){
    EntityRegistry registry;
    const Entity first=registry.Create();
    SparseSet<Position> positions;
    positions.Emplace(first,1.f,2.f);
    if(!registry.IsAlive(first)||!positions.Contains(first)||positions.TryGet(first)->x!=1.f)return 1;
    if(!registry.Destroy(first)||registry.IsAlive(first))return 2;
    const Entity replacement=registry.Create();
    if(replacement.index!=first.index||replacement.generation==first.generation||positions.Contains(replacement)||positions.TryGet(first)==nullptr)return 3;
    positions.Emplace(replacement,3.f,4.f);
    if(positions.Contains(first)||positions.TryGet(replacement)->x!=3.f)return 4;
    positions.Emplace(replacement,5.f,6.f);
    if(positions.Size()!=1||positions.TryGet(replacement)->x!=5.f||positions.TryGet(replacement)->y!=6.f)return 4;

    constexpr size_t EntityCount=10000;
    std::vector<Entity> entities;entities.reserve(EntityCount);
    SparseSet<Velocity> velocities;
    for(size_t index=0;index<EntityCount;++index){const Entity entity=registry.Create();entities.push_back(entity);positions.Emplace(entity,float(index),0.f);if(index%2==0)velocities.Emplace(entity,1.f,2.f);}
    if(registry.LivingCount()!=EntityCount+1||positions.Size()!=EntityCount+1||velocities.Size()!=EntityCount/2)return 5;

    size_t viewed=0;
    View(registry,positions,velocities).Each([&](Entity entity,Position& position,Velocity& velocity){position.x+=velocity.x;++viewed;if(!registry.IsAlive(entity))viewed=0;});
    if(viewed!=EntityCount/2)return 6;

    const Entity swapped=entities.back();
    const float swappedValue=positions.TryGet(swapped)->x;if(!positions.Remove(entities[1])||positions.Contains(entities[1])||!positions.Contains(swapped)||positions.TryGet(swapped)->x!=swappedValue||positions.Size()!=EntityCount)return 7;
    if(positions.Remove(entities[1]))return 8;

    for(size_t index=0;index<entities.size();index+=3)if(!registry.DeferDestroy(entities[index]))return 9;
    if(registry.DeferDestroy(entities[0]))return 9;
    const size_t expectedDeferred=(EntityCount+2)/3;
    if(registry.DeferredDestroyCount()!=expectedDeferred||registry.FlushDeferredDestroy()!=expectedDeferred||registry.DeferredDestroyCount())return 10;
    for(size_t index=0;index<entities.size();index+=3)if(registry.IsAlive(entities[index]))return 11;
    if(registry.DeferDestroy(first)||registry.FlushDeferredDestroy())return 12;
    const size_t deadPositions=positions.RemoveDead(registry),deadVelocities=velocities.RemoveDead(registry);if(deadPositions!=expectedDeferred||deadVelocities!=(EntityCount+5)/6)return 12;
    const Entity queued=registry.Create();if(!registry.DeferDestroy(queued)||!registry.Destroy(queued))return 12;const Entity renewed=registry.Create();if(renewed.index!=queued.index||!registry.DeferDestroy(renewed)||registry.FlushDeferredDestroy()!=1||registry.IsAlive(renewed))return 12;

    const ECSMemoryStats registryBefore=registry.MemoryStats();
    const ECSMemoryStats positionsBefore=positions.MemoryStats();
    registry.TrimMemory();positions.TrimMemory();
    const ECSMemoryStats registryAfter=registry.MemoryStats();
    const ECSMemoryStats positionsAfter=positions.MemoryStats();
    if(registryAfter.capacityBytes>registryBefore.capacityBytes||positionsAfter.capacityBytes>positionsBefore.capacityBytes||registryAfter.trimCount!=1||positionsAfter.trimCount!=1)return 13;
    if(!registry.IsAlive(replacement)||!positions.Contains(replacement)||positions.TryGet(replacement)->x!=5.f)return 14;

    std::cout<<"entities="<<registry.LivingCount()<<" positions="<<positions.Size()<<" view="<<viewed<<" registryBytes="<<registryAfter.capacityBytes<<" componentBytes="<<positionsAfter.capacityBytes<<'\n';
    return 0;
}
