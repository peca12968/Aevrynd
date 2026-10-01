#include <Aevrynd/WaterMesh3D.h>
#include <iostream>
#include <limits>
int main() {
    using namespace Aevrynd;
    for(unsigned mask=0;mask<16;++mask) {
        HeightField3D terrain({{0,0,0},2,2,1});
        for(unsigned index=0;index<4;++index)terrain.SetHeight(index%2,index/2,(mask&(1u<<index))?-1.f:1.f);
        const auto water=CreateHeightFieldWaterMesh3D(terrain,0);
        const auto repeat=CreateHeightFieldWaterMesh3D(terrain,0);
        if(water.indices!=repeat.indices || water.vertices.size()!=repeat.vertices.size())return 1;
        double area=0;
        for(std::size_t offset=0;offset<water.indices.size();offset+=3) {
            const auto p=[&](unsigned index){const auto& v=water.vertices[index];return Vec3{v.x,v.y,v.z};};
            const auto a=p(water.indices[offset]),b=p(water.indices[offset+1]),c=p(water.indices[offset+2]);
            const float faceArea=Cross(b-a,c-a).y*.5f;
            if(faceArea<=0 || a.y!=0 || b.y!=0 || c.y!=0)return 2;
            area+=faceArea;
            for(unsigned u=0;u<=8;++u)for(unsigned v=0;v<=8-u;++v) {
                const auto point=a+(b-a)*(float(u)/8.f)+(c-a)*(float(v)/8.f);
                if(terrain.SampleHeight(point.x,point.z)>.00001f)return 3;
            }
        }
        // 高低點均為正負一時，一濕點面積為四分之一，兩濕點為四分之三。
        const auto triangleArea=[&](unsigned a,unsigned b,unsigned c) {
            const unsigned wet=unsigned(bool(mask&(1u<<a)))+unsigned(bool(mask&(1u<<b)))+unsigned(bool(mask&(1u<<c)));
            constexpr double ratio[]{0,.25,.75,1};return ratio[wet]*.5;
        };
        const double expected=triangleArea(0,2,1)+triangleArea(1,2,3);
        if(std::abs(area-expected)>.00001) {
            std::cerr<<"FIRST_DIVERGENCE: clipped water area; mask="<<mask<<" area="<<area<<" expected="<<expected<<'\n';return 4;
        }
        for(std::size_t index=0;index<water.vertices.size();++index) {
            const auto& a=water.vertices[index];const auto& b=repeat.vertices[index];
            if(a.x!=b.x || a.y!=b.y || a.z!=b.z || a.u!=b.u || a.v!=b.v)return 5;
            if(a.u!=a.x*.25f || a.v!=a.z*.25f)return 6;
        }
    }
    HeightField3D flat({{0,0,0},2,2,1});
    if(CreateHeightFieldWaterMesh3D(flat,0).Valid())return 7;
    flat.SetHeight(0,0,-1);
    const auto touches=CreateHeightFieldWaterMesh3D(flat,0);
    if(!touches.Valid() || touches.indices.size()!=3)return 8;
    const auto nan=std::numeric_limits<float>::quiet_NaN();
    if(CreateHeightFieldWaterMesh3D(flat,nan).Valid() || CreateHeightFieldWaterMesh3D(flat,0,nan).Valid() ||
       CreateHeightFieldWaterMesh3D(flat,0,0).Valid() || CreateHeightFieldWaterMesh3D(flat,0,-1).Valid())return 9;
    std::cout<<"Water mesh PASS; 16 wet/dry corner patterns, exact areas, shared terrain diagonal, deterministic UV, dry/touching/invalid inputs\n";
}
