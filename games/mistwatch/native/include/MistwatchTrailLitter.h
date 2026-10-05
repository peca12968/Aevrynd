#pragma once
#include "MistwatchWorld.h"
#include <bit>
#include <span>
#include <limits>

namespace Mistwatch {
// 這是最高 4.5 公分的非碰撞地表碎物，不是另一批草或大型地物；原禁放門檻不變。
struct TrailLitterSettings {
    std::uint32_t seed{0x4c495454u};
    unsigned targetPatches{48}, maximumAttempts{2048};
    unsigned branchesPerPatch{5}, stonesPerPatch{7};
    unsigned maximumTriangles{32768};
    float minimumZ{-8.f}, maximumZ{26.f};
    float minimumAbsX{3.7f}, maximumAbsX{4.65f};
    float patchHalfX{.28f}, patchHalfZ{.36f};
};
struct TrailLitterFootprint {
    Aevrynd::Vec3 centre;
    float halfX{}, halfZ{};
    unsigned branchCount{}, stoneCount{};
};
struct TrailLitterResult {
    Aevrynd::Mesh3D branches, stones;
    std::vector<TrailLitterFootprint> patches;
    unsigned branchCount{}, stoneCount{}, rejectedClearance{}, rejectedBudget{};
    std::uint64_t fingerprint{14695981039346656037ull};
    bool reachedBudget{};
};
namespace TrailLitterDetail {
inline bool ValidSolid(const Aevrynd::BoxCollider3D& box) {
    return Session::Finite(box.center)&&Session::Finite(box.extents)&&
        box.extents.x>=0&&box.extents.y>=0&&box.extents.z>=0;
}
inline bool ClearsFootprint(const TrailLitterFootprint& p,
    std::span<const Aevrynd::BoxCollider3D> solids) {
    if(!Session::Finite(p.centre)||!std::isfinite(p.halfX)||!std::isfinite(p.halfZ)||
        p.halfX<=0||p.halfZ<=0)return false;
    const double x=p.centre.x,z=p.centre.z,rx=p.halfX,rz=p.halfZ;
    const double minX=x-rx,maxX=x+rx,minZ=z-rz,maxZ=z+rz;
    if(minZ<43.&&maxZ>29.)return false;
    // 可見主路邊界的連續保守界：中心導數 .0665，半寬導數 .111。
    // 不是鬆綁原草的 ±4.5 公尺門檻；薄碎物使用獨立的可見路面契約。
    const double centre=.35*std::sin(z*.19),width=2.8+.3*std::sin(z*.37);
    const double variation=.1775*rz+.08;
    if(!(minX>=centre+width+variation||maxX<=centre-width-variation))return false;
    const auto circle=[&](Aevrynd::Vec3 point,double radius) {
        const double dx=(std::max)(0.,std::abs(x-point.x)-rx);
        const double dz=(std::max)(0.,std::abs(z-point.z)-rz);
        return std::hypot(dx,dz)>=radius+.02;
    };
    const auto branch=[&](Aevrynd::Vec3 point,double zMargin,double xMargin) {
        return !(minZ<point.z+zMargin&&maxZ>point.z-zMargin&&
            minX<(std::max)(0.,double(point.x))+xMargin&&
            maxX>(std::min)(0.,double(point.x))-xMargin);
    };
    for(const auto point:Session::supplies)
        if(!circle(point,3.)||!branch(point,2.,2.))return false;
    for(const auto point:Session::repairSites)
        if(!circle(point,4.)||!branch(point,1.5,1.))return false;
    for(const auto point:Session::relaySites)
        if(!circle(point,4.)||!branch(point,1.8,1.))return false;
    for(const auto point:{Session::fuse,Session::generator,Session::rangerLog})
        if(!circle(point,5.)||!branch(point,1.15,0.))return false;
    if(!circle(Session::arena,7.)||!circle(Session::beacon,5.))return false;
    // 整個簇的河岸距離界，含中心線與河寬的最大導數；不只採樣四角。
    const double river=RiverCenterZ(p.centre.x);
    const double dryDistance=(std::max)(0.,std::abs(z-river)-rz-.77*rx);
    if(dryDistance<RiverOuterWidth(p.centre.x)+.75*rx+2.)return false;
    for(const auto& box:solids) {
        if(!ValidSolid(box))return false;
        if(minX<box.center.x+box.extents.x+.02&&maxX>box.center.x-box.extents.x-.02&&
            minZ<box.center.z+box.extents.z+.02&&maxZ>box.center.z-box.extents.z-.02)return false;
    }
    return true;
}
struct Point {double x{},z{},offset{};};
inline double Side(Point a,Point b,Point p) {return (b.x-a.x)*(p.z-a.z)-(b.z-a.z)*(p.x-a.x);}
inline std::vector<Point> Clip(std::vector<Point> polygon,Point a,Point b,double sign) {
    std::vector<Point> result;
    if(polygon.empty())return result;
    Point previous=polygon.back();double prev=sign*Side(a,b,previous);
    for(const auto current:polygon) {
        const double distance=sign*Side(a,b,current);
        if((prev>=0)!=(distance>=0)) {
            const double t=prev/(prev-distance);
            result.push_back({previous.x+(current.x-previous.x)*t,
                previous.z+(current.z-previous.z)*t,previous.offset+(current.offset-previous.offset)*t});
        }
        if(distance>=0)result.push_back(current);
        previous=current;prev=distance;
    }
    return result;
}
// 每片碎物三角形沿真地形格線／對角線切開，避免只貼頂點卻讓內部穿過坡面。
inline void Face(Aevrynd::Mesh3D& mesh,const Aevrynd::HeightField3D& terrain,
    std::array<Point,3> points,Aevrynd::Color color,unsigned maximumTriangles) {
    using namespace Aevrynd;
    const auto& grid=terrain.Settings();
    const double minX=(std::min)({points[0].x,points[1].x,points[2].x});
    const double maxX=(std::max)({points[0].x,points[1].x,points[2].x});
    const double minZ=(std::min)({points[0].z,points[1].z,points[2].z});
    const double maxZ=(std::max)({points[0].z,points[1].z,points[2].z});
    const int x0=(std::max)(0,int(std::floor((minX-grid.origin.x)/grid.cellSize))-1);
    const int x1=(std::min)(int(grid.width)-2,int(std::floor((maxX-grid.origin.x)/grid.cellSize))+1);
    const int z0=(std::max)(0,int(std::floor((minZ-grid.origin.z)/grid.cellSize))-1);
    const int z1=(std::min)(int(grid.depth)-2,int(std::floor((maxZ-grid.origin.z)/grid.cellSize))+1);
    for(int z=z0;z<=z1;++z)for(int x=x0;x<=x1;++x) {
        const auto aa=terrain.SamplePosition(unsigned(x),unsigned(z));
        const auto dd=terrain.SamplePosition(unsigned(x+1),unsigned(z+1));
        const Point a{aa.x,aa.z,0},b{dd.x,aa.z,0},c{aa.x,dd.z,0},d{dd.x,dd.z,0};
        for(const auto cell:std::array<std::array<Point,3>,2>{{{a,c,b},{b,c,d}}}) {
            std::vector<Point> polygon(points.begin(),points.end());
            for(unsigned edge=0;edge<3;++edge)polygon=Clip(std::move(polygon),cell[edge],cell[(edge+1)%3],-1.);
            for(std::size_t index=1;index+1<polygon.size();++index) {
                const std::array<Point,3> face{polygon[0],polygon[index],polygon[index+1]};
                std::array<Vec3,3> positions{};
                for(unsigned v=0;v<3;++v) {
                    const float px=float(face[v].x),pz=float(face[v].z),height=terrain.SampleHeight(px,pz);
                    if(!std::isfinite(height))throw std::invalid_argument("Invalid litter terrain support");
                    positions[v]={px,height+float(face[v].offset),pz};
                }
                const auto cross=Cross(positions[1]-positions[0],positions[2]-positions[0]);
                if(Length(cross)<1.e-10f)continue;
                if(mesh.indices.size()/3>=maximumTriangles)throw std::length_error("Litter triangle limit");
                const auto base=std::uint32_t(mesh.vertices.size()),normal=PackNormalSnorm8(Normalize(cross));
                for(const auto position:positions)mesh.vertices.push_back({position.x,position.y,position.z,normal,
                    PackColorRgba8(color),position.x,position.z});
                mesh.indices.insert(mesh.indices.end(),{base,base+1,base+2});
            }
        }
    }
}
inline void Twig(Aevrynd::Mesh3D& mesh,const Aevrynd::HeightField3D& terrain,
    double x,double z,double angle,double length,double radius,Aevrynd::Color color,unsigned limit) {
    constexpr double Pi=3.14159265358979323846;
    std::array<std::array<Point,6>,3> rings{};
    for(unsigned ring=0;ring<3;++ring) {
        const double t=double(ring)*.5,along=(t-.5)*length;
        const double bend=ring==1?length*.075:0.,r=radius*(1.-.55*t);
        for(unsigned side=0;side<6;++side) {
            const double phi=2.*Pi*double(side)/6.,lateral=bend+std::cos(phi)*r;
            rings[ring][side]={x+std::cos(angle)*along-std::sin(angle)*lateral,
                z+std::sin(angle)*along+std::cos(angle)*lateral,.003+r*(1.+std::sin(phi))};
        }
    }
    for(unsigned ring=0;ring<2;++ring)for(unsigned side=0;side<6;++side) {
        const auto next=(side+1)%6;
        Face(mesh,terrain,{rings[ring][side],rings[ring+1][side],rings[ring][next]},color,limit);
        Face(mesh,terrain,{rings[ring][next],rings[ring+1][side],rings[ring+1][next]},color,limit);
    }
    for(unsigned end=0;end<2;++end) {
        Point center{};const unsigned ring=end*2;
        for(const auto point:rings[ring]){center.x+=point.x/6.;center.z+=point.z/6.;center.offset+=point.offset/6.;}
        for(unsigned side=0;side<6;++side)
            Face(mesh,terrain,end?std::array<Point,3>{center,rings[ring][(side+1)%6],rings[ring][side]}:
                std::array<Point,3>{center,rings[ring][side],rings[ring][(side+1)%6]},color,limit);
    }
}
inline void Stone(Aevrynd::Mesh3D& mesh,const Aevrynd::HeightField3D& terrain,
    double x,double z,double angle,double radius,double height,Aevrynd::Color color,unsigned limit) {
    constexpr double Pi=3.14159265358979323846;
    std::array<Point,7> base{},rim{};
    for(unsigned side=0;side<7;++side) {
        const double phi=2.*Pi*double(side)/7.,r=radius*(.83+.17*std::sin(double(side)*2.31+angle));
        const double dx=std::cos(phi)*r,dz=std::sin(phi)*r*.73;
        const double px=x+std::cos(angle)*dx-std::sin(angle)*dz;
        const double pz=z+std::sin(angle)*dx+std::cos(angle)*dz;
        base[side]={px,pz,.0025};
        rim[side]={x+(px-x)*.75,z+(pz-z)*.75,height*(.48+.12*std::sin(double(side)+angle))};
    }
    const Point peak{x+radius*.13*std::cos(angle),z+radius*.13*std::sin(angle),height};
    const Point bottom{x,z,.0025};
    for(unsigned side=0;side<7;++side) {
        const auto next=(side+1)%7;
        Face(mesh,terrain,{base[side],rim[side],base[next]},color,limit);
        Face(mesh,terrain,{base[next],rim[side],rim[next]},color,limit);
        Face(mesh,terrain,{peak,rim[next],rim[side]},color,limit);
        Face(mesh,terrain,{bottom,base[side],base[next]},color,limit);
    }
}
inline void Append(Aevrynd::Mesh3D& destination,const Aevrynd::Mesh3D& source) {
    const auto base=std::uint32_t(destination.vertices.size());
    destination.vertices.insert(destination.vertices.end(),source.vertices.begin(),source.vertices.end());
    for(const auto index:source.indices)destination.indices.push_back(base+index);
}
}
inline TrailLitterResult CreateTrailLitter(const Aevrynd::HeightField3D& terrain,
    std::span<const Aevrynd::BoxCollider3D> solids,TrailLitterSettings settings={}) {
    using namespace Aevrynd;using namespace TrailLitterDetail;
    const auto& grid=terrain.Settings();
    if(settings.targetPatches>64||settings.maximumAttempts>4096||!settings.maximumAttempts||
        !settings.branchesPerPatch||settings.branchesPerPatch>8||!settings.stonesPerPatch||settings.stonesPerPatch>12||
        !settings.maximumTriangles||settings.maximumTriangles>32768||
        !std::isfinite(settings.minimumZ)||!std::isfinite(settings.maximumZ)||
        settings.minimumZ< -8||settings.maximumZ>26||settings.minimumZ>=settings.maximumZ||
        !std::isfinite(settings.minimumAbsX)||!std::isfinite(settings.maximumAbsX)||
        settings.minimumAbsX<3.5f||settings.maximumAbsX>4.8f||settings.minimumAbsX>=settings.maximumAbsX||
        !std::isfinite(settings.patchHalfX)||!std::isfinite(settings.patchHalfZ)||
        settings.patchHalfX<.28f||settings.patchHalfX>.4f||settings.patchHalfZ<.36f||settings.patchHalfZ>.45f||
        !Session::Finite(grid.origin)||!std::isfinite(grid.cellSize)||grid.cellSize<.1f||grid.cellSize>4.f||
        grid.width<2||grid.depth<2||terrain.SampleCount()>1000000||
        std::abs(grid.origin.x)>10000.f||std::abs(grid.origin.z)>10000.f)
        throw std::invalid_argument("Invalid trail litter settings");
    for(const auto& box:solids)if(!ValidSolid(box))throw std::invalid_argument("Invalid litter collider");
    for(unsigned z=0;z<grid.depth;++z)for(unsigned x=0;x<grid.width;++x)
        if(!std::isfinite(terrain.Height(x,z)))throw std::invalid_argument("Invalid litter terrain");
    TrailLitterResult result;std::uint32_t state=settings.seed;
    const auto random=[&](){state=state*1664525u+1013904223u;return double(state>>8)/16777216.;};
    for(unsigned attempt=0;attempt<settings.maximumAttempts&&result.patches.size()<settings.targetPatches;++attempt) {
        const double sign=attempt%2?1.:-1.;
        const float x=float(sign*(settings.minimumAbsX+(settings.maximumAbsX-settings.minimumAbsX)*random()));
        const float z=float(settings.minimumZ+(settings.maximumZ-settings.minimumZ)*random());
        TrailLitterFootprint patch{{x,0,z},settings.patchHalfX,settings.patchHalfZ,
            settings.branchesPerPatch,settings.stonesPerPatch};
        if(!terrain.Contains(x-patch.halfX,z-patch.halfZ)||!terrain.Contains(x+patch.halfX,z+patch.halfZ)||
            !ClearsFootprint(patch,solids)){++result.rejectedClearance;continue;}
        patch.centre.y=terrain.SampleHeight(x,z);
        bool overlaps=false;
        for(const auto& old:result.patches)
            overlaps|=std::abs(x-old.centre.x)<patch.halfX+old.halfX+.04f&&
                std::abs(z-old.centre.z)<patch.halfZ+old.halfZ+.04f;
        if(overlaps){++result.rejectedClearance;continue;}
        Mesh3D branches,stones;
        try {
            for(unsigned index=0;index<settings.branchesPerPatch;++index) {
                const double px=x+(random()-.5)*.16,pz=z+(random()-.5)*.28;
                const double angle=random()*6.283185307179586,length=.16+.17*random(),radius=.005+.008*random();
                const float shade=float(.65+.35*random());
                Twig(branches,terrain,px,pz,angle,length,radius,{shade,shade*.94f,shade*.82f,1},settings.maximumTriangles);
            }
            for(unsigned index=0;index<settings.stonesPerPatch;++index) {
                const double px=x+(random()-.5)*.39,pz=z+(random()-.5)*.56;
                const float shade=float(.58+.42*random());
                Stone(stones,terrain,px,pz,random()*6.283185307179586,.018+.025*random(),.015+.03*random(),
                    {shade,shade*.97f,shade*.91f,1},settings.maximumTriangles);
            }
        } catch(const std::length_error&) {++result.rejectedBudget;continue;}
        if((result.branches.indices.size()+result.stones.indices.size()+branches.indices.size()+stones.indices.size())/3>
            settings.maximumTriangles){++result.rejectedBudget;continue;}
        if(!branches.Valid()||!stones.Valid())throw std::runtime_error("Invalid trail litter geometry");
        // 生成幾何的實際完整投影必須仍在已通過的整簇 AABB 中。
        for(const auto* mesh:{&branches,&stones})for(const auto& vertex:mesh->vertices)
            if(std::abs(vertex.x-x)>patch.halfX||std::abs(vertex.z-z)>patch.halfZ)
                throw std::runtime_error("Litter geometry outside authored footprint");
        Append(result.branches,branches);Append(result.stones,stones);result.patches.push_back(patch);
        result.branchCount+=settings.branchesPerPatch;result.stoneCount+=settings.stonesPerPatch;
    }
    result.reachedBudget=result.patches.size()==settings.targetPatches;
    const auto mix=[&](std::uint32_t bits){for(unsigned byte=0;byte<4;++byte){result.fingerprint^=(bits>>(byte*8))&255u;result.fingerprint*=1099511628211ull;}};
    for(const auto* mesh:{&result.branches,&result.stones}) {
        mix(std::uint32_t(mesh->vertices.size()));mix(std::uint32_t(mesh->indices.size()));
        for(const auto& vertex:mesh->vertices){for(float value:{vertex.x,vertex.y,vertex.z,vertex.u,vertex.v})mix(std::bit_cast<std::uint32_t>(value));mix(vertex.normal);mix(vertex.color);}
        for(const auto index:mesh->indices)mix(index);
    }
    return result;
}
}
