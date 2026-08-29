#pragma once
#include "CollisionWorld3D.h"
#include "HeightField3D.h"
#include "Memory.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Aevrynd {
struct NavigationCell3D {int x{},z{};constexpr bool operator==(const NavigationCell3D&)const=default;};
struct NavigationGridSettings3D {Vec3 origin{-32,0,-32};unsigned width{64},depth{64};float cellSize{1.f};std::size_t maximumVisitedNodes{4096};bool allowDiagonal{true};};
struct NavigationPathStats3D {std::size_t visitedNodes{},openPushes{},rawPoints{},simplifiedPoints{};float totalCost{};bool reachedGoal{},searchLimitReached{};};
struct NavigationPath3D {std::vector<Vec3> points;NavigationPathStats3D stats{};explicit operator bool()const{return stats.reachedGoal&&!points.empty();}};
struct NavigationWorkspaceStats3D {std::size_t cellCapacity{},openCapacity{},rawCapacity{},simplifiedCapacity{};};
class NavigationGrid3D;

class NavigationQueryWorkspace3D {
public:
    void Reserve(std::size_t cells);
    std::size_t TrimUnusedMemory(std::size_t retainedCells=0);
    ReusableMemoryStats MemoryStats()const;
    NavigationWorkspaceStats3D Stats()const;
private:
    struct OpenNode {float score{};std::size_t index{};};
    friend class NavigationGrid3D;
    std::vector<OpenNode> open_;
    std::vector<float> costs_;
    std::vector<int64_t> parents_;
    std::vector<uint8_t> closed_;
    std::vector<NavigationCell3D> raw_,simplified_;
    std::size_t retainedCells_{};
    std::size_t lastReleasedBytes_{};
    uint64_t trimCount_{};
};

class NavigationGrid3D {
public:
    explicit NavigationGrid3D(NavigationGridSettings3D settings={}){Configure(settings);}
    void Configure(NavigationGridSettings3D settings);
    void Clear(uint8_t traversalCost=1);
    bool SetTraversalCost(NavigationCell3D cell,uint8_t traversalCost);
    bool SetBlocked(NavigationCell3D cell,bool blocked=true){return SetTraversalCost(cell,blocked?0:1);}
    uint8_t TraversalCost(NavigationCell3D cell)const;
    bool IsWalkable(NavigationCell3D cell)const{return TraversalCost(cell)>0;}
    bool InBounds(NavigationCell3D cell)const;
    NavigationCell3D CoordinateAt(Vec3 position)const;
    Vec3 WorldPosition(NavigationCell3D cell)const;
    std::size_t Rasterize(const CollisionWorld3D& world,CollisionQueryFilter3D filter={},float agentRadius=.45f,float agentHeight=1.8f);
    std::size_t ApplyHeightField(const HeightField3D& terrain,float maximumSlopeDegrees=50.f,float slopeCostScale=6.f,bool blockOutside=true);
    NavigationPath3D FindPath(Vec3 start,Vec3 goal)const;
    NavigationPathStats3D FindPathInto(Vec3 start,Vec3 goal,NavigationQueryWorkspace3D& workspace,std::vector<Vec3>& output)const;
    std::size_t BlockedCellCount()const;
    const NavigationGridSettings3D& Settings()const{return settings_;}
    bool CanRebaseOrigin(Vec3 delta)const noexcept;
    void RebaseOrigin(Vec3 delta)noexcept;
private:
    std::size_t Index(NavigationCell3D cell)const{return std::size_t(cell.z)*settings_.width+std::size_t(cell.x);}
    NavigationCell3D Cell(std::size_t index)const{return{int(index%settings_.width),int(index/settings_.width)};}
    bool FindNearestWalkable(NavigationCell3D requested,NavigationCell3D& result)const;
    bool HasLineOfSight(NavigationCell3D from,NavigationCell3D to)const;
    NavigationGridSettings3D settings_{};
    std::vector<uint8_t> traversalCosts_;
};
}
