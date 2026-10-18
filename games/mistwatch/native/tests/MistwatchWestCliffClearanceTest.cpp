#include "MistwatchWestCliffClearance.h"
#include <iostream>

namespace {
unsigned checks{};
void Check(bool condition){++checks;if(!condition)throw std::runtime_error("WEST_CLIFF_CLEARANCE_TEST_"+std::to_string(checks));}
template<class F> void Reject(F action){bool rejected=false;try{action();}catch(const std::invalid_argument&){rejected=true;}Check(rejected);}
Aevrynd::HeightField3D Field(){Aevrynd::HeightField3D t({{-50,0,50},21,33,.5f});t.Fill(10);return t;}
}
int main(){
    try{
        using Aevrynd::Vec3;using Mistwatch::WestCliffClearance::Envelope;
        auto canonical=Field(),derived=Field();derived.Fill(8);Envelope env(canonical,derived);
        const auto high=env.Query({-47,12,54},{-46,13,55},{-45,14,54});
        Check(!high.disjoint&&high.separated&&high.cells>0&&high.ceiling==10&&high.minimumY==12&&high.padding>0);
        const auto zero=env.Query({-47,12,54},{-46,13,55},{-45,14,54},{});
        Check(zero.disjoint==high.disjoint&&zero.separated==high.separated&&zero.minimumY==high.minimumY&&
              zero.ceiling==high.ceiling&&zero.padding==high.padding&&zero.cells==high.cells);
        constexpr auto sweep=Mistwatch::WestCliffClearance::FoliageSweep;
        const Vec3 outsideX{-49.6f,12,54},outsideZ{-47,12,50.45f};
        Check(env.Query(outsideX,outsideX,outsideX).disjoint);
        const auto sweptX=env.Query(outsideX,outsideX,outsideX,sweep);
        Check(!sweptX.disjoint&&sweptX.separated&&sweptX.cells>0);
        Check(env.Query(outsideZ,outsideZ,outsideZ).disjoint);
        const auto sweptZ=env.Query(outsideZ,outsideZ,outsideZ,sweep);
        Check(!sweptZ.disjoint&&sweptZ.separated&&sweptZ.cells>0);
        const Vec3 lowOutside{-49.6f,10,54};Check(!env.Query(lowOutside,lowOutside,lowOutside,sweep).separated);
        const Vec3 nearFloor{-47,10.005f,54};
        Check(env.Query(nearFloor,nearFloor,nearFloor).separated);
        Check(!env.Query(nearFloor,nearFloor,nearFloor,sweep).separated);
        const Vec3 touchPoint{-47,10.125f,54};Check(!env.Query(touchPoint,touchPoint,touchPoint,{0,.125f,0}).separated);
        const Vec3 guardEdge{-49.625f,12,54};
        Check(!env.Query(guardEdge,guardEdge,guardEdge,{.125f,0,0}).disjoint);
        Check(!env.Query({-47,10,54},{-46,12,55},{-45,12,54}).separated);
        Check(!env.Query({-47,9,54},{-46,12,55},{-45,12,54}).separated);
        Check(!env.Query({-47,7,54},{-46,7,55},{-45,7,54}).separated);
        // 投影為點或線的直立幾何仍須保留最低點。
        Check(env.Query({-47,12,54},{-47,14,54},{-47,13,54}).separated);
        Check(!env.Query({-47,10,54},{-47,14,54},{-47,13,54}).separated);
        Check(!env.Query({-47,9,54},{-47,14,55},{-47,13,56}).separated);
        const auto away=env.Query({0,0,0},{1,0,0},{0,0,1});Check(away.disjoint&&!away.separated&&away.cells==0);
        // 閉邊界兩側格子都納入，不能漏掉僅碰邊的高角點。
        derived.SetHeight(0,1,20);Envelope edge(canonical,derived);
        const auto touching=edge.Query({-49.5f,12,50.5f},{-49.5f,12,50.5f},{-49.5f,12,50.5f});
        Check(!touching.disjoint&&!touching.separated&&touching.ceiling==20&&touching.cells==4);
        derived=Field();derived.Fill(8);derived.SetHeight(7,10,20);Envelope peak(canonical,derived);
        Check(!peak.Query({-48,12,53},{-44,12,53},{-46,12,57}).separated);
        // 舊地形高點亦必須保護，不能只檢查挖低後的地形。
        canonical.SetHeight(7,10,22);derived.Fill(8);Envelope oldPeak(canonical,derived);
        Check(!oldPeak.Query({-48,12,53},{-44,12,53},{-46,12,57}).separated);
        Check(oldPeak.Query({-48,23,53},{-44,23,53},{-46,23,57}).separated);
        canonical=Field();derived=Field();Envelope roundoff(canonical,derived);
        const float next=std::nextafter(10.f,11.f);
        Check(!roundoff.Query({-47,next,54},{-46,next,55},{-45,next,54}).separated);
        const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
        Reject([&]{roundoff.Query(outsideX,outsideX,outsideX,{-1,0,0});});
        Reject([&]{roundoff.Query(outsideX,outsideX,outsideX,{0,-1,0});});
        Reject([&]{roundoff.Query(outsideX,outsideX,outsideX,{0,0,-1});});
        Reject([&]{roundoff.Query(outsideX,outsideX,outsideX,{nan,0,0});});
        Reject([&]{roundoff.Query(outsideX,outsideX,outsideX,{0,inf,0});});
        Reject([&]{roundoff.Query({nan,12,54},{-46,12,55},{-45,12,54});});
        Reject([&]{roundoff.Query({-47,inf,54},{-46,12,55},{-45,12,54});});
        derived.SetHeight(7,10,nan);Reject([&]{Envelope bad(canonical,derived);});
        derived=Field();canonical.SetHeight(7,10,inf);Reject([&]{Envelope bad(canonical,derived);});
        canonical=Field();derived=Aevrynd::HeightField3D({{-50,0,50},22,33,.5f});Reject([&]{Envelope bad(canonical,derived);});
        canonical=Aevrynd::HeightField3D({{0,0,0},2,2,1});derived=canonical;Reject([&]{Envelope bad(canonical,derived);});
        canonical=Aevrynd::HeightField3D({{nan,0,0},2,2,1});derived=canonical;Reject([&]{Envelope bad(canonical,derived);});
        // 原點很大時 float 格線退化，必須在索引／覆蓋證明之前拒絕。
        canonical=Aevrynd::HeightField3D({{1.e20f,0,50},3,3,.5f});derived=canonical;Reject([&]{Envelope bad(canonical,derived);});
        canonical=Aevrynd::HeightField3D({{-50,0,50},201,321,.05f});derived=canonical;Reject([&]{Envelope bad(canonical,derived);});
        // 正常負高度、非零 origin.y 仍使用 Height 的絕對高度。
        canonical=Aevrynd::HeightField3D({{-50,100,50},21,33,.5f});derived=canonical;
        canonical.Fill(-10);derived.Fill(-12);Envelope negative(canonical,derived);
        Check(negative.Query({-47,-9,54},{-46,-9,55},{-45,-9,54}).separated);
        Check(!negative.Query({-47,-11,54},{-46,-9,55},{-45,-9,54}).separated);
        std::cout<<"WEST_CLIFF_CLEARANCE_PASS checks="<<checks<<'\n';return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
