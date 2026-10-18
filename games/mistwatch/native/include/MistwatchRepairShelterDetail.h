#pragma once
#include "MistwatchShelterWeatheredUv.h"

namespace Mistwatch::ShelterDetail {
using namespace Aevrynd;
inline constexpr const char* Profile="framed-corrugated-v1";
struct PartRange {
    unsigned part{};
    std::size_t firstVertex{},vertexCount{},firstIndex{},indexCount{};
    bool frame{};
};
struct Result {
    Mesh3D wood,roof;
    std::vector<PartRange> woodParts,roofParts;
    std::uint64_t woodFingerprint{},roofFingerprint{};
    unsigned frameMembers{},corrugations{};
};
inline std::uint64_t Fingerprint(const Mesh3D& mesh) {
    std::uint64_t hash=14695981039346656037ull;
    const auto word=[&](std::uint32_t v){hash^=v;hash*=1099511628211ull;};
    word(static_cast<std::uint32_t>(mesh.vertices.size()));word(static_cast<std::uint32_t>(mesh.indices.size()));
    for(const auto& v:mesh.vertices){for(float x:{v.x,v.y,v.z,v.u,v.v})word(std::bit_cast<std::uint32_t>(x));word(v.normal);word(v.color);}
    for(const auto index:mesh.indices)word(index);return hash;
}
namespace Detail {
inline Vec3 Point(const Vertex3D& p){return {p.x,p.y,p.z};}
inline void Normals(Mesh3D& mesh,std::size_t first,std::size_t count) {
    // 每個倒角面為獨立頂點；非等比內縮後以真三角面重算法線，保留原UV及面順序。
    for(std::size_t i=first;i<first+count;i+=3){
        const auto a=mesh.indices[i],b=mesh.indices[i+1],c=mesh.indices[i+2];
        // 毫米倒角的叉積面積小於一般向量Normalize門檻，不能因此產生零法線。
        Vec3 normal{};
        if(!TryComputeTriangleNormal(Point(mesh.vertices[a]),Point(mesh.vertices[b]),Point(mesh.vertices[c]),normal))
            throw std::runtime_error("SHELTER_DETAIL_INSET_DEGENERATE_TRIANGLE_"+std::to_string(i/3));
        for(const auto id:{a,b,c})mesh.vertices[id].normal=PackNormalSnorm8(normal);
    }
}
inline void Frame(Result& result,unsigned part,Vec3 centre,Vec3 size,Vec3 axis0={1,0,0},Vec3 axis1={0,1,0},Vec3 axis2={0,0,1},
    const std::array<float,2>* authoredY=nullptr) {
    const auto first=result.wood.vertices.size(),index=result.wood.indices.size();
    if(!AppendBeveledBox3D(result.wood,centre,size,.002f,{.64f,.61f,.56f,1},axis0,axis1,axis2))
        throw std::runtime_error("SHELTER_DETAIL_FRAME_GEOMETRY");
    if(authoredY){
        // 支撐梁必須共用碰撞體的精確端面；中心加減半高可能因運算順序跨出1 ULP。
        // 只對這根新梁做端面參數映射，並非截斷全網格或容許碰撞體外的頂點。
        float generatedLow=result.wood.vertices[first].y,generatedHigh=generatedLow;
        for(std::size_t i=first;i<result.wood.vertices.size();++i){generatedLow=(std::min)(generatedLow,result.wood.vertices[i].y);
            generatedHigh=(std::max)(generatedHigh,result.wood.vertices[i].y);}
        if(!(generatedHigh>generatedLow&&(*authoredY)[1]>(*authoredY)[0]))throw std::runtime_error("SHELTER_DETAIL_RAFTER_ENDPOINTS");
        for(std::size_t i=first;i<result.wood.vertices.size();++i){auto& y=result.wood.vertices[i].y;
            if(y==generatedLow)y=(*authoredY)[0];
            else if(y==generatedHigh)y=(*authoredY)[1];
            else y=float(double((*authoredY)[0])+(double(y)-generatedLow)/(double(generatedHigh)-generatedLow)*
                (double((*authoredY)[1])-(*authoredY)[0]));}
        Normals(result.wood,index,result.wood.indices.size()-index);
    }
    const std::array<Vec3,3> axes{axis0,axis1,axis2};const std::array<float,3> lengths{size.x,size.y,size.z};
    const auto band=ShelterWeatheredUv::Bands[result.frameMembers%4];
    for(std::size_t i=first;i<result.wood.vertices.size();++i){auto& v=result.wood.vertices[i];
        const auto n=UnpackNormalSnorm8(v.normal);const Vec3 normal{n.x,n.y,n.z};
        unsigned dominant=0;for(unsigned a=1;a<3;++a)if(std::abs(Dot(normal,axes[a]))>std::abs(Dot(normal,axes[dominant])))dominant=a;
        unsigned u=(dominant+1)%3,w=(dominant+2)%3;if(lengths[u]>lengths[w])std::swap(u,w);
        const auto p=Point(v)-centre;
        const float across=Dot(p,axes[u])/lengths[u]+.5f;
        v.u=(band.x+across*(band.y-band.x))/ShelterRoofUv::StaticDecodedScale;
        v.v=(.13f*float(result.frameMembers%5)+(Dot(p,axes[w])+lengths[w]*.5f)/1.4f)/ShelterRoofUv::StaticDecodedScale;
    }
    result.woodParts.push_back({part,first,result.wood.vertices.size()-first,index,result.wood.indices.size()-index,true});++result.frameMembers;
}
inline void Quad(Mesh3D& mesh,std::array<Vec3,4> points,Vec3 outward) {
    if(Dot(Cross(points[1]-points[0],points[2]-points[0]),outward)<0)std::swap(points[1],points[3]);
    const auto normal=Normalize(Cross(points[1]-points[0],points[2]-points[0]));
    const auto axis=Normalize(points[1]-points[0]),other=Cross(normal,axis);
    const auto first=static_cast<std::uint32_t>(mesh.vertices.size());
    for(const auto p:points){const auto delta=p-points[0];mesh.vertices.push_back({p.x,p.y,p.z,
        PackNormalSnorm8(normal),PackColorRgba8({1,1,1,1}),Dot(delta,axis),Dot(delta,other)});}
    mesh.indices.insert(mesh.indices.end(),{first,first+1,first+2,first,first+2,first+3});
}
}
inline Result Create() {
    Result result;const auto solids=RepairShelterSolids();const auto source=ShelterWeatheredUv::Create();
    if(solids.size()!=5||source.vertices.size()!=ShelterVisualVertices||source.indices.size()!=ShelterVisualIndices)
        throw std::runtime_error("SHELTER_DETAIL_SOURCE_MEMBERSHIP");
    std::size_t sourceVertex{},sourceIndex{};
    for(unsigned part=0;part<5;++part){const auto vertices=std::size_t(ShelterBoardCounts[part])*96,indices=std::size_t(ShelterBoardCounts[part])*132;
        if(part!=0){const auto first=result.wood.vertices.size(),index=result.wood.indices.size();const auto& box=solids[part];
            for(std::size_t i=sourceVertex;i<sourceVertex+vertices;++i){auto v=source.vertices[i];
                if(part==1)v.x=box.center.x+(v.x-box.center.x)*.8f;
                else if(part==2||part==3)v.z=box.center.z+(v.z-box.center.z)*.8f;
                result.wood.vertices.push_back(v);}
            for(std::size_t i=sourceIndex;i<sourceIndex+indices;++i)result.wood.indices.push_back(
                static_cast<std::uint32_t>(first+source.indices[i]-sourceVertex));
            if(part<4)Detail::Normals(result.wood,index,indices);
            result.woodParts.push_back({part,first,vertices,index,indices,false});
        }
        sourceVertex+=vertices;sourceIndex+=indices;
    }
    // 框架兩面都完全留在既有牆體厚度內；東側不新增門楣或柱，不改蹲入路徑。
    for(unsigned part=1;part<=3;++part){const auto& box=solids[part];const bool west=part==1;
        const Vec3 along=west?Vec3{0,0,1}:Vec3{1,0,0};const Vec3 up{0,1,0};
        const Vec3 normal=Cross(along,up);const float half=west?box.extents.z:box.extents.x;
        for(const float side:{-1.f,1.f}){
            const Vec3 centre=box.center+normal*(side*.09f);
            for(const float offset:{-half+.085f,0.f,half-.085f})
                Detail::Frame(result,part,centre+along*offset,{.12f,1.36f,.018f},along,up,normal);
            // 橫梁在三根立柱之間分段，兩端各留5mm；避免相同外表面重疊閃爍。
            for(const float offset:{-.57f,.57f})for(const float sign:{-1.f,1.f})
                Detail::Frame(result,part,centre+up*offset+along*(sign*(half-.085f)*.5f),
                    {half-.215f,.12f,.018f},along,up,normal);
            // 雙斜撐在兩個半牆段形成結構輪廓，不跨過外部牆盒。
            for(const float sign:{-1.f,1.f}){
                const Vec3 a=centre+along*(sign*.13f)+up*(-.45f);
                const Vec3 b=centre+along*(sign*(half-.19f))+up*.45f;
                const Vec3 axis=Normalize(b-a),across=Cross(normal,axis);
                Detail::Frame(result,part,(a+b)*.5f,{Length(b-a),.10f,.018f},axis,across,normal);
            }
        }
    }
    const auto roof=solids[0];const float minX=roof.center.x-roof.extents.x,maxX=roof.center.x+roof.extents.x;
    const float minZ=roof.center.z-roof.extents.z,maxZ=roof.center.z+roof.extents.z;
    const float bottom=roof.center.y-roof.extents.y,peak=roof.center.y+roof.extents.y,trough=peak-.08f;
    constexpr float sheetThickness=.004f;
    // 實機審視後改為薄波紋板與三根實體木樑，不再以厚實平底金屬盒填滿碰撞體。
    const float supportTop=trough-sheetThickness,supportHeight=supportTop-bottom;
    const std::array<float,2> supportEndpoints{bottom,supportTop};
    for(const float offset:{-1.7f,0.f,1.7f})
        Detail::Frame(result,0,{roof.center.x,bottom+supportHeight*.5f,roof.center.z+offset},
            {roof.extents.x*2.f,supportHeight,.14f},{1,0,0},{0,1,0},{0,0,1},&supportEndpoints);
    result.corrugations=20;constexpr unsigned segments=80;
    std::array<float,segments+1> xs{},ys{};
    for(unsigned i=0;i<=segments;++i){xs[i]=i==segments?maxX:minX+(maxX-minX)*float(i)/float(segments);
        ys[i]=(i%4==1||i%4==2)?peak:trough;}
    // 上下表面相隔4mm並跟隨相同波形；四周薄邊封閉，木樑底維持原1.45m。
    for(unsigned i=0;i<segments;++i){const float x0=xs[i],x1=xs[i+1],y0=ys[i],y1=ys[i+1];
        const float b0=y0-sheetThickness,b1=y1-sheetThickness;
        Detail::Quad(result.roof,{{{x0,y0,minZ},{x1,y1,minZ},{x1,y1,maxZ},{x0,y0,maxZ}}},{0,1,0});
        Detail::Quad(result.roof,{{{x0,b0,minZ},{x1,b1,minZ},{x1,b1,maxZ},{x0,b0,maxZ}}},{0,-1,0});
        Detail::Quad(result.roof,{{{x0,b0,minZ},{x1,b1,minZ},{x1,y1,minZ},{x0,y0,minZ}}},{0,0,-1});
        Detail::Quad(result.roof,{{{x0,b0,maxZ},{x1,b1,maxZ},{x1,y1,maxZ},{x0,y0,maxZ}}},{0,0,1});
    }
    Detail::Quad(result.roof,{{{minX,ys[0]-sheetThickness,minZ},{minX,ys[0],minZ},{minX,ys[0],maxZ},{minX,ys[0]-sheetThickness,maxZ}}},{-1,0,0});
    Detail::Quad(result.roof,{{{maxX,ys[segments]-sheetThickness,minZ},{maxX,ys[segments],minZ},{maxX,ys[segments],maxZ},{maxX,ys[segments]-sheetThickness,maxZ}}},{1,0,0});
    result.roofParts.push_back({0,0,result.roof.vertices.size(),0,result.roof.indices.size(),false});
    result.wood.InvalidateRaycastIndex();result.roof.InvalidateRaycastIndex();
    if(!result.wood.Valid()||!result.roof.Valid())throw std::runtime_error("SHELTER_DETAIL_INVALID_OUTPUT");
    result.woodFingerprint=Fingerprint(result.wood);result.roofFingerprint=Fingerprint(result.roof);return result;
}
}
