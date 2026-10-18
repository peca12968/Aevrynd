#include <Aevrynd/TerrainUphillLimit3D.h>
#include <iostream>
#include <stdexcept>

namespace {
unsigned checks{};
void Check(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
template<class F>Aevrynd::HeightField3D Field(Aevrynd::HeightFieldSettings3D settings,F height){
    Aevrynd::HeightField3D t(settings);const auto& s=t.Settings();
    for(unsigned z=0;z<s.depth;++z)for(unsigned x=0;x<s.width;++x)t.SetHeight(x,z,height(x,z));
    t.RecalculateRange();return t;
}
}
int main(){try{
    using namespace Aevrynd;
    auto shallow=Field({{-2,7,-2},5,5,1},[](unsigned x,unsigned z){return .4f*float(x)+.3f*float(z);});
    const auto flat=LimitTerrainUphill3D(shallow,{-.75f,9,-.75f},{1,0,1});
    Check(flat.valid&&!flat.limited&&flat.fraction==1,"shallow slope allowed");
    auto steep=Field({{-2,7,-2},5,5,1},[](unsigned x,unsigned){return 2.f*float(x);});
    auto hit=LimitTerrainUphill3D(steep,{-.75f,9,-.75f},{1,0,0});
    Check(hit.valid&&hit.limited&&hit.fraction==0&&hit.gradient.x==2&&hit.gradient.y==0,"starts steep uphill");
    auto down=LimitTerrainUphill3D(steep,{.75f,9,-.75f},{-1,0,0});
    Check(down.valid&&!down.limited&&down.fraction==1,"downhill allowed");
    auto tangent=LimitTerrainUphill3D(steep,{-.75f,9,-.75f},{0,0,1});
    Check(tangent.valid&&!tangent.limited,"tangent allowed");
    Check(!LimitTerrainUphill3D(steep,{-.75f,9,-.75f},{1,0,0},2).limited,"grade exactly at threshold");
    auto ramp=Field({{0,100,0},7,3,1},[](unsigned x,unsigned){return x<=2?0.f:2.f*float(x-2);});
    hit=LimitTerrainUphill3D(ramp,{.5f,0,.5f},{3,0,0});
    Check(hit.valid&&hit.limited&&hit.fraction==std::nextafter(.5f,0.f)&&hit.cellX==2,"flat then steep grid crossing");
    auto diagonal=Field({{0,50,0},2,2,1},[](unsigned x,unsigned z){return x&&z?4.f:0.f;});
    hit=LimitTerrainUphill3D(diagonal,{.1f,0,.1f},{.8f,0,.8f});
    Check(hit.valid&&hit.limited&&hit.fraction<.5f&&hit.fraction>.4999f&&hit.triangle==1,"actual diagonal crossing");
    down=LimitTerrainUphill3D(diagonal,{.9f,0,.9f},{-.8f,0,-.8f});
    Check(down.valid&&!down.limited,"diagonal downhill");
    tangent=LimitTerrainUphill3D(diagonal,{.25f,0,.75f},{.5f,0,-.5f});
    Check(tangent.valid&&!tangent.limited,"motion exactly along shared diagonal");
    hit=LimitTerrainUphill3D(diagonal,{.25f,0,.75f},{.2f,0,.1f});
    Check(hit.valid&&hit.limited&&hit.fraction==0,"uphill departure from diagonal");
    auto narrow=Field({{0,0,0},11,3,.1f},[](unsigned x,unsigned){return x==5?1.f:0.f;});
    hit=LimitTerrainUphill3D(narrow,{.05f,0,.05f},{.9f,0,0});
    Check(hit.valid&&hit.limited&&hit.fraction>.38f&&hit.fraction<.40f,"interior narrow peak despite equal endpoint heights");
    const auto repeat=LimitTerrainUphill3D(narrow,{.05f,0,.05f},{.9f,0,0});
    Check(repeat.fraction==hit.fraction&&repeat.cellX==hit.cellX&&repeat.triangle==hit.triangle&&repeat.cellsChecked==hit.cellsChecked,"deterministic repeated traversal");
    auto translated=Field({{100,999,-200},7,3,1},[](unsigned x,unsigned){return x<=2?-20.f:-20.f+2.f*float(x-2);});
    hit=LimitTerrainUphill3D(translated,{100.5f,-20,-199.5f},{3,0,0});
    Check(hit.valid&&hit.limited&&hit.fraction==std::nextafter(.5f,0.f),"translated field with absolute heights");
    hit=LimitTerrainUphill3D(ramp,{.5f,0,1},{3,0,0});
    Check(hit.valid&&hit.limited&&hit.fraction==std::nextafter(.5f,0.f),"segment on closed grid line");
    hit=LimitTerrainUphill3D(ramp,{0,0,0},{0,0,0});
    Check(hit.valid&&!hit.limited&&hit.fraction==1,"zero horizontal displacement at outer corner");
    hit=LimitTerrainUphill3D(ramp,{3,2,1},{0,2,0});
    Check(hit.valid&&!hit.limited&&hit.fraction==1,"pure vertical not a horizontal slope restriction");
    hit=LimitTerrainUphill3D(ramp,{6,8,2},{0,0,0});
    Check(hit.valid&&!hit.limited,"maximum closed terrain corner valid");
    const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
    Check(!LimitTerrainUphill3D(ramp,{nan,0,0},{1,0,0}).valid,"NaN start rejected");
    Check(!LimitTerrainUphill3D(ramp,{1,inf,1},{1,0,0}).valid,"nonfinite vertical start rejected");
    Check(!LimitTerrainUphill3D(ramp,{1,0,1},{1,nan,0}).valid,"nonfinite vertical displacement rejected");
    Check(!LimitTerrainUphill3D(ramp,{1,0,1},{inf,0,0}).valid,"infinite displacement rejected");
    Check(!LimitTerrainUphill3D(ramp,{1,0,1},{1,0,0},nan).valid,"NaN grade rejected");
    Check(!LimitTerrainUphill3D(ramp,{1,0,1},{1,0,0},-1).valid,"negative grade rejected");
    Check(!LimitTerrainUphill3D(ramp,{-1,0,1},{1,0,0}).valid,"outside start rejected");
    Check(!LimitTerrainUphill3D(ramp,{5,0,1},{2,0,0}).valid,"outside endpoint rejected");
    auto invalid=ramp;invalid.SetHeight(2,1,nan);
    Check(!LimitTerrainUphill3D(invalid,{1,0,.5f},{2,0,0}).valid,"intersected invalid sample rejected");
    auto large=Field({{0,0,0},130,130,.1f},[](unsigned,unsigned){return 0.f;});
    Check(!LimitTerrainUphill3D(large,{.05f,0,.05f},{12.8f,0,12.8f}).valid,"bounded candidate grid budget");
    auto collapsed=Field({{1.e20f,0,0},3,3,.5f},[](unsigned,unsigned){return 0.f;});
    Check(!LimitTerrainUphill3D(collapsed,{1.e20f,0,.5f},{}).valid,"collapsed float grid rejected");
    std::cout<<"TERRAIN_UPHILL_LIMIT_PASS checks="<<checks<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"TERRAIN_UPHILL_LIMIT_FAIL "<<e.what()<<'\n';return 1;}}
