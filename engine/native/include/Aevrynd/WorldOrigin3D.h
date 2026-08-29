#pragma once
#include "Math.h"
#include <cstdint>
#include <thread>
#include <vector>

namespace Aevrynd {
struct GlobalChunkCoordinate3D {std::int64_t x{},y{},z{};constexpr bool operator==(const GlobalChunkCoordinate3D&)const=default;};
struct GlobalPosition3D {GlobalChunkCoordinate3D chunk{};Vec3 offset{};constexpr bool operator==(const GlobalPosition3D& value)const{return chunk==value.chunk&&offset.x==value.offset.x&&offset.y==value.offset.y&&offset.z==value.offset.z;}};
struct WorldOrigin3D {GlobalPosition3D position{};std::uint64_t generation{1};constexpr bool operator==(const WorldOrigin3D&)const=default;};
struct WorldOriginRebaseEvent3D {WorldOrigin3D previous{},current{};Vec3 localDelta{};explicit operator bool()const{return current.generation&&current.generation!=previous.generation;}};
enum class WorldOriginHistoryPolicy3D:std::uint8_t {Reset};

bool NormalizeGlobalPosition3D(GlobalPosition3D& value,float chunkSize=32.f)noexcept;
bool AddGlobalPosition3D(GlobalPosition3D value,Vec3 delta,GlobalPosition3D& result,float chunkSize=32.f)noexcept;
bool ToLocalPosition3D(GlobalPosition3D value,WorldOrigin3D origin,Vec3& result,float chunkSize=32.f)noexcept;
bool ToGlobalPosition3D(Vec3 local,WorldOrigin3D origin,GlobalPosition3D& result,float chunkSize=32.f)noexcept;

class IWorldOriginParticipant3D {
public:
 virtual ~IWorldOriginParticipant3D()=default;
 virtual bool PrepareWorldOriginRebase3D(const WorldOriginRebaseEvent3D& event)=0;
 virtual void CommitWorldOriginRebase3D(const WorldOriginRebaseEvent3D& event)noexcept=0;
 virtual void AbortWorldOriginRebase3D()noexcept=0;
};

class WorldOriginCoordinator3D {
public:
 explicit WorldOriginCoordinator3D(WorldOrigin3D initial={},float chunkSize=32.f);
 bool Register(IWorldOriginParticipant3D& participant);
 bool Unregister(IWorldOriginParticipant3D& participant)noexcept;
 bool Request(GlobalPosition3D origin);
 bool ApplyFrameBoundary();
 WorldOrigin3D Origin()const noexcept{return origin_;}
 bool Pending()const noexcept{return pending_;}
 std::uint64_t AppliedRebases()const noexcept{return applied_;}
private:
 WorldOrigin3D origin_{};GlobalPosition3D requested_{};float chunkSize_{32.f};std::thread::id owner_{};std::vector<IWorldOriginParticipant3D*> participants_{};bool pending_{};std::uint64_t applied_{};
};
}
