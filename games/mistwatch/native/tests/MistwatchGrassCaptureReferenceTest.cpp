#include "MistwatchGrassCaptureReference.h"
#include "MistwatchWorld.h"
#include <array>
#include <iostream>
#include <stdexcept>

namespace {
void Check(Aevrynd::Vec3 v,std::array<std::uint32_t,3> expected){
    const std::array words{std::bit_cast<std::uint32_t>(v.x),std::bit_cast<std::uint32_t>(v.y),std::bit_cast<std::uint32_t>(v.z)};
    if(words!=expected)throw std::runtime_error("Original grass-camera exact bit identity differs");
}
}
int main(){try{
    namespace Ref=Mistwatch::GrassCaptureReference;
    Check(Ref::Root,{0x413584c9,0x3ec28f78,0x3fb1d9a6});
    Check(Ref::Target(),{0x413584c9,0x3f2e1489,0x3fb1d9a6});
    Check(Ref::Position(),{0x4147eb2f,0x3f9d70ab,0x3e753398});
    const auto terrain=Mistwatch::CreateTerrain();
    if(!terrain.Contains(Ref::Root.x,Ref::Root.z)||
       std::bit_cast<std::uint32_t>(terrain.SampleHeight(Ref::Root.x,Ref::Root.z))!=0x3ec28f78u)
        throw std::runtime_error("Original camera root terrain identity differs");
    // 三個分量任一位元改變均被獨立期望值拒絕，不採用寬鬆距離容差。
    for(unsigned component=0;component<3;++component){
        auto p=Ref::Position();
        auto* coordinate=component==0?&p.x:component==1?&p.y:&p.z;
        *coordinate=std::bit_cast<float>(std::bit_cast<std::uint32_t>(*coordinate)^1u);
        bool rejected{};try{Check(p,{0x4147eb2f,0x3f9d70ab,0x3e753398});}
        catch(const std::runtime_error&){rejected=true;}
        if(!rejected)throw std::runtime_error("One-bit camera negative control accepted");
    }
    std::cout<<"GRASS_CAPTURE_REFERENCE_PASS root_bits=413584c9,3ec28f78,3fb1d9a6 target_bits=413584c9,3f2e1489,3fb1d9a6 position_bits=4147eb2f,3f9d70ab,3e753398 negative_controls=3\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
