#include "MistwatchRepairShelterDetail.h"
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <iomanip>
using namespace Aevrynd;
using namespace Mistwatch;
namespace D=::Mistwatch::ShelterDetail;
static bool ValidateMesh(const Mesh3D& mesh,const std::vector<D::PartRange>& parts,const std::vector<BoxCollider3D>& solids,std::string* diagnostic=nullptr) {
    const auto fail=[&](const char* gate,unsigned part,std::size_t index,const std::string& detail={}){
        if(diagnostic)*diagnostic=std::string(gate)+" part="+std::to_string(part)+" index="+std::to_string(index)+" "+detail;return false;};
    const auto values=[](std::initializer_list<double> fields){std::ostringstream s;s<<std::setprecision(17);for(auto f:fields)s<<f<<',';return s.str();};
    if(!mesh.Valid()||parts.empty())return fail("MESH_VALID",0,0);
    std::vector<unsigned> owners(mesh.vertices.size());std::size_t seenIndices{};
    for(const auto& range:parts){
        if(range.part>=solids.size()||range.firstVertex>mesh.vertices.size()||range.vertexCount>mesh.vertices.size()-range.firstVertex||
            range.firstIndex!=seenIndices||range.indexCount>mesh.indices.size()-seenIndices||range.indexCount%3)return fail("PART_RANGE",range.part,range.firstIndex);
        const auto& box=solids[range.part];const auto low=box.center-box.extents,high=box.center+box.extents;
        for(std::size_t i=range.firstVertex;i<range.firstVertex+range.vertexCount;++i){const auto& v=mesh.vertices[i];
            if(++owners[i]!=1)return fail("VERTEX_OWNER",range.part,i);
            if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||!std::isfinite(v.u)||!std::isfinite(v.v))return fail("VERTEX_FINITE",range.part,i);
            if(v.x<low.x||v.x>high.x||v.y<low.y||v.y>high.y||v.z<low.z||v.z>high.z)
                return fail("VERTEX_BOUNDS",range.part,i,"xyz,low,high="+values({v.x,v.y,v.z,low.x,low.y,low.z,high.x,high.y,high.z}));
            const auto n=UnpackNormalSnorm8(v.normal);const float length=Length({n.x,n.y,n.z});
            if(!std::isfinite(length)||length<.98f||length>1.02f)return fail("NORMAL_LENGTH",range.part,i,"normal,length="+values({n.x,n.y,n.z,length}));
        }
        for(std::size_t i=range.firstIndex;i<range.firstIndex+range.indexCount;i+=3){
            for(unsigned j=0;j<3;++j)if(mesh.indices[i+j]<range.firstVertex||mesh.indices[i+j]>=range.firstVertex+range.vertexCount)return fail("INDEX_OWNER",range.part,i+j);
            const auto& a=mesh.vertices[mesh.indices[i]];const auto& b=mesh.vertices[mesh.indices[i+1]];const auto& c=mesh.vertices[mesh.indices[i+2]];
            // 驗證使用獨立雙精度叉積，不共用生產法線函式，也不把小面積誤判成零方向。
            const double ux=double(b.x)-a.x,uy=double(b.y)-a.y,uz=double(b.z)-a.z;
            const double vx=double(c.x)-a.x,vy=double(c.y)-a.y,vz=double(c.z)-a.z;
            const double nx=uy*vz-uz*vy,ny=uz*vx-ux*vz,nz=ux*vy-uy*vx;
            const double area=std::sqrt(nx*nx+ny*ny+nz*nz);
            if(!std::isfinite(area)||area<1.e-10f)return fail("TRIANGLE_AREA",range.part,i/3,"area="+values({area}));
            for(const auto* v:{&a,&b,&c}){const auto n=UnpackNormalSnorm8(v->normal);const double alignment=(nx*n.x+ny*n.y+nz*n.z)/area;
                if(!std::isfinite(alignment)||alignment<.98f)return fail("WINDING_NORMAL",range.part,i/3,"area,alignment="+values({area,alignment}));}
            const double uv=(double(b.u)-a.u)*(double(c.v)-a.v)-(double(c.u)-a.u)*(double(b.v)-a.v);
            if(!std::isfinite(uv)||std::abs(uv)<1.e-15)return fail("UV_JACOBIAN",range.part,i/3,"uv="+values({uv,a.u,a.v,b.u,b.v,c.u,c.v}));
        }
        seenIndices+=range.indexCount;
    }
    if(seenIndices!=mesh.indices.size()||!std::all_of(owners.begin(),owners.end(),[](unsigned owner){return owner==1;}))return fail("UNCOVERED_GEOMETRY",0,seenIndices);
    return true;
}
static bool ClosedRoof(const Mesh3D& mesh) {
    using Point=std::array<std::uint32_t,3>;using Edge=std::pair<Point,Point>;
    std::map<Edge,std::pair<unsigned,int>> edges;
    const auto point=[&](unsigned index){const auto& v=mesh.vertices[index];return Point{
        std::bit_cast<std::uint32_t>(v.x),std::bit_cast<std::uint32_t>(v.y),std::bit_cast<std::uint32_t>(v.z)};};
    for(std::size_t i=0;i<mesh.indices.size();i+=3)for(unsigned j=0;j<3;++j){auto a=point(mesh.indices[i+j]),b=point(mesh.indices[i+(j+1)%3]);
        const int orientation=a<b?1:-1;if(b<a)std::swap(a,b);auto& e=edges[{a,b}];++e.first;e.second+=orientation;}
    return !edges.empty()&&std::all_of(edges.begin(),edges.end(),[](const auto& entry){return entry.second.first==2&&entry.second.second==0;});
}
static bool SameVertex(const Vertex3D& a,const Vertex3D& b) {
    for(const auto pair:std::array<std::pair<float,float>,5>{{{a.x,b.x},{a.y,b.y},{a.z,b.z},{a.u,b.u},{a.v,b.v}}})
        if(std::bit_cast<std::uint32_t>(pair.first)!=std::bit_cast<std::uint32_t>(pair.second))return false;
    return a.normal==b.normal&&a.color==b.color;
}
int main()try{
    const auto need=[](bool good,const char* why){if(!good)throw std::runtime_error(why);};
    // 2mm有效三角形的叉積為4e-6：小於一般Normalize門檻，但不是退化幾何。
    Mesh3D millimetre;millimetre.vertices.resize(3);millimetre.indices={0,1,2};
    millimetre.vertices[1].x=.002f;millimetre.vertices[2].y=.002f;
    D::Detail::Normals(millimetre,0,3);
    for(const auto& vertex:millimetre.vertices){const auto n=UnpackNormalSnorm8(vertex.normal);
        need(Length({n.x,n.y,n.z})>=.98f&&Length({n.x,n.y,n.z})<=1.02f&&n.z>=.98f,"Millimetre triangle keeps nonzero outward normal");}
    const auto solids=RepairShelterSolids();const auto old=ShelterWeatheredUv::Create();const auto oldHash=D::Fingerprint(old);
    const auto out=D::Create();
    std::string diagnostic;
    if(!ValidateMesh(out.wood,out.woodParts,solids,&diagnostic))throw std::runtime_error("WOOD "+diagnostic);
    if(!ValidateMesh(out.roof,out.roofParts,solids,&diagnostic))throw std::runtime_error("ROOF "+diagnostic);
    need(ClosedRoof(out.roof),"Roof must be watertight with consistently oriented paired edges");
    need(out.corrugations==20&&out.frameMembers==57,"Visible full corrugations, 54 wall frames and three rafters");
    need(out.woodParts.size()==61&&out.roofParts.size()==1,"Four original wall/table parts and complete frame ownership");
    unsigned rafters{};for(const auto& range:out.woodParts){need(range.part<=4,"Known wood part");
        if(range.part==0){need(range.frame&&range.vertexCount==96&&range.indexCount==132,"Roof wood is a new rafter, not original plank slab");++rafters;}}
    need(rafters==3,"Exactly three roof support rafters");
    const auto frameBounds=[&](const D::PartRange& range){Vec3 low{1000,1000,1000},high{-1000,-1000,-1000};
        for(std::size_t i=range.firstVertex;i<range.firstVertex+range.vertexCount;++i){const auto& v=out.wood.vertices[i];
            low={(std::min)(low.x,v.x),(std::min)(low.y,v.y),(std::min)(low.z,v.z)};
            high={(std::max)(high.x,v.x),(std::max)(high.y,v.y),(std::max)(high.z,v.z)};}
        return std::pair{low,high};};
    for(unsigned i=0;i<out.woodParts.size();++i)if(out.woodParts[i].frame)
        for(unsigned j=i+1;j<out.woodParts.size();++j)if(out.woodParts[j].frame&&out.woodParts[i].part==out.woodParts[j].part){
            const auto a=frameBounds(out.woodParts[i]),b=frameBounds(out.woodParts[j]);
            need(!(a.first.x<b.second.x&&a.second.x>b.first.x&&a.first.y<b.second.y&&a.second.y>b.first.y&&
                a.first.z<b.second.z&&a.second.z>b.first.z),"Wall frame members must not have overlapping coplanar faces");}
    const auto& table=out.woodParts[3];need(table.part==4&&table.vertexCount==96&&table.indexCount==132&&!table.frame,"Original tool table membership");
    const auto tableFirst=old.vertices.size()-96,tableIndex=old.indices.size()-132;
    for(std::size_t i=0;i<96;++i)need(SameVertex(old.vertices[tableFirst+i],out.wood.vertices[table.firstVertex+i]),"Tool table bit exact");
    for(std::size_t i=0;i<132;++i)need(old.indices[tableIndex+i]-tableFirst==out.wood.indices[table.firstIndex+i]-table.firstVertex,"Tool table exact topology");
    // 牆板只在厚度方向退縮兩公分；框架則在保留的薄殼內形成真深度。
    std::size_t sourceFirst=ShelterBoardCounts[0]*96u;
    for(unsigned part=1;part<=3;++part){const auto& range=out.woodParts[part-1];const auto box=solids[part];
        for(std::size_t i=0;i<range.vertexCount;++i){const auto& v=out.wood.vertices[range.firstVertex+i];const auto& s=old.vertices[sourceFirst+i];
            if(part==1)need(v.x==box.center.x+(s.x-box.center.x)*.8f&&v.y==s.y&&v.z==s.z,"West thickness-only inset");
            else need(v.z==box.center.z+(s.z-box.center.z)*.8f&&v.y==s.y&&v.x==s.x,"North/south thickness-only inset");
            need(v.u==s.u&&v.v==s.v&&v.color==s.color,"Wall weathered UV/color preserved");}
        sourceFirst+=range.vertexCount;
    }
    const float bottom=solids[0].center.y-solids[0].extents.y,top=solids[0].center.y+solids[0].extents.y;
    const float trough=top-.08f,supportTop=trough-.004f;
    float actualLow=100,actualHigh=-100;std::set<std::uint32_t> heights;
    std::map<std::pair<float,float>,std::set<float>> roofColumns;
    for(const auto& v:out.roof.vertices){actualLow=(std::min)(actualLow,v.y);actualHigh=(std::max)(actualHigh,v.y);heights.insert(std::bit_cast<std::uint32_t>(v.y));
        roofColumns[{v.x,v.z}].insert(v.y);}
    need(actualLow==supportTop&&actualHigh==top&&heights.size()==4,"Roof top and bottom follow corrugations, without flat underside");
    need(roofColumns.size()==162,"Every corrugated strip has two end columns");
    for(const auto& [position,column]:roofColumns){need(column.size()==2,"Exactly top and underside at each roof column");
        const float high=*column.rbegin(),low=*column.begin();
        need((high==top||high==trough)&&low==high-.004f&&std::abs((double(high)-low)-.004)<1.e-6,"Uniform four millimetre vertical sheet thickness");}
    float supportLow=100;unsigned supportIndex{};
    for(const auto& range:out.woodParts)if(range.part==0){const auto bounds=frameBounds(range);
        need(bounds.first.y==bottom&&bounds.second.y==supportTop,"Rafters preserve original underside and touch trough bottom");
        need(bounds.first.x==solids[0].center.x-solids[0].extents.x&&bounds.second.x==solids[0].center.x+solids[0].extents.x,"Rafters span original roof width");
        const std::array<float,3> offsets{-1.7f,0.f,1.7f};
        need(std::abs((bounds.first.z+bounds.second.z)*.5f-(solids[0].center.z+offsets[supportIndex]))<1.e-6f,"Rafters at three designed support positions");
        supportLow=(std::min)(supportLow,bounds.first.y);++supportIndex;}
    need(supportLow==bottom&&std::abs(bottom-1.45f)<1.e-6f&&actualHigh==top,"Combined roof and supports preserve complete original vertical envelope");
    const float east=solids[0].center.x+solids[0].extents.x;
    for(const auto& range:out.woodParts)if(range.part!=0)
        for(std::size_t i=range.firstVertex;i<range.firstVertex+range.vertexCount;++i){const auto& v=out.wood.vertices[i];
            if(v.x>east-.15f)need(v.z<=solids[2].center.z+solids[2].extents.z||
                v.z>=solids[3].center.z-solids[3].extents.z,"East opening cannot acquire wall frame geometry");}
    const auto again=D::Create();need(D::Fingerprint(again.wood)==out.woodFingerprint&&D::Fingerprint(again.roof)==out.roofFingerprint,"Deterministic complete mesh output");
    const auto solidsAfter=RepairShelterSolids();need(solidsAfter.size()==5&&D::Fingerprint(ShelterWeatheredUv::Create())==oldHash,"Old roof/boards remain unchanged");
    for(unsigned i=0;i<5;++i)need(solids[i].center.x==solidsAfter[i].center.x&&solids[i].center.y==solidsAfter[i].center.y&&
        solids[i].center.z==solidsAfter[i].center.z&&solids[i].extents.x==solidsAfter[i].extents.x&&solids[i].extents.y==solidsAfter[i].extents.y&&
        solids[i].extents.z==solidsAfter[i].extents.z,"All five original colliders unchanged");
    unsigned negatives{};auto bad=out.wood;bad.vertices[0].x=-100;need(!ValidateMesh(bad,out.woodParts,solids),"Outside geometry accepted");++negatives;
    // 保留m2474真實失敗回歸：哪怕只低於作者底面1 ULP，也必須判退，不能調寬bounds。
    bad=out.wood;bool rafterMutated=false;
    for(const auto& range:out.woodParts)if(range.part==0&&!rafterMutated)
        for(std::size_t i=range.firstVertex;i<range.firstVertex+range.vertexCount;++i)if(bad.vertices[i].y==bottom){
            bad.vertices[i].y=std::nextafter(bottom,0.f);rafterMutated=true;break;}
    need(rafterMutated&&!ValidateMesh(bad,out.woodParts,solids),"One ULP outside rafter endpoint accepted");++negatives;
    bad=out.roof;std::swap(bad.indices[0],bad.indices[1]);need(!ValidateMesh(bad,out.roofParts,solids),"Reversed face accepted");++negatives;
    bad=out.roof;for(auto& v:bad.vertices){v.u=0;v.v=0;}need(!ValidateMesh(bad,out.roofParts,solids),"Collapsed UV accepted");++negatives;
    bad=out.roof;bad.indices.resize(bad.indices.size()-6);need(!ClosedRoof(bad),"Missing roof cap accepted");++negatives;
    std::cout<<"DETAIL_EXPECTED shelter_detail_wood_vertices="<<out.wood.vertices.size()<<'\n'
        <<"DETAIL_EXPECTED shelter_detail_wood_indices="<<out.wood.indices.size()<<'\n'
        <<"DETAIL_EXPECTED shelter_detail_wood_fingerprint="<<out.woodFingerprint<<'\n'
        <<"DETAIL_EXPECTED shelter_detail_roof_vertices="<<out.roof.vertices.size()<<'\n'
        <<"DETAIL_EXPECTED shelter_detail_roof_indices="<<out.roof.indices.size()<<'\n'
        <<"DETAIL_EXPECTED shelter_detail_roof_fingerprint="<<out.roofFingerprint<<'\n'
        <<"DETAIL_EXPECTED shelter_detail_frame_members="<<out.frameMembers<<'\n'
        <<"DETAIL_EXPECTED shelter_detail_corrugations="<<out.corrugations<<'\n'
        <<"SHELTER_DETAIL_CPU_PASS old_geometry_unchanged=1 colliders_unchanged=1 watertight_roof=1 millimetre_normal=1 negative_controls="<<negatives<<" native=0 art=0\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"SHELTER_DETAIL_FIRST_DIVERGENCE "<<e.what()<<'\n';return 1;}
