#pragma once
#include "Aevrynd/Animation3D.h"
#include <cstdint>

namespace Aevrynd::RootMotionInternal {
struct Motion3D {double x{},y{},z{},yaw{};};
struct Twist3D {double x{},y{},z{},yaw{};};

bool ValidateRootTrack(const AnimationClip3D&,std::size_t root)noexcept;
JointTransform3D SampleRootTransform(const Skeleton3D&,const AnimationClip3D&,std::size_t root,double wrappedTime)noexcept;
bool ExtractInterval(const Skeleton3D&,const AnimationClip3D&,std::size_t root,double fromTime,double toTime,std::uint32_t maximumCycles,bool extractVertical,Motion3D&,std::int64_t& crossedCycles)noexcept;
// Q32.32 路徑先以整數拆分 floor cycle 與 phase，禁止巨大 cycle 先轉 double 而遺失相位。
bool ExtractIntervalQ32(const Skeleton3D&,const AnimationClip3D&,std::size_t root,std::int64_t fromQ32,std::int64_t toQ32,std::uint32_t maximumCycles,bool extractVertical,Motion3D&,std::int64_t& crossedCycles)noexcept;
bool LogSE2(Motion3D,Twist3D&)noexcept;
bool ExpSE2(Twist3D,Motion3D&)noexcept;
Motion3D Compose(Motion3D,Motion3D)noexcept;
Motion3D Inverse(Motion3D)noexcept;
double NormalizeYaw(double)noexcept;
double WrappedPhase(double time,double duration)noexcept;
bool NormalizedClockToQ32(double value,std::int64_t& output)noexcept;
// delta 先量化為 Q32.32，再做 checked integer addition；失敗時 output 保持不變。
bool AdvanceNormalizedClockQ32(std::int64_t current,double delta,std::int64_t& output)noexcept;
double WrappedPhaseQ32(std::int64_t value)noexcept;
}
