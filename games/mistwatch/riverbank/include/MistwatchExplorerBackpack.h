#pragma once
#include "MistwatchExplorerSurface.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace Mistwatch::BackpackFit1250 {
struct Checkpoint {void(*callback)(void*){};void* context{};void operator()()const {if(callback)callback(context);}};
struct Receipt {std::uint32_t sourceRecords{9225},records{9281},sourceIndices{36792},indices{37032},appended{56};};
namespace Detail {
using namespace ExplorerArtAuthor;
inline void Need(bool value,const char* why){if(!value)throw std::runtime_error(why);}
inline std::uint32_t Actual(std::uint32_t local){Need(local>=6931&&local<7149,"owned local ID domain");return local<7093?local:9225u+local-7093u;}
// V1204固定六圈與附件配方；本候選只改來源圈前半及真平面cap平均，尚未執行。
inline void Shape(Builder& b) {
    // 背側原支持點保留；中圈由固定內縮來源生成，最終f32須由原袋凸證書獨立驗證。
    const auto first=static_cast<std::uint32_t>(b.model.bindMesh.vertices.size());
    const auto triangleFirst=b.triangles[static_cast<unsigned>(ExplorerArtMaterial::Leather)].size();
    const std::array<std::array<float,5>,4> sourceRows{{{0,1.03f,-.18f,.13f,.085f},{0,1.07f,-.21f,.15f,.105f},
        {0,1.34f,-.21f,.15f,.10f},{0,1.39f,-.19f,.115f,.080f}}};
    std::array<std::array<Vec3,16>,4> source{};
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<16;++c){const auto p=sourceRows[r];const float a=2*Pi*static_cast<float>(c)/16.f;
        source[r][c]=Vec3{p[0],p[1],p[2]}+Vec3{p[3]*std::sin(a),0,p[4]*std::cos(a)};}
    // 只對身體側固定半圈以原反射點作凸內縮；不修改原模型或選第二組權重。
    const auto unfitted=source;
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<16;++c){const float a=2*Pi*static_cast<float>(c)/16.f;
        if(std::cos(a)<=0)continue;const auto p=unfitted[r][c],q=unfitted[r][(8u+16u-c)%16u];
        source[r][c]={static_cast<float>(double(p.x)*.65+double(q.x)*.35),
            static_cast<float>(double(p.y)*.65+double(q.y)*.35),static_cast<float>(double(p.z)*.65+double(q.z)*.35)};
    }
    Ring previous,bottom;
    for(unsigned r=0;r<6;++r){Ring ring;
        for(unsigned c=0;c<16;++c){Vec3 p{};
            if(r==0||r==1||r==5)p=source[r==5?3:r][c];
            else if(r==2||r==3){const double t=r==2?1./3.:2./3.,s=r==2?.015:.030;
                const auto l=source[1][c],u=source[2][c],a=source[1][(c+7u)%16u],d=source[2][(c+9u)%16u];
                const double w0=(1.-s)*(1.-t),w1=(1.-s)*t,w2=s/2.,w3=s/2.;
                p={static_cast<float>(((double(l.x)*w0+double(u.x)*w1)+double(a.x)*w2)+double(d.x)*w3),
                   static_cast<float>(((double(l.y)*w0+double(u.y)*w1)+double(a.y)*w2)+double(d.y)*w3),
                   static_cast<float>(((double(l.z)*w0+double(u.z)*w1)+double(a.z)*w2)+double(d.z)*w3)};
            }else{const auto l=source[2][c],a=source[2][(c+7u)%16u],d=source[2][(c+9u)%16u];
                p={static_cast<float>((double(l.x)*.925+double(a.x)*.0375)+double(d.x)*.0375),l.y,
                   static_cast<float>((double(l.z)*.925+double(a.z)*.0375)+double(d.z)*.0375)};}
            const float angle=2*Pi*static_cast<float>(c)/16.f;
            ring.push_back(b.Vertex(p,{std::sin(angle)/.15f,0,std::cos(angle)/.1f},Pack,Rigid(1),static_cast<float>(c)/16.f,p.y));
        }
        if(previous.empty())bottom=ring;else b.Connect(previous,ring,ExplorerArtMaterial::Leather);previous=ring;
    }
    // 真圈的double平均只cast一次；保留原cap有向扇面、UV預設與Rigid(1)。
    const auto cap=[&](const Ring& ring,Vec3 normal){Need(ring.size()==16u,"fixed16 cap support");
        double x{},y{},z{};for(auto id:ring){const auto p=b.Position(id);x+=double(p.x);y+=double(p.y);z+=double(p.z);}
        const Vec3 p{static_cast<float>(x/16.),static_cast<float>(y/16.),static_cast<float>(z/16.)};
        Need(std::bit_cast<std::uint32_t>(p.y)==std::bit_cast<std::uint32_t>(b.Position(ring.front()).y),"cap true circle plane bits");
        const auto centre=b.Vertex(p,normal,Pack,Rigid(1));
        for(std::size_t c=0;c<ring.size();++c)b.Triangle(centre,ring[c],ring[(c+1u)%ring.size()],ExplorerArtMaterial::Leather,normal);
    };
    cap(bottom,{0,-1,0});cap(previous,{0,1,0});
    const auto triangleEnd=b.triangles[static_cast<unsigned>(ExplorerArtMaterial::Leather)].size();
    // 附件直接複用母袋真有向strip，不以長平面cap弦猜測曲面。
    const auto sourceTriangles=b.triangles[static_cast<unsigned>(ExplorerArtMaterial::Leather)];
    const auto sheet=[&](unsigned rowFirst,unsigned rowLast,float gap,float thickness,Color color){
        const unsigned rows=rowLast-rowFirst+1u;
        std::array<std::array<std::uint32_t,5>,3> outer{},inner{};
        for(unsigned row=0;row<rows;++row)for(unsigned col=0;col<5;++col){
            const Vec3 source=b.Position(first+(rowFirst+row)*16u+6u+col);
            outer[row][col]=b.Vertex(source+Vec3{0,0,-gap-thickness},{0,0,-1},color,Rigid(1),source.x,source.y);
            inner[row][col]=b.Vertex(source+Vec3{0,0,-gap},{0,0,1},color,Rigid(1),source.x,source.y);
        }
        const auto local=[&](std::uint32_t id,unsigned& row,unsigned& col){
            if(id<first||id>=first+96u)return false;const unsigned at=id-first;
            const unsigned sourceRow=at/16u,sourceCol=at%16u;
            if(sourceRow<rowFirst||sourceRow>rowLast||sourceCol<6u||sourceCol>10u)return false;
            row=sourceRow-rowFirst;col=sourceCol-6u;return true;
        };
        for(std::size_t at=triangleFirst;at<triangleEnd;at+=3){
            std::array<unsigned,3> row{},col{};bool included=true;
            for(unsigned k=0;k<3;++k)included=local(sourceTriangles[at+k],row[k],col[k])&&included;
            if(!included)continue;
            b.Triangle(outer[row[0]][col[0]],outer[row[1]][col[1]],outer[row[2]][col[2]],ExplorerArtMaterial::Leather,{0,0,-1});
            b.Triangle(inner[row[0]][col[0]],inner[row[2]][col[2]],inner[row[1]][col[1]],ExplorerArtMaterial::Leather,{0,0,1});
        }
        std::vector<std::pair<unsigned,unsigned>> boundary;
        for(unsigned col=0;col<5;++col)boundary.push_back({0,col});
        for(unsigned row=1;row<rows;++row)boundary.push_back({row,4});
        for(unsigned col=4;col>0;--col)boundary.push_back({rows-1u,col-1u});
        for(unsigned row=rows-1u;row>1;--row)boundary.push_back({row-1u,0});
        for(std::size_t edge=0;edge<boundary.size();++edge){
            const auto [ar,ac]=boundary[edge];const auto [br,bc]=boundary[(edge+1u)%boundary.size()];
            const Vec3 a=b.Position(outer[ar][ac]),c=b.Position(outer[br][bc]);
            const Vec3 ai=b.Position(inner[ar][ac]),ci=b.Position(inner[br][bc]);
            const Vec3 normal=Unit(Aevrynd::Cross(ai-a,c-a));
            // 側牆另建hard-normal頂點；alias邊必須在獨立Smoke重建為閉域。
            const auto a0=b.Vertex(a,normal,color,Rigid(1)),a1=b.Vertex(ai,normal,color,Rigid(1));
            const auto c0=b.Vertex(c,normal,color,Rigid(1)),c1=b.Vertex(ci,normal,color,Rigid(1));
            b.Triangle(a0,a1,c0,ExplorerArtMaterial::Leather,normal);
            b.Triangle(c0,a1,c1,ExplorerArtMaterial::Leather,normal);
        }
    };
    // 附件只貼新中段真strip，原gap及厚度不變；不再下移到舊最外支持圈。
    sheet(2,3,.004f,.012f,Leather);
    sheet(3,4,.020f,.011f,Pack);
}
inline bool Raw(const Aevrynd::Vertex3D& a,const Aevrynd::Vertex3D& z){
    return std::bit_cast<std::uint32_t>(a.x)==std::bit_cast<std::uint32_t>(z.x)&&std::bit_cast<std::uint32_t>(a.y)==std::bit_cast<std::uint32_t>(z.y)&&
        std::bit_cast<std::uint32_t>(a.z)==std::bit_cast<std::uint32_t>(z.z)&&a.normal==z.normal&&a.color==z.color&&
        std::bit_cast<std::uint32_t>(a.u)==std::bit_cast<std::uint32_t>(z.u)&&std::bit_cast<std::uint32_t>(a.v)==std::bit_cast<std::uint32_t>(z.v);}
inline void Source(const Explorer& source){
    const auto& m=source.model;Need(m.Valid()&&m.bindMesh.vertices.size()==9225&&m.bindMesh.indices.size()==36792&&m.influences.size()==9225&&
        m.tangents.empty()&&m.skeleton.joints.size()==15&&m.inverseBindMatrices.size()==15,"one post-Nape9225/12264 source required");
    Need(m.skeletonCompatibilityId==Aevrynd::ComputeStructuralSkeletonCompatibilityId3D(m.skeleton,m.inverseBindMatrices),"source structural rig identity");
    Explorer original;const auto& old=original.model;
    for(unsigned j=0;j<15;++j){const auto& a=old.skeleton.joints[j];const auto& z=m.skeleton.joints[j];Need(a.parent==z.parent,"source original15 parents");
        const auto equal=[](float x,float y){return std::bit_cast<std::uint32_t>(x)==std::bit_cast<std::uint32_t>(y);};
        const auto vec=[&](Aevrynd::Vec3 x,Aevrynd::Vec3 y){return equal(x.x,y.x)&&equal(x.y,y.y)&&equal(x.z,y.z);};
        const auto& x=a.bindTransform;const auto& y=z.bindTransform;Need(vec(x.translation,y.translation)&&vec(x.rotation,y.rotation)&&vec(x.scale,y.scale)&&
            equal(x.quaternion.x,y.quaternion.x)&&equal(x.quaternion.y,y.quaternion.y)&&equal(x.quaternion.z,y.quaternion.z)&&equal(x.quaternion.w,y.quaternion.w)&&
            x.usesQuaternion==y.usesQuaternion,"source all original bind fields");
        for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)Need(equal(old.inverseBindMatrices[j][r][c],m.inverseBindMatrices[j][r][c]),"source original inverse bind raw");
    }
    for(std::uint32_t i=6931;i<7093;++i)Need(Raw(old.bindMesh.vertices[i],m.bindMesh.vertices[i])&&
        old.influences[i].joints==m.influences[i].joints&&old.influences[i].weights==m.influences[i].weights,"exact original162 bag source fields");
    for(std::uint32_t i=12084;i<12636;++i)Need(old.bindMesh.indices[i]==m.bindMesh.indices[i],"exact original184 directed bag faces");
    ExplorerSurface::Plan plan;Need(ExplorerSurface::BuildPlan(m,source.art,plan),"source original nine-window material/lens routes");
    for(unsigned i=0;i<5;++i){const auto& a=original.art.ranges[i];const auto& z=source.art.ranges[i];
        Need(a.material==z.material&&z.firstIndex==a.firstIndex+(i==4?5016u:0u)&&z.indexCount==a.indexCount+(i==3?5016u:0u),"post-Nape ranges exact expected source delta");}
    for(unsigned i=0;i<2;++i){const auto& a=original.art.eyeLensSpans[i];const auto& z=source.art.eyeLensSpans[i];Need(a.firstIndex==z.firstIndex&&a.indexCount==z.indexCount&&
        a.firstVertex==z.firstVertex&&a.vertexCount==z.vertexCount,"source original lenses unchanged");}
}
}
// 純一次後處理；不用Evidence、檔案、GPU、模型下載或全人物Finish；姿態與非袋ID不寫。
inline Receipt Apply(Explorer& explorer,const Checkpoint& checkpoint){
    Detail::Need(checkpoint.callback!=nullptr,"same earliest budget callback required");checkpoint();Detail::Source(explorer);
    auto next=explorer.model;auto art=explorer.art;auto scratch=explorer.model;ExplorerArtAuthor::Builder b(scratch);
    b.model.bindMesh.vertices.assign(explorer.model.bindMesh.vertices.begin(),explorer.model.bindMesh.vertices.begin()+6931);
    b.model.influences.assign(explorer.model.influences.begin(),explorer.model.influences.begin()+6931);b.hints.resize(6931);
    checkpoint();Detail::Shape(b);const auto& faces=b.triangles[1];Detail::Need(b.model.bindMesh.vertices.size()==7149&&faces.size()==792,"fixed218/264 pure bag shape");
    std::array<Aevrynd::Vec3,218> sums{};for(std::size_t at=0;at<faces.size();at+=3){checkpoint();const auto a=faces[at],z=faces[at+1],c=faces[at+2];
        Detail::Need(a>=6931&&a<7149&&z>=6931&&z<7149&&c>=6931&&c<7149,"shape index ownership");
        const auto n=ExplorerArtAuthor::Unit(Aevrynd::Cross(b.Position(z)-b.Position(a),b.Position(c)-b.Position(a)));
        for(auto id:{a,z,c})sums[id-6931]=sums[id-6931]+n;
    }
    for(std::uint32_t local=6931;local<7149;++local){checkpoint();const auto& sum=sums[local-6931];Detail::Need(Aevrynd::Dot(sum,sum)>=1.e-10f,"original Finish nonopposing incident normal domain");
        auto v=b.model.bindMesh.vertices[local];v.normal=Aevrynd::PackNormalSnorm8(ExplorerArtAuthor::Unit(sum));const auto id=Detail::Actual(local);
        if(id<9225){next.bindMesh.vertices[id]=v;next.influences[id]=b.model.influences[local];}
        else{Detail::Need(id==next.bindMesh.vertices.size(),"exact tail-only56 append order");next.bindMesh.vertices.push_back(v);next.influences.push_back(b.model.influences[local]);}
    }
    next.bindMesh.indices.erase(next.bindMesh.indices.begin()+12084,next.bindMesh.indices.begin()+12636);
    std::vector<std::uint32_t> mapped;mapped.reserve(792);for(auto id:faces)mapped.push_back(Detail::Actual(id));
    next.bindMesh.indices.insert(next.bindMesh.indices.begin()+12084,mapped.begin(),mapped.end());art.ranges[1].indexCount+=240;
    for(unsigned i=2;i<5;++i)art.ranges[i].firstIndex+=240;for(auto& lens:art.eyeLensSpans)lens.firstIndex+=240;
    next.bindMesh.InvalidateRaycastIndex();Detail::Need(next.Valid()&&next.bindMesh.vertices.size()==9281&&next.bindMesh.indices.size()==37032&&next.influences.size()==9281&&
        next.tangents.empty(),"exact combined9281/12344 wellformed storage");checkpoint();
    static_assert(std::is_nothrow_swappable_v<Aevrynd::SkinnedModel3D>&&std::is_nothrow_swappable_v<ExplorerArtReceipt>);
    using std::swap;swap(explorer.model,next);swap(explorer.art,art);return {};
}
}
