#include "MistwatchShelterWeatheredUv.h"
#include <iostream>

namespace {
using namespace Aevrynd;
unsigned checks{};
void Check(bool ok,const char* reason){++checks;if(!ok)throw std::runtime_error(reason);}
bool Same(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
}
int main()try {
    const auto original=Mistwatch::ShelterRoofUv::CreatePlankedRepairShelterRoofUv();
    const auto actual=Mistwatch::ShelterWeatheredUv::Create();
    const auto repeat=Mistwatch::ShelterWeatheredUv::Create();
    Check(actual.Valid()&&actual.vertices.size()==7392&&actual.indices.size()==10164,"Exact original shelter geometry membership");
    Check(actual.indices==original.indices&&actual.indices==repeat.indices,"Original topology unchanged");
    constexpr std::array<Vec2,4> bands{{{132.f/1024.f,216.f/1024.f},{360.f/1024.f,436.f/1024.f},
        {694.f/1024.f,758.f/1024.f},{932.f/1024.f,1006.f/1024.f}}};
    constexpr float decoded=2048.f*(16.f/32767.f);
    constexpr std::array<unsigned,5> counts{20,16,20,20,1};
    std::size_t first{},changed{};std::array<unsigned,4> used{};
    for(unsigned part=0;part<5;++part) {
        for(unsigned board=0;board<counts[part];++board) {
            const auto band=bands[(board+part*3u)%4];
            if(part<4)++used[(board+part*3u)%4];
            for(std::size_t i=first+std::size_t(board)*96;i<first+std::size_t(board+1)*96;++i) {
                const auto& a=actual.vertices[i];const auto& b=original.vertices[i];const auto& c=repeat.vertices[i];
                Check(Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z)&&a.normal==b.normal&&a.color==b.color,"No geometry, normal, wind, or tint change");
                Check(Same(a.u,c.u)&&Same(a.v,c.v),"Repeat UV bit identity");
                if(part==4){Check(Same(a.u,b.u)&&Same(a.v,b.v),"Entire tool table UV unchanged");continue;}
                const float x=(b.u*decoded-32.f/1024.f)/(48.f/1024.f);
                const float oldPhase=.1f+.025f*float(part==0?board%5u:(board+part*3u)%5u);
                const float y=(b.v*decoded-oldPhase)/.5f;
                const float expectedU=(band.x+x*(band.y-band.x))/decoded;
                const float expectedV=(.17f*float((board+part*2u)%5u)+y/1.4f)/decoded;
                Check(Same(a.u,expectedU)&&Same(a.v,expectedV),"Independent literal authored wood projection");
                Check(std::isfinite(a.u)&&std::isfinite(a.v)&&a.u*decoded>=band.x-1.e-7f&&a.u*decoded<=band.y+1.e-7f,"All actual coordinates inside selected source board");
                changed+=!Same(a.u,b.u)||!Same(a.v,b.v);
            }
        }
        first+=std::size_t(counts[part])*96;
    }
    Check(first==7392&&changed==7296,"Exactly 76 visual boards changed, equipment preserved");
    for(const auto count:used)Check(count==19,"Balanced four real source bands");
    for(std::size_t i=0;i<actual.indices.size();i+=3) {
        const auto jac=[](const Mesh3D& m,std::size_t k){const auto& a=m.vertices[m.indices[k]];const auto& b=m.vertices[m.indices[k+1]];const auto& c=m.vertices[m.indices[k+2]];
            return (double(b.u)-a.u)*(double(c.v)-a.v)-(double(c.u)-a.u)*(double(b.v)-a.v);};
        const double before=jac(original,i),after=jac(actual,i);
        Check(std::isfinite(before)&&std::isfinite(after)&&before!=0&&after!=0&&std::signbit(before)==std::signbit(after),"All real triangles preserve tangent handedness and nonzero UV area");
    }
    Check(Mistwatch::ShelterWeatheredUv::Fingerprint(actual)==Mistwatch::ShelterWeatheredUv::Fingerprint(repeat)&&
        Mistwatch::ShelterWeatheredUv::Fingerprint(actual)!=Mistwatch::ShelterWeatheredUv::Fingerprint(original),"Distinct deterministic UV identity");
    std::cout<<"SHELTER_WEATHERED_UV_PASS checks="<<checks<<" boards=76 bands=4 vertices_changed="<<changed<<" triangles=3388 geometry_unchanged=1 GPU_or_art_pass=0\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
