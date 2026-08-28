#include <Aevrynd/Renderer.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

using namespace Aevrynd;

int main(){
    const LocalReflectionProbe3D large{{20},{-4,-4,-4},{4,4,4},{0,0,0},1};
    const LocalReflectionProbe3D small{{10},{-1,-1,-1},{1,1,1},{.3f,0,0},1};
    const LocalReflectionProbe3D priority{{30},{-3,-3,-3},{3,3,3},{0,0,0},2};
    if(!ValidLocalReflectionProbe3D(large)||!ValidLocalReflectionProbe3D(small)||!ValidLocalReflectionProbe3D(priority))return 1;
    auto invalid=small;invalid.maximum.x=invalid.minimum.x;if(ValidLocalReflectionProbe3D(invalid))return 2;
    invalid=small;invalid.capturePosition.x=(std::numeric_limits<float>::quiet_NaN)();if(ValidLocalReflectionProbe3D(invalid))return 3;
    std::array probes{large,small,priority};
    if(SelectLocalReflectionProbe3D({0,0,0},probes)!=2||SelectLocalReflectionProbe3D({3.5f,0,0},probes)!=0||SelectLocalReflectionProbe3D({8,0,0},probes)!=SIZE_MAX)return 4;
    std::array reordered{priority,large,small};if(SelectLocalReflectionProbe3D({0,0,0},reordered)!=0)return 5;
    std::array equalPriority{large,small};if(SelectLocalReflectionProbe3D({0,0,0},equalPriority)!=1)return 6;
    const auto corrected=BoxProjectedReflectionDirection3D({.5f,.2f,0},{1,.2f,.1f},small);
    if(!std::isfinite(corrected.x)||!std::isfinite(corrected.y)||!std::isfinite(corrected.z)||std::abs(Length(corrected)-1.f)>.001f)return 7;
    const Vec3 bad=BoxProjectedReflectionDirection3D({0,0,0},{(std::numeric_limits<float>::infinity)(),0,0},small);if(Length(bad)!=0)return 8;
    DrawList draw{DrawListCapacityProfile::Minimal2D};
    if(!draw.AddLocalReflectionProbe(large)||!draw.AddLocalReflectionProbe(small)||!draw.MoveLocalReflectionProbe(1,0)||draw.LocalReflectionProbes().front().environment!=small.environment||!draw.RemoveLocalReflectionProbe(1)||draw.LocalReflectionProbes().size()!=1)return 9;
    draw.Clear();for(size_t index=0;index<DrawList::MaximumLocalReflectionProbes;++index){auto probe=small;probe.environment.value=uint32_t(index+1);if(!draw.AddLocalReflectionProbe(probe))return 10;}if(draw.AddLocalReflectionProbe(small))return 11;
    const auto retained=draw.MemoryStats().trackedCapacityBytes;draw.TrimTransientMemory();if(!draw.LocalReflectionProbes().empty()||draw.MemoryStats().trackedCapacityBytes>=retained)return 12;
    constexpr size_t Iterations=1000000;size_t checksum{};const auto begin=std::chrono::steady_clock::now();for(size_t index=0;index<Iterations;++index){const float x=float(int(index%120)-60)*.1f;checksum^=SelectLocalReflectionProbe3D({x,0,0},probes);(void)BoxProjectedReflectionDirection3D({0,.1f,0},{1,float(index&7)*.01f,.1f},small);}const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-begin).count();
    std::cout<<"LocalReflectionProbe3D iterations="<<Iterations<<" nsPerIteration="<<double(elapsed)/Iterations<<" probeBytes="<<sizeof(LocalReflectionProbe3D)<<" maxGpuBytes="<<3*DrawList::MaximumLocalReflectionProbes*3*sizeof(Vec4)<<" checksum="<<checksum<<'\n';
    return elapsed<=0?13:0;
}
