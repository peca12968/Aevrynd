#include "MistwatchSettlementGround.h"
#include "MistwatchForestHabitatFloor.h"
#include "fallen/MistwatchAuthoredNativeExpected.h"
#include "MistwatchEntranceRoadMaterial.h"
#include <iostream>
#include <limits>

namespace {
void Need(bool condition,const char* reason){if(!condition)throw std::runtime_error(reason);}
bool Same(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool Geometry(const Aevrynd::Vertex3D& a,const Aevrynd::Vertex3D& b){
    return Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z)&&a.color==b.color&&a.normal==b.normal;
}
bool SameTestVertex(const Aevrynd::Vertex3D& a,const Aevrynd::Vertex3D& b){
    return Geometry(a,b)&&Same(a.u,b.u)&&Same(a.v,b.v);
}
}
void CheckFixture(const Aevrynd::Mesh3D& source,const char* marker){
    using namespace Mistwatch;namespace S=SettlementGround;
    const auto preserved=source;const auto colliders=WorkshopSolids();
    const auto entrance=CreateEntranceRoadDomain(source),preservedEntrance=entrance;
    const auto a=S::Create(source,entrance),b=S::Create(source,entrance);const auto& p=a.partition;
    Need(S::SameMesh(source,preserved)&&S::SameMesh(entrance.forest,preservedEntrance.forest)&&
        S::SameMesh(entrance.road,preservedEntrance.road),"Input changed");
    Need(S::SameMesh(p.forest,b.partition.forest)&&S::SameMesh(p.road,b.partition.road)&&
        a.movedSourceTriangles==b.movedSourceTriangles&&a.newBoundaryVertices==b.newBoundaryVertices,"Determinism");
    Need(p.road.vertices.size()==source.vertices.size()&&p.forest.vertices.size()==source.vertices.size(),"New vertices");
    Need(std::equal(entrance.road.indices.begin(),entrance.road.indices.end(),p.road.indices.begin()),"Old road prefix");
    std::vector<std::uint8_t> member(source.vertices.size());
    std::size_t fi{},ri=entrance.road.indices.size(),moved{};
    for(std::size_t t=0;t<entrance.triangleIsRoad.size();++t){
        const bool added=!entrance.triangleIsRoad[t]&&p.triangleIsRoad[t];
        Need(!entrance.triangleIsRoad[t]||p.triangleIsRoad[t],"Old road removed");
        for(unsigned c=0;c<3;++c){const auto index=source.indices[t*3+c];
            member[index]=std::uint8_t(member[index]|(p.triangleIsRoad[t]?2u:1u));
            if(added){Need(ri<p.road.indices.size()&&p.road.indices[ri++]==index,"New road identity/winding");
                const auto& v=source.vertices[index];const auto roof=colliders[0];
                const float east=roof.center.x+roof.extents.x;
                Need(v.x>=roof.center.x-roof.extents.x-.5f&&v.x<=east+3.5f&&
                    std::abs(v.z-roof.center.z)<=roof.extents.z+.5f-(std::max)(0.f,v.x-east)/3.5f,
                    "Outside authored workshop footprint");
            } else if(!p.triangleIsRoad[t])
                Need(fi<p.forest.indices.size()&&p.forest.indices[fi++]==index,"Forest identity/winding");
            if(entrance.triangleIsRoad[t])Need(SameTestVertex(entrance.road.vertices[index],p.road.vertices[index]),"Entrance road vertex changed");
        }
        if(added){Need(moved<a.movedSourceTriangles.size()&&a.movedSourceTriangles[moved]==t,"Moved ID mismatch");++moved;}
    }
    Need(fi==p.forest.indices.size()&&ri==p.road.indices.size()&&
        fi+ri==source.indices.size()&&moved==a.movedSourceTriangles.size(),"Missing/duplicate triangle");
    std::vector<std::uint32_t> seams;
    std::size_t changed{},interior{},eastApron{},forestFromSource{},roadFromSource{};
    for(std::size_t i=0;i<source.vertices.size();++i){const auto& s=source.vertices[i];
        for(const auto* v:{&p.forest.vertices[i],&p.road.vertices[i]}){
            Need(Geometry(s,*v)&&Same(s.v,v->v),"Geometry or rock modified");
            Need(std::isfinite(v->u)&&v->u>=0.f&&v->u+v->v<=1.f,"Invalid coverage");
        }
        const auto roof=colliders[0];
        const bool outside=s.x<roof.center.x-roof.extents.x-1.25f||
            s.x>roof.center.x+roof.extents.x+4.25f||
            std::abs(s.z-roof.center.z)>roof.extents.z+1.25f;
        if(outside)Need(SameTestVertex(p.forest.vertices[i],entrance.forest.vertices[i])&&
            SameTestVertex(p.road.vertices[i],entrance.road.vertices[i]),"Out of bounds change");
        changed+=!Same(p.forest.vertices[i].u,entrance.forest.vertices[i].u);
        changed+=!Same(p.road.vertices[i].u,entrance.road.vertices[i].u);
        forestFromSource+=!Same(p.forest.vertices[i].u,s.u);
        roadFromSource+=!Same(p.road.vertices[i].u,s.u);
        if(member[i]==3u){seams.push_back(std::uint32_t(i));
            Need(Same(p.road.vertices[i].u,p.forest.vertices[i].u),"Material seam mismatch");}
        if(s.x>=-8.f&&s.x<=-2.5f&&std::abs(s.z-Session::fuse.z)<.26f&&member[i]==2u&&
            p.road.vertices[i].u<.2f){++interior;if(s.x>colliders[0].center.x+colliders[0].extents.x)++eastApron;}
    }
    Need(seams==p.sharedBoundaryVertices,"Independent seam incidence");
    Need(moved>100&&moved<=S::MaxMovedTriangles&&changed>100&&changed<=S::MaxChangedVertices&&
        a.newBoundaryVertices.size()<=S::MaxBoundaryVertices&&interior>5&&eastApron>0,"Visible bounded area missing");
    Need(S::Inside(-2.5f,Session::fuse.z)&&!S::Inside(-11.5f,Session::fuse.z),"East entry orientation");
    const auto after=WorkshopSolids();Need(colliders.size()==after.size(),"Collider count changed");
    for(std::size_t i=0;i<colliders.size();++i){const auto& x=colliders[i];const auto& y=after[i];
        Need(Same(x.center.x,y.center.x)&&Same(x.center.y,y.center.y)&&Same(x.center.z,y.center.z)&&
            Same(x.extents.x,y.extents.x)&&Same(x.extents.y,y.extents.y)&&Same(x.extents.z,y.extents.z),"Collider changed");}
    const auto forest=HabitatGroundDefinition(true,true);const auto road=EntranceRoadMaterialDefinition(forest);
    Need(road.textures.baseColorPath=="poly-haven/forest-ground-04/slot-0-base.dds"&&
        road.textures.additionalLayers[0].baseColorPath==forest.textures.additionalLayers[0].baseColorPath&&
        road.textures.additionalLayers[1].baseColorPath==forest.textures.additionalLayers[1].baseColorPath,"Material slot semantics");
    unsigned negatives{};
    const auto reject=[&](const auto& mesh,const auto& domain){bool rejected=false;
        try{(void)S::Create(mesh,domain);}catch(const std::invalid_argument&){rejected=true;}
        Need(rejected,"Negative accepted");++negatives;};
    auto bad=source;bad.vertices[0].u=std::numeric_limits<float>::quiet_NaN();reject(bad,entrance);
    bad=source;bad.vertices[0].v=1.1f;reject(bad,entrance);
    bad=source;bad.indices.pop_back();reject(bad,entrance);
    auto badDomain=entrance;badDomain.road.vertices[0].u=.314f;reject(source,badDomain);
    badDomain=entrance;badDomain.forest.indices[0]=badDomain.forest.indices[1];reject(source,badDomain);
    reject(source,p);
    const auto& old=entrance.receipt;const auto& current=p.receipt;
    Need(current.sourceFingerprint==EntranceRoadDomainDetail::Fingerprint(source)&&
        current.sharedVertices==seams.size()&&current.forestChangedVertices==forestFromSource&&
        current.roadChangedVertices==roadFromSource&&
        a.changedForestVertices+a.changedRoadVertices==changed,"Receipt actual mesh counts");
    Need(current.sourceFingerprint==old.sourceFingerprint&&
        current.sourceTriangles==old.sourceTriangles&&
        current.forestTriangles+current.roadTriangles==current.sourceTriangles&&
        current.roadTriangles==old.roadTriangles+moved&&
        current.forestTriangles+moved==old.forestTriangles,"Receipt triangle contract");
    Need(current.forestFingerprint==EntranceRoadDomainDetail::Fingerprint(p.forest)&&
        current.roadFingerprint==EntranceRoadDomainDetail::Fingerprint(p.road),"Receipt fingerprint contract");
    // expected 完全來自 CPU 真來源，不能由 GPU 上傳結果回填。
    std::cout<<marker<<" source_fingerprint="<<current.sourceFingerprint
        <<" source_triangles="<<current.sourceTriangles
        <<" entrance_forest_fingerprint="<<old.forestFingerprint
        <<" entrance_road_fingerprint="<<old.roadFingerprint
        <<" entrance_forest_triangles="<<old.forestTriangles
        <<" entrance_road_triangles="<<old.roadTriangles
        <<" entrance_shared_vertices="<<old.sharedVertices
        <<" entrance_forest_changed_vertices="<<old.forestChangedVertices
        <<" entrance_road_changed_vertices="<<old.roadChangedVertices
        <<" forest_fingerprint="<<current.forestFingerprint
        <<" road_fingerprint="<<current.roadFingerprint
        <<" forest_triangles="<<current.forestTriangles
        <<" road_triangles="<<current.roadTriangles
        <<" shared_vertices="<<current.sharedVertices
        <<" forest_changed_vertices="<<current.forestChangedVertices
        <<" road_changed_vertices="<<current.roadChangedVertices
        <<" moved="<<moved<<" changed="<<changed<<" boundary="<<a.newBoundaryVertices.size()
        <<" settlement_forest_changed_vertices="<<a.changedForestVertices
        <<" settlement_road_changed_vertices="<<a.changedRoadVertices
        <<" negatives="<<negatives<<'\n';
}
int wmain(int argc,wchar_t** argv)try{
    using namespace Mistwatch;using namespace Aevrynd;
    namespace S=SettlementGround;namespace Q=AuthoredQualification;namespace W=FallenWorldSeat;
    Need(argc==3&&std::wstring_view(argv[1])==L"--runtime-assets","Absolute runtime usage");
    const auto root=std::filesystem::path(argv[2]);Need(root.is_absolute(),"Absolute assets required");
    unsigned profileChecks{},profileNegatives{};
    for(const auto requested:{L"",L"original",L"workshop-ground-domain-v1",L"typo",
        L"entrance-road-material-domain-v1",L"workshop-ground-domain-v2"}){
        Need(S::ResolveProfile(false,requested)==L"workshop-ground-domain-v1","Normal player ground changed by diagnostic residue");++profileChecks;
    }
    Need(S::ResolveProfile(true,L"")==L"original"&&S::ResolveProfile(true,L"original")==L"original"&&
        S::ResolveProfile(true,L"workshop-ground-domain-v1")==L"workshop-ground-domain-v1","Capture selection");
    profileChecks+=3;
    for(const auto requested:{L"typo",L"entrance-road-material-domain-v1",L"workshop-ground-domain-v2"}){
        bool rejected=false;try{(void)S::ResolveProfile(true,requested);}
        catch(const std::invalid_argument& e){
            Need(std::string_view(e.what())=="SETTLEMENT_INVALID_CAPTURE_PROFILE","Wrong profile rejection");
            rejected=true;
        }
        Need(rejected,"Invalid capture profile accepted");++profileNegatives;
    }
    const auto terrain=CreateTerrain();
    const auto simple=CreateTerrainMesh(terrain,true,ForestFloorMaskSelector::ForestWood,BankCoverageSelector::Patchy);
    CheckFixture(simple,"SETTLEMENT_GROUND_SIMPLE_EXPECTED");
    const auto assets=Q::LoadAssets(root);W::Budget budget;
    const auto sourceContext=Q::CreateSourceContext(terrain,assets,budget);std::vector<W::Placement> placements;
    for(const auto& p:FallenLogPlacements()){
        W::Placement q;q.propId=p.propId;q.anchor=p.authoredAnchor;q.pose=p.pose;q.collider=p.collider;
        placements.push_back(q);
    }
    const auto context=Q::CreateContext(terrain,assets,sourceContext,placements,budget);
    Q::ValidateContext(context,terrain,assets,budget);const auto contextSeal=Q::ContextFingerprint(context);
    std::vector<Vec3> finalCanopyRoots;
    for(const auto& plant:context.canopy.plants)finalCanopyRoots.push_back(plant.pose.position);
    Need(context.author.firs.roots.size()==95&&finalCanopyRoots.size()==8,"Actual final author roots");
    auto authored=simple;
    (void)ApplyForestHabitatFloor(authored,terrain,{context.author.firs.roots,finalCanopyRoots});
    CheckFixture(authored,"SETTLEMENT_GROUND_AUTHORED_EXPECTED");
    Need(Q::ContextFingerprint(context)==contextSeal,"Authored world context mutated");
    std::cout<<"SETTLEMENT_GROUND_CPU_PASS fixtures=2 negative_controls=12 profile_checks="<<profileChecks
        <<" profile_negatives="<<profileNegatives<<" context_fingerprint="<<contextSeal<<" native=0 art=0\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
