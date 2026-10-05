#include <span>
#include <Aevrynd/MeshCollisionGridSolid3D.h>
#include <iostream>

using namespace Aevrynd::MeshCollisionGridSolid3D;
void Require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
std::vector<Cell> Cube(bool hole){
    std::vector<Cell> cells;
    for(int x=-2;x<=2;++x)for(int y=-2;y<=2;++y)for(int z=-2;z<=2;++z)
        if((std::abs(x)==2||std::abs(y)==2||std::abs(z)==2)&&!(hole&&x==2&&y==0&&z==0))cells.push_back({x,y,z});
    return cells;
}
void ValidatePacking(const std::vector<Cell>& cells){
    const auto boxes=PackExact(cells,cells.size());std::set<Cell> actual;
    for(const auto& box:boxes)for(int x=box.minimum[0];x<=box.maximum[0];++x)
        for(int y=box.minimum[1];y<=box.maximum[1];++y)for(int z=box.minimum[2];z<=box.maximum[2];++z)
            Require(actual.insert({x,y,z}).second,"packing duplicated occupancy");
    Require(actual==std::set<Cell>(cells.begin(),cells.end()),"packing added air or removed occupancy");
}
int main(){try{
    const auto closed=FillEnclosed(Cube(false),1000);
    Require(closed.surfaceCount==98&&closed.interiorCount==27&&closed.occupied.size()==125&&closed.domainCount==343,"closed cube fill");
    Require(PackExact(closed.occupied,1).size()==1,"filled cube compression");ValidatePacking(closed.occupied);
    const auto open=FillEnclosed(Cube(true),1000);
    Require(open.interiorCount==0&&open.occupied.size()==97,"open tunnel incorrectly filled");ValidatePacking(open.occupied);
    const std::vector<Cell> sparse{{-100,0,0},{-98,0,0},{-98,0,1},{-98,1,1}};
    Require(FillEnclosed(sparse,1000).interiorCount==0,"sparse fill");ValidatePacking(sparse);
    bool rejected=false;try{FillEnclosed(Cube(false),100);}catch(const std::invalid_argument&){rejected=true;}
    Require(rejected,"domain budget not rejected");rejected=false;
    const std::vector<Cell> extreme{{(std::numeric_limits<std::int32_t>::max)(),0,0}};
    try{FillEnclosed(extreme,100);}catch(const std::invalid_argument&){rejected=true;}Require(rejected,"padding overflow");
    rejected=false;try{PackExact(sparse,1);}catch(const std::invalid_argument&){rejected=true;}Require(rejected,"box budget");
    rejected=false;try{PackExact(sparse,4,0);}catch(const std::invalid_argument&){rejected=true;}Require(rejected,"packing work budget");
    Require(FillEnclosed({},0).occupied.empty(),"empty input");
    for(double spacing:{.125,.15,.2})for(int coordinate:{-2147483647,-1000,-553,-1,0,1,553,1000,2147483646}){
        const CellBox box{{coordinate,coordinate,coordinate},{coordinate,coordinate,coordinate}};
        const auto collider=ToCollider(box,spacing);
        Require(double(collider.center.x-collider.extents.x)<=double(coordinate)*spacing&&
            double(collider.center.x+collider.extents.x)>=(double(coordinate)+1)*spacing,"float endpoints shrink cell");
    }
    for(double invalid:{0.,-1.,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),2.}){
        rejected=false;try{ToCollider({{0,0,0},{0,0,0}},invalid);}catch(const std::invalid_argument&){rejected=true;}
        Require(rejected,"invalid conversion spacing");
    }
    rejected=false;try{ToCollider({{1,0,0},{0,0,0}},.15);}catch(const std::invalid_argument&){rejected=true;}
    Require(rejected,"invalid conversion grid box");
    std::cout<<"GRID_SOLID_3D_PASS; CLOSED_INTERIOR=27; OPEN_INTERIOR=0; EXACT_PACKING=3; FLOAT_ENCLOSURE_CONTROLS=27; HOSTILE_CONTROLS=10\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
