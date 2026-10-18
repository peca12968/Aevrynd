#include "MistwatchRepairSignVisual.h"
#include "MistwatchWorld.h"
#include <iostream>
#include <map>
#include <set>
#include <vector>

namespace SignSmoke {
using namespace Aevrynd;
using Product=Mistwatch::RepairSign::Geometry;
unsigned checks{},negatives{};
void Check(bool ok,const char* text){++checks;if(!ok)throw std::runtime_error(text);}
bool Bits(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool VectorBits(Vec3 a,Vec3 b){return Bits(a.x,b.x)&&Bits(a.y,b.y)&&Bits(a.z,b.z);}
using Key=std::array<std::uint32_t,3>;
Key PositionKey(Vec3 p){return {p.x==0?0u:std::bit_cast<std::uint32_t>(p.x),p.y==0?0u:std::bit_cast<std::uint32_t>(p.y),p.z==0?0u:std::bit_cast<std::uint32_t>(p.z)};}
Vec3 Position(const Vertex3D& p){return {p.x,p.y,p.z};}
struct D {double x{},y{},z{};};
D Point(Vec3 p){return {p.x,p.y,p.z};}
D Sub(D a,D b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
D CrossD(D a,D b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
double Scalar(D a,D b){return a.x*b.x+a.y*b.y+a.z*b.z;}
struct Hull {std::vector<D> points;std::vector<std::array<unsigned,3>> faces;std::set<std::pair<unsigned,unsigned>> edges;};
Hull Extract(const Mesh3D& mesh,unsigned firstVertex,unsigned vertexCount,unsigned firstIndex,unsigned indexCount){
    Hull result;std::map<Key,unsigned> weld;std::vector<unsigned> aliases(vertexCount);
    for(unsigned i=0;i<vertexCount;++i){const auto p=Position(mesh.vertices[firstVertex+i]);const auto key=PositionKey(p);
        const auto [it,inserted]=weld.emplace(key,static_cast<unsigned>(result.points.size()));
        if(inserted)result.points.push_back(Point(p));aliases[i]=it->second;}
    for(unsigned i=firstIndex;i<firstIndex+indexCount;i+=3){std::array<unsigned,3> f{};
        for(unsigned k=0;k<3;++k){const auto raw=mesh.indices[i+k];if(raw<firstVertex||raw>=firstVertex+vertexCount)throw std::runtime_error("cross-part index");f[k]=aliases[raw-firstVertex];}
        result.faces.push_back(f);for(unsigned k=0;k<3;++k){const auto a=f[k],b=f[(k+1)%3];result.edges.emplace((std::min)(a,b),(std::max)(a,b));}}
    return result;
}
bool Closed(const Hull& h){
    std::map<std::pair<unsigned,unsigned>,std::pair<unsigned,int>> edge;
    double volume=0;const auto origin=h.points.front();
    for(const auto& f:h.faces){const auto a=h.points[f[0]],b=h.points[f[1]],c=h.points[f[2]];
        if(Scalar(CrossD(Sub(b,a),Sub(c,a)),CrossD(Sub(b,a),Sub(c,a)))==0)return false;
        volume+=Scalar(Sub(a,origin),CrossD(Sub(b,origin),Sub(c,origin)));
        for(unsigned k=0;k<3;++k){const auto aId=f[k],bId=f[(k+1)%3];auto& e=edge[{(std::min)(aId,bId),(std::max)(aId,bId)}];++e.first;e.second+=aId<bId?1:-1;}}
    for(const auto& [unused,e]:edge){(void)unused;if(e.first!=2||e.second!=0)return false;}
    return std::isfinite(volume)&&volume>0&&static_cast<long long>(h.points.size())-static_cast<long long>(edge.size())+static_cast<long long>(h.faces.size())==2;
}
// 已知來源primitive皆為凸封閉體。完整凸SAT：真面法線＋兩體全部真edge叉積，不將AABB重疊當穿模。
// 投影區間等端點只代表零體積接觸；無epsilon，也不拿有交叉的零梯度當separating axis。
bool InteriorOverlap(const Hull& a,const Hull& b){
    const auto separate=[&](D axis){if(Scalar(axis,axis)==0)return false;
        double loA=INFINITY,hiA=-INFINITY,loB=INFINITY,hiB=-INFINITY;
        for(const auto p:a.points){const auto t=Scalar(axis,p);loA=(std::min)(loA,t);hiA=(std::max)(hiA,t);}
        for(const auto p:b.points){const auto t=Scalar(axis,p);loB=(std::min)(loB,t);hiB=(std::max)(hiB,t);}
        return hiA<=loB||hiB<=loA;};
    for(const D axis:std::array<D,3>{{{1,0,0},{0,1,0},{0,0,1}}})if(separate(axis))return false;
    for(const auto& f:a.faces)if(separate(CrossD(Sub(a.points[f[1]],a.points[f[0]]),Sub(a.points[f[2]],a.points[f[0]]))))return false;
    for(const auto& f:b.faces)if(separate(CrossD(Sub(b.points[f[1]],b.points[f[0]]),Sub(b.points[f[2]],b.points[f[0]]))))return false;
    for(const auto& ea:a.edges)for(const auto& eb:b.edges)
        if(separate(CrossD(Sub(a.points[ea.second],a.points[ea.first]),Sub(b.points[eb.second],b.points[eb.first]))))return false;
    return true;
}
double PlaneArea(const Hull& h,double z,int outwardSign){
    double area=0;
    for(const auto& f:h.faces){const auto a=h.points[f[0]],b=h.points[f[1]],c=h.points[f[2]];
        if(a.z==z&&b.z==z&&c.z==z){const auto n=CrossD(Sub(b,a),Sub(c,a));if(n.z*outwardSign>0)area+=std::abs(n.z)*.5;}}
    return area;
}
// 獨立來源合同，不讀候選Source／UV helpers，也不用首次輸出回填golden。
struct ExpectedPart {unsigned group;bool head;Vec3 center,size;float bevel;};
constexpr std::array<ExpectedPart,6> Expected{{
    {0,false,{0,.6f,9.f/256.f},{.1f,1.2f,5.f/128.f},.004f},
    {0,false,{0,1.036f,-1.f/256.f},{.68f,.16f,5.f/128.f},.003f},
    {0,false,{0,1.204f,-1.f/256.f},{.68f,.16f,5.f/128.f},.003f},
    {1,false,{0,1.204f,-13.f/512.f},{.26f,.106f,1.f/256.f},.001f},
    {2,true,{-.085f,1.204f,-1.f/32.f},{.02f,.02f,1.f/128.f},0},
    {2,true,{ .085f,1.204f,-1.f/32.f},{.02f,.02f,1.f/128.f},0}
}};
std::set<Key> ExpectedPositions(const ExpectedPart& s){
    std::set<Key> out;
    if(!s.head){const std::array<float,3> half{s.size.x*.5f,s.size.y*.5f,s.size.z*.5f};
        for(const float x:{-1.f,1.f})for(const float y:{-1.f,1.f})for(const float z:{-1.f,1.f})for(unsigned full=0;full<3;++full){
            const std::array<float,3> sign{x,y,z};std::array<float,3> p{};
            for(unsigned a=0;a<3;++a)p[a]=sign[a]*(half[a]-(a==full?0.f:s.bevel));
            out.insert(PositionKey({s.center.x+p[0],s.center.y+p[1],s.center.z+p[2]}));}
    }else{constexpr float pi=3.14159265358979323846f;
        for(unsigned i=0;i<12;++i){const float angle=2.f*pi*float(i)/12.f;
            for(const float end:{-.5f,.5f})out.insert(PositionKey({s.center.x+(std::sin(angle)*.5f)*s.size.x,
                s.center.y-(std::cos(angle)*.5f)*s.size.y,s.center.z+end*s.size.z}));}
        for(const float end:{-.5f,.5f})out.insert(PositionKey({s.center.x,s.center.y,s.center.z+end*s.size.z}));}
    return out;
}
bool Validate(const Product& product){
    constexpr std::array<unsigned,3> nv{288,96,100},ni{396,132,288};
    for(unsigned g=0;g<3;++g)if(!product.meshes[g].Valid()||product.meshes[g].vertices.size()!=nv[g]||product.meshes[g].indices.size()!=ni[g])return false;
    std::array<unsigned,3> vertexCursor{},indexCursor{};std::array<Hull,6> hulls;
    for(unsigned i=0;i<6;++i){const auto& s=Expected[i];const auto& r=product.parts[i];const auto& mesh=product.meshes[s.group];
        const unsigned count=s.head?50u:96u,indices=s.head?144u:132u;
        if(r.group!=s.group||r.firstVertex!=vertexCursor[s.group]||r.firstIndex!=indexCursor[s.group]||r.vertexCount!=count||r.indexCount!=indices)return false;
        vertexCursor[s.group]+=count;indexCursor[s.group]+=indices;
        std::set<Key> actual;Vec3 lo{INFINITY,INFINITY,INFINITY},hi{-INFINITY,-INFINITY,-INFINITY};
        for(unsigned k=0;k<count;++k){const auto& p=mesh.vertices[r.firstVertex+k];
            if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||!std::isfinite(p.u)||!std::isfinite(p.v))return false;
            actual.insert(PositionKey(Position(p)));lo.x=(std::min)(lo.x,p.x);hi.x=(std::max)(hi.x,p.x);
            lo.y=(std::min)(lo.y,p.y);hi.y=(std::max)(hi.y,p.y);lo.z=(std::min)(lo.z,p.z);hi.z=(std::max)(hi.z,p.z);
            // 每個凸part完整留在原單一盒，不用只查頂點落在聯集來掩飾跨出三角形。
            if(i==0){if(p.x<-.06f||p.x>.06f||p.y<0||p.y>1.2f||p.z<-.06f||p.z>.06f)return false;}
            else if(p.x<-.35f||p.x>.35f||p.y<.95f||p.y>1.29f||p.z<-.05f||p.z>.05f)return false;
            const auto n=UnpackNormalSnorm8(p.normal);const float length=std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);
            if(!std::isfinite(length)||std::abs(length-1.f)>.02f||n.w!=1.f)return false;
            const auto color=PackColorRgba8(s.group==0?Color{1,1,1,1}:s.group==1?Color{.78f,.58f,.15f,1}:Color{.62f,.65f,.68f,1});
            if(p.color!=color)return false;
            if(s.head){constexpr float pi=3.14159265358979323846f;Vec3 desired{};
                if(k<24){const float angle=2.f*pi*float(k/2u)/12.f;desired={std::sin(angle),-std::cos(angle),0};}
                else desired=k<37?Vec3{0,0,-1}:Vec3{0,0,1};
                if(p.normal!=PackNormalSnorm8(desired))return false;}
            if(s.group!=0&&(!Bits(p.u,0.f)||!Bits(p.v,0.f)))return false;
        }
        if(actual!=ExpectedPositions(s))return false;
        if(s.group==0)for(unsigned k=0;k<count;++k){const auto& p=mesh.vertices[r.firstVertex+k];const auto n=UnpackNormalSnorm8(p.normal);
            const float nx=std::abs(n.x),ny=std::abs(n.y),nz=std::abs(n.z);float distance{},extent{},q{};
            if(i==0){if(ny>=nx&&ny>=nz){distance=p.x-lo.x;extent=hi.x-lo.x;q=(p.z-lo.z)/(hi.z-lo.z);}
                else{distance=p.y-lo.y;extent=hi.y-lo.y;q=nx>=nz?(p.z-lo.z)/(hi.z-lo.z):(p.x-lo.x)/(hi.x-lo.x);}}
            else{if(nx>=ny&&nx>=nz){distance=p.z-lo.z;extent=hi.z-lo.z;q=(p.y-lo.y)/(hi.y-lo.y);}
                else{distance=p.x-lo.x;extent=hi.x-lo.x;q=ny>=nz?(p.z-lo.z)/(hi.z-lo.z):(p.y-lo.y)/(hi.y-lo.y);}}
            const float phase=.1f+.13f*float(i),scale=2048.f*(16.f/32767.f),u=(phase+distance*.5f)/scale,v=(.025f+.05f*q)/scale;
            if(!std::isfinite(q)||q<0||q>1||!Bits(p.u,u)||!Bits(p.v,v)||p.u<phase/scale||p.u>(phase+extent*.5f)/scale||
               p.v<.025f/scale||p.v>(.025f+.05f)/scale)return false;}
        std::vector<bool> referenced(count,false);
        for(unsigned j=r.firstIndex;j<r.firstIndex+indices;j+=3){
            for(unsigned k=0;k<3;++k){const auto index=mesh.indices[j+k];if(index<r.firstVertex||index>=r.firstVertex+count)return false;referenced[index-r.firstVertex]=true;}
            const auto& a=mesh.vertices[mesh.indices[j]];const auto& b=mesh.vertices[mesh.indices[j+1]];const auto& c=mesh.vertices[mesh.indices[j+2]];
            Vec3 normal{};if(!TryComputeTriangleNormal(Position(a),Position(b),Position(c),normal))return false;
            for(unsigned k=0;k<3;++k){const auto& p=mesh.vertices[mesh.indices[j+k]];const auto n=UnpackNormalSnorm8(p.normal);
                const float bound=s.head?float(std::cos(3.14159265358979323846/12.0)-std::sqrt(3.0)/(2.0*127.0)):.99f;
                if(Dot(normal,{n.x,n.y,n.z})<=bound)return false;
                if(!s.head&&p.normal!=PackNormalSnorm8(normal))return false;}
            if(s.group==0){const double jac=(double(b.u)-a.u)*(double(c.v)-a.v)-(double(c.u)-a.u)*(double(b.v)-a.v);if(!std::isfinite(jac)||jac==0)return false;}
        }
        if(std::find(referenced.begin(),referenced.end(),false)!=referenced.end())return false;
        hulls[i]=Extract(mesh,r.firstVertex,count,r.firstIndex,indices);if(!Closed(hulls[i]))return false;
    }
    if(vertexCursor!=nv||indexCursor!=ni)return false;
    for(unsigned a=0;a<6;++a)for(unsigned b=a+1;b<6;++b)if(InteriorOverlap(hulls[a],hulls[b]))return false;
    constexpr std::array<std::array<unsigned,2>,5> contact{{{0,1},{0,2},{2,3},{3,4},{3,5}}};
    constexpr std::array<double,5> plane{1.0/64,1.0/64,-3.0/128,-7.0/256,-7.0/256};
    constexpr std::array<int,5> normalA{-1,-1,-1,-1,-1};
    for(unsigned i=0;i<5;++i)if(PlaneArea(hulls[contact[i][0]],plane[i],normalA[i])<=0||PlaneArea(hulls[contact[i][1]],plane[i],-normalA[i])<=0)return false;
    return true;
}
bool SameMaterial(const Material3D& a,const Material3D& b){
    return Bits(a.tint.r,b.tint.r)&&Bits(a.tint.g,b.tint.g)&&Bits(a.tint.b,b.tint.b)&&Bits(a.tint.a,b.tint.a)&&
        Bits(a.surface.roughness,b.surface.roughness)&&Bits(a.surface.metallic,b.surface.metallic)&&Bits(a.surface.emissive,b.surface.emissive)&&Bits(a.surface.occlusion,b.surface.occlusion)&&
        a.castsShadow==b.castsShadow&&a.textures.value==b.textures.value&&a.textures.semanticIdentity==b.textures.semanticIdentity&&Bits(a.textureScale,b.textureScale)&&a.useUv==b.useUv&&
        a.materialClass==b.materialClass&&a.shadingModel==b.shadingModel&&a.alphaMode==b.alphaMode&&Bits(a.alphaCutoff,b.alphaCutoff)&&a.doubleSided==b.doubleSided&&
        Bits(a.normalScale,b.normalScale)&&Bits(a.baseTextureScale,b.baseTextureScale)&&Bits(a.detailTextureScale,b.detailTextureScale);
}
template<class Change>void Reject(const Product& a,Change change,const char* name){auto bad=a;change(bad);++negatives;Check(!Validate(bad),name);}
}

int main(){try{
    using namespace SignSmoke;
    const auto before=Mistwatch::RepairShelterSolids();const auto a=Mistwatch::RepairSign::Create(),b=Mistwatch::RepairSign::Create();
    Check(Validate(a),"complete source geometry UV winding closed convex parts and contact");
    unsigned vertices=0,triangles=0;
    for(unsigned g=0;g<3;++g){const auto& x=a.meshes[g];const auto& y=b.meshes[g];vertices+=static_cast<unsigned>(x.vertices.size());triangles+=static_cast<unsigned>(x.indices.size()/3u);
        Check(x.indices==y.indices,"repeat indices");for(std::size_t i=0;i<x.vertices.size();++i){const auto& p=x.vertices[i];const auto& q=y.vertices[i];
            Check(Bits(p.x,q.x)&&Bits(p.y,q.y)&&Bits(p.z,q.z)&&p.normal==q.normal&&p.color==q.color&&Bits(p.u,q.u)&&Bits(p.v,q.v),"repeat all vertex bits");}}
    Check(vertices==484&&triangles==272&&vertices<=512&&triangles<=512,"fixed finite budget");
    for(unsigned i=0;i<6;++i){const auto& r=a.parts[i];Vec3 lo{INFINITY,INFINITY,INFINITY},hi{-INFINITY,-INFINITY,-INFINITY};
        for(unsigned k=0;k<r.vertexCount;++k){const auto& p=a.meshes[r.group].vertices[r.firstVertex+k];
            lo.x=(std::min)(lo.x,p.x);hi.x=(std::max)(hi.x,p.x);lo.y=(std::min)(lo.y,p.y);hi.y=(std::max)(hi.y,p.y);lo.z=(std::min)(lo.z,p.z);hi.z=(std::max)(hi.z,p.z);}
        const Vec3 oldLo=i==0?Vec3{-.06f,0,-.06f}:Vec3{-.35f,.95f,-.05f},oldHi=i==0?Vec3{.06f,1.2f,.06f}:Vec3{.35f,1.29f,.05f};
        const float margin=(std::min)({lo.x-oldLo.x,oldHi.x-hi.x,lo.y-oldLo.y,oldHi.y-hi.y,lo.z-oldLo.z,oldHi.z-hi.z});
        std::cout<<"REPAIR_SIGN_PART part="<<i<<" group="<<r.group<<" vertices="<<r.vertexCount<<" indices="<<r.indexCount
                 <<" low="<<lo.x<<','<<lo.y<<','<<lo.z<<" high="<<hi.x<<','<<hi.y<<','<<hi.z<<" minimum_envelope_margin="<<margin<<'\n';}
    const auto after=Mistwatch::RepairShelterSolids();Check(before.size()==5&&after.size()==5,"original5 collider membership");
    for(unsigned i=0;i<5;++i){const auto& x=before[i];const auto& y=after[i];Check(VectorBits(x.center,y.center)&&VectorBits(x.extents,y.extents),"collider fields unchanged");}
    for(unsigned site=0;site<3;++site){const auto expected=Mistwatch::Session::repairSites[site]+Vec3{site==0?2.8f:1.3f,0,0};
        Check(VectorBits(Mistwatch::RepairSign::Anchor(site),expected),"original sign3 anchors");
        for(unsigned mask=0;mask<8;++mask)Check(Mistwatch::RepairSign::Visible(mask,site)==!(mask&(1u<<site)),"original collectionbit visibility");}
    Material3D fixture{{1,1,1,1},{.8f,0,0,.8f},true,{17,0x1002},.5f};
    const auto materials=Mistwatch::RepairSign::Materials(fixture);auto expectedWood=fixture;expectedWood.useUv=true;expectedWood.textureScale=1;
    const std::array<Material3D,3> expectedMaterials{expectedWood,Material3D{{1,1,1,1},{.72f,0,0,1}},Material3D{{1,1,1,1},{.65f,.7f,0,1}}};
    for(unsigned g=0;g<3;++g)Check(SameMaterial(materials[g],expectedMaterials[g]),"all material fields exact route");
    auto wrongMaterials=materials;wrongMaterials[1].textures=fixture.textures;++negatives;Check(!SameMaterial(wrongMaterials[1],expectedMaterials[1]),"paint wrong texture route");
    bool absent=false;try{(void)Mistwatch::RepairSign::Materials(Material3D{});}catch(const std::invalid_argument&){absent=true;}
    ++negatives;Check(absent,"missing original wood textures rejected");
    Reject(a,[](Product& p){p.meshes[0].indices[0]=p.meshes[0].indices[1];},"degenerate face");
    Reject(a,[](Product& p){std::swap(p.meshes[0].indices[0],p.meshes[0].indices[1]);},"reverse face");
    Reject(a,[](Product& p){p.meshes[0].indices.erase(p.meshes[0].indices.begin(),p.meshes[0].indices.begin()+3);},"open part");
    Reject(a,[](Product& p){p.meshes[2].indices[0]=500;},"index out of range");
    Reject(a,[](Product& p){p.meshes[0].vertices[0].x=.36f;},"outside original envelope");
    Reject(a,[](Product& p){p.meshes[0].vertices[0].normal=0;},"invalid normal");
    Reject(a,[](Product& p){p.meshes[0].vertices[0].u=INFINITY;},"nonfinite UV");
    Reject(a,[](Product& p){p.meshes[0].vertices[0].v=.5f;},"atlas strip wrong");
    Reject(a,[](Product& p){for(unsigned i=96;i<192;++i)std::swap(p.meshes[0].vertices[i].u,p.meshes[0].vertices[i].v);},"wrong grain");
    Reject(a,[](Product& p){p.meshes[0].vertices[0].u*=Mistwatch::RepairSign::StaticScale;},"missing static compensation");
    Reject(a,[](Product& p){p.parts[3].group=0;},"part material route");
    Reject(a,[](Product& p){p.meshes[2].vertices.pop_back();},"unclosed cap membership");
    auto crossed=a;for(auto& p:crossed.meshes[1].vertices)p.z+=.002f;
    const auto& wood=a.parts[2];const auto& paint=crossed.parts[3];
    ++negatives;Check(InteriorOverlap(Extract(a.meshes[0],wood.firstVertex,wood.vertexCount,wood.firstIndex,wood.indexCount),
        Extract(crossed.meshes[1],paint.firstVertex,paint.vertexCount,paint.firstIndex,paint.indexCount))&&!Validate(crossed),"true volume crossing reject not only boxes");
    Mesh3D boxA,boxB,boxC;
    Check(AppendBeveledBox3D(boxA,{0,0,0},{1,1,1},.2f,{1,1,1,1})&&AppendBeveledBox3D(boxB,{.9f,.9f,0},{1,1,1},.2f,{1,1,1,1})&&
          AppendBeveledBox3D(boxC,{.7f,.7f,0},{1,1,1},.2f,{1,1,1,1}),"bounded actual SAT controls");
    const auto ha=Extract(boxA,0,96,0,132),hb=Extract(boxB,0,96,0,132),hc=Extract(boxC,0,96,0,132);
    Check(!InteriorOverlap(ha,hb),"overlapping AABBs but truly separated beveled corners");
    ++negatives;Check(InteriorOverlap(ha,hc),"true beveled convex volume overlap");
    std::cout<<"REPAIR_SIGN_CPU_PASS checks="<<checks<<" negatives="<<negatives<<" vertices="<<vertices<<" triangles="<<triangles
             <<" groups=3 closed_parts=6 native=false art=false world_changed=false\n";return 0;
}catch(const std::exception& e){std::cerr<<"FIRST_DIVERGENCE "<<e.what()<<'\n';return 1;}}
