#include <Aevrynd/EnvironmentMeshes3D.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string_view>
#include <vector>
using namespace Aevrynd;
namespace {
Vec3 Point(const Vertex3D& v){return {v.x,v.y,v.z};}
Vec3 Center(const Mesh3D& m,unsigned blade,unsigned ring,unsigned segments){
    const auto i=blade*(segments+1)*2+ring*2;return (Point(m.vertices[i])+Point(m.vertices[i+1]))*.5f;
}
double Area(const Mesh3D& m){
    double area{};for(std::size_t i=0;i<m.indices.size();i+=3){
        const auto a=Point(m.vertices[m.indices[i]]),b=Point(m.vertices[m.indices[i+1]]),c=Point(m.vertices[m.indices[i+2]]);
        area+=Length(Cross(b-a,c-a))*.5;
    }return area;
}
float Variation(unsigned seed,unsigned index){
    std::uint32_t v=seed*0x9e3779b9u+index*0x85ebca6bu+0x632be59bu;
    v^=v>>16;v*=0x7feb352du;v^=v>>15;return float(v&0xffffu)/65535.f;
}
unsigned Coverage(const Mesh3D& m,unsigned view){
    // 五個固定正交投影以相同格距估算三角形聯集；不把重疊的葉面重複計數。
    constexpr unsigned resolution=256;constexpr float step=2.4f/resolution;
    std::vector<bool> occupied(resolution*resolution);
    const float yaw=float(view)*.78539816339f;
    const auto project=[&](std::uint32_t index){const auto p=Point(m.vertices[index]);
        return Vec3{view==4?p.x:p.x*std::cos(yaw)+p.z*std::sin(yaw),view==4?p.z:p.y,0};};
    const auto edge=[](Vec3 a,Vec3 b,Vec3 p){return (b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x);};
    for(std::size_t i=0;i<m.indices.size();i+=3){
        const auto a=project(m.indices[i]),b=project(m.indices[i+1]),c=project(m.indices[i+2]);
        if(std::abs(edge(a,b,c))<.00000001f)continue;
        const auto cell=[](float value){return std::clamp(int(std::floor((value+1.2f)/step)),0,int(resolution)-1);};
        const int x0=cell((std::min)({a.x,b.x,c.x})),x1=cell((std::max)({a.x,b.x,c.x}));
        const int y0=cell((std::min)({a.y,b.y,c.y})),y1=cell((std::max)({a.y,b.y,c.y}));
        for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){
            const Vec3 p{-1.2f+(float(x)+.5f)*step,-1.2f+(float(y)+.5f)*step,0};
            const float e0=edge(a,b,p),e1=edge(b,c,p),e2=edge(c,a,p);
            if((e0>=0&&e1>=0&&e2>=0)||(e0<=0&&e1<=0&&e2<=0))occupied[std::size_t(y)*resolution+x]=true;
        }
    }
    return unsigned(std::count(occupied.begin(),occupied.end(),true));
}
bool Profile(const Mesh3D& m,const Mesh3D& old,unsigned blades,unsigned segments){
    if(!m.Valid()||m.vertices.size()!=blades*(segments+1)*2||m.indices!=old.indices)return false;
    unsigned bent{};double length{},oldLength{};float rootWidthRatio{};
    for(unsigned blade=0;blade<blades;++blade){
        const auto root=Center(m,blade,0,segments),tip=Center(m,blade,segments,segments);
        // 左右葉寬原本不對稱；縮窄後截面中點最多移動 4.2 mm，名義根點與地面高度仍不變。
        if(Length(root-Center(old,blade,0,segments))>.0042f||root.y!=0||tip.y<.6f)return false;
        const auto tangent=tip-Center(m,blade,segments-1,segments);
        if(std::hypot(tangent.x,tangent.z)>tangent.y*.65f)++bent;
        for(unsigned ring=0;ring<=segments;++ring){
            const auto i=blade*(segments+1)*2+ring*2;
            for(unsigned side=0;side<2;++side){
                const auto& v=m.vertices[i+side];const auto& o=old.vertices[i+side];const auto n=UnpackNormalSnorm8(v.normal);
                if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||v.y<0||
                    std::abs(Length({n.x,n.y,n.z})-1.f)>.02f||
                    (v.normal&0xff000000u)!=(o.normal&0xff000000u)||v.color!=o.color||v.u!=o.u||v.v!=o.v)return false;
                const auto across=Point(m.vertices[i+1])-Point(m.vertices[i]);
                if(std::abs(Dot(Normalize(across),{n.x,n.y,n.z}))>.02f)return false;
                const float t=float(ring)/segments;
                const auto right=Normalize(across),forward=Cross(right,{0,1,0});
                const float height=.61f+.43f*Variation(73,blade*5+2),lean=.16f+.18f*Variation(73,blade*5+5);
                const float sideBias=(Variation(73,blade*5+4)-.5f)*.18f;
                const auto expectedTangent=Normalize(Vec3{0,height*(1.f+.32f*3.14159265359f*std::cos(t*3.14159265359f)),0}+
                    forward*(2.f*lean*t)+right*(sideBias*3.14159265359f*std::cos(t*3.14159265359f)));
                if(std::abs(Dot(expectedTangent,{n.x,n.y,n.z}))>.02f||Dot(Normalize(Cross(right,expectedTangent)),{n.x,n.y,n.z})<.98f)return false;
            }
            if(!ring)rootWidthRatio=(std::max)(rootWidthRatio,Length(Point(m.vertices[i+1])-Point(m.vertices[i]))/Length(Point(old.vertices[i+1])-Point(old.vertices[i])));
            else{length+=Length(Center(m,blade,ring,segments)-Center(m,blade,ring-1,segments));oldLength+=Length(Center(old,blade,ring,segments)-Center(old,blade,ring-1,segments));}
        }
    }
    for(std::size_t i=0;i<m.indices.size();i+=3){
        const auto a=Point(m.vertices[m.indices[i]]),b=Point(m.vertices[m.indices[i+1]]),c=Point(m.vertices[m.indices[i+2]]);
        if(Length(Cross(b-a,c-a))<.000001f)return false;
    }
    const auto a=m.Bounds(),b=old.Bounds();const auto areaRatio=Area(m)/Area(old);
    std::cout<<"blades="<<blades<<" segments="<<segments<<" vertices="<<m.vertices.size()<<" triangles="<<m.indices.size()/3
        <<" root_width_ratio="<<rootWidthRatio<<" area_ratio="<<areaRatio<<" arc_ratio="<<length/oldLength<<" leaning_tips="<<bent<<'\n';
    for(unsigned view=0;view<5;++view){const auto oldCoverage=Coverage(old,view),coverage=Coverage(m,view);
        const double ratio=double(coverage)/oldCoverage;std::cout<<"coverage_view="<<view<<" old="<<oldCoverage<<" curved="<<coverage<<" ratio="<<ratio<<'\n';
        if(ratio<.9)return false;
    }
    std::cout<<"footprint_extents_delta="<<a.extents.x-b.extents.x<<','<<a.extents.z-b.extents.z<<'\n';
    // 葉片造型必須縮窄根部並彎曲，但不能以縮小整叢或刪除葉面來冒充改善。
    return rootWidthRatio<.7f&&areaRatio>=.8&&areaRatio<=1.15&&length/oldLength>=.95&&length/oldLength<=1.2&&
        a.extents.y>=b.extents.y*.95f&&a.extents.x>=b.extents.x*.85f&&a.extents.z>=b.extents.z*.85f&&
        a.extents.x<=b.extents.x*1.25f&&a.extents.z<=b.extents.z*1.25f&&bent>=blades/2;
}
}
int main(int argc,char** argv){
    const bool baseline=argc==2&&std::string_view(argv[1])=="--baseline";
    for(const unsigned blades:{4u,9u,11u})for(const unsigned segments:{2u,3u,5u}){
        const auto old=CreateGrassClumpMesh3D(blades,segments);
        const auto mesh=CreateGrassClumpMesh3D(blades,segments,baseline?GrassBladeStyle3D::Upright:GrassBladeStyle3D::Curved);
        if(!Profile(mesh,old,blades,segments)){std::cerr<<"FIRST_DIVERGENCE: grass width/curve/coverage contract\n";return 1;}
        const auto repeat=CreateGrassClumpMesh3D(blades,segments,GrassBladeStyle3D::Curved);
        for(std::size_t i=0;i<mesh.vertices.size();++i){const auto& a=mesh.vertices[i];const auto& b=repeat.vertices[i];
            if(a.x!=b.x||a.y!=b.y||a.z!=b.z||a.normal!=b.normal||a.color!=b.color)return 2;}
        auto vanished=mesh;for(auto& v:vanished.vertices){v.x*=.01f;v.y*=.01f;v.z*=.01f;}
        if(Profile(vanished,old,blades,segments))return 3;
        auto wind=mesh;wind.vertices[(segments+1)*2-1].normal^=0x10000000u;
        if(Profile(wind,old,blades,segments))return 4;
        auto wrongNormal=mesh;wrongNormal.vertices[2].normal=PackNormalSnorm8({0,1,0},UnpackNormalSnorm8(wrongNormal.vertices[2].normal).w);
        if(Profile(wrongNormal,old,blades,segments))return 5;
    }
    const auto production=CreateGrassClumpMesh3D(9,5,GrassBladeStyle3D::Curved),legacy=CreateGrassClumpMesh3D(9,3);
    if(production.vertices.size()!=108||production.indices.size()/3!=180)return 6;
    std::cout<<"production_vs_legacy_area_ratio="<<Area(production)/Area(legacy)<<'\n';
    for(unsigned view=0;view<5;++view){const double ratio=double(Coverage(production,view))/Coverage(legacy,view);
        std::cout<<"production_vs_legacy_coverage_view="<<view<<" ratio="<<ratio<<'\n';if(ratio<.9)return 7;}
    std::cout<<"Curved grass: 9 parameter combinations; shrink/wind hostile fixtures rejected\n";
}
