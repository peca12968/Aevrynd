// CPU支撐政策負向回歸；實際原生地表另由提交收據驗證，不能冒充完整世界。
#include "MistwatchCoreHerbSupport.h"
#include "MistwatchWestCliffRelief.h"
#include <Aevrynd/MeshLoader.h>
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>

using namespace Aevrynd;
namespace C=Mistwatch::CoreHerbClusters;
namespace S=Mistwatch::CoreHerbSupport;
namespace D=Mistwatch::LandscapeSpeciesDetail;
unsigned checks{},negatives{};
void Need(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
template<class F> void Throws(F&& action,const char* reason){
    try{action();}catch(const std::invalid_argument& error){
        Need(std::string_view(error.what())==reason,"WRONG_NEGATIVE_REJECTION");++negatives;return;
    }
    throw std::runtime_error("EXPECTED_INVALID_INPUT_ACCEPTED");
}
void SameProofGeometry(const S::Proof& optimized,const S::Proof& reference){
    const auto& a=optimized.contact;const auto& b=reference.contact;
    const auto bits=[](float x,float y){return std::bit_cast<std::uint32_t>(x)==std::bit_cast<std::uint32_t>(y);};
    Need(a.safe==b.safe&&a.rejection==b.rejection&&optimized.allFaces==reference.allFaces&&
        optimized.sourceFootPieces==reference.sourceFootPieces&&optimized.windPinnedVertices==reference.windPinnedVertices&&
        optimized.sourceFingerprint==reference.sourceFingerprint&&optimized.derivedFingerprint==reference.derivedFingerprint&&
        optimized.surfaceFingerprint==reference.surfaceFingerprint,"FOOTPRINT_OPTIMIZATION_CHANGED_PROOF_IDENTITY_OR_DECISION");
    Need(bits(a.minimumGapBound,b.minimumGapBound)&&bits(a.maximumFootGapBound,b.maximumFootGapBound)&&
        bits(a.minimumNormalY,b.minimumNormalY)&&bits(a.maximumHeightDelta,b.maximumHeightDelta)&&
        a.footprintFaces==b.footprintFaces&&a.evaluatedFaces==b.evaluatedFaces&&a.skippedFaces==b.skippedFaces&&
        a.footPieces==b.footPieces,"FOOTPRINT_OPTIMIZATION_CHANGED_GEOMETRY_OR_FACE_ACCOUNTING");
    // cachedGroundFaces／groundHeightPlanes／aabbRejected僅記實際操作量，不混入幾何等價門。
}
void SamePreparedClip(std::span<const S::Detail::Tagged> face,const D::R& limit){
    std::vector<S::Detail::Tagged> input(face.begin(),face.end());
    std::vector<D::R> deltas;std::vector<int> signs;deltas.reserve(input.size());signs.reserve(input.size());
    for(const auto& point:input){deltas.push_back(point.authorHeight-limit);signs.push_back(deltas.back().Sign());}
    const auto original=S::Detail::Foot(input,limit),prepared=S::Detail::FootPrepared(input,deltas,signs,limit);
    Need(original.size()==prepared.size(),"PREPARED_FOOT_CLIP_CHANGED_POINT_COUNT_OR_DUPLICATES");
    for(unsigned i=0;i<original.size();++i)
        Need(D::Exact::Equal(original[i].actual.point,prepared[i].actual.point)&&
            Compare(original[i].actual.wind,prepared[i].actual.wind)==0&&
            Compare(original[i].authorHeight,prepared[i].authorHeight)==0,"PREPARED_FOOT_CLIP_CHANGED_EXACT_POINT_OR_ORDER");
}
void ClipCases(){
    // 根帶由來源相對高度定義，不受世界Y、坡面方向或形變後高度重新分類。
    const auto limit=D::R::Float(C::RootPinnedHeight),twice=limit*D::R(2);
    const std::array<S::Detail::Tagged,3> triangle{{
        {{{D::R{},D::R(10),D::R{}},D::R{}},D::R{}},
        {{{D::R(1),D::R{}-D::R(10),D::R{}},D::R(1)},twice},
        {{{D::R{},D::R{},D::R(1)},D::R(1)},twice}}};
    const auto clipped=S::Detail::Foot(triangle,limit);
    Need(clipped.size()==3,"SOURCE_FOOT_CLIP_PIECES");
    unsigned below{},edges{};
    for(const auto& point:clipped){
        Need(Compare(point.authorHeight,limit)<=0,"SOURCE_FOOT_CLIP_HEIGHT");
        if(point.authorHeight.Sign()==0){++below;Need(D::Exact::Equal(point.actual.point,triangle[0].actual.point),"SOURCE_FOOT_ORIGIN");}
        else{++edges;Need(Compare(point.authorHeight,limit)==0&&Compare(point.actual.wind,D::R(1)/D::R(2))==0,"SOURCE_FOOT_EXACT_EDGE_WEIGHT");
            Need(Compare(point.actual.point[0]+point.actual.point[2],D::R(1)/D::R(2))==0,"SOURCE_FOOT_EXACT_EDGE_POSITION");}
    }
    Need(below==1&&edges==2,"SOURCE_FOOT_CLIP_IDENTITY");
    auto moved=triangle;for(auto& p:moved)p.actual.point[1]=p.actual.point[1]+D::R(100);
    const auto elevated=S::Detail::Foot(moved,limit);Need(elevated.size()==clipped.size(),"SOURCE_FOOT_WORLD_HEIGHT_RECLASSIFIED");
    for(unsigned i=0;i<clipped.size();++i){
        Need(Compare(elevated[i].authorHeight,clipped[i].authorHeight)==0&&
             Compare(elevated[i].actual.wind,clipped[i].actual.wind)==0&&
             Compare(elevated[i].actual.point[0],clipped[i].actual.point[0])==0&&
             Compare(elevated[i].actual.point[2],clipped[i].actual.point[2])==0&&
             Compare(elevated[i].actual.point[1]-clipped[i].actual.point[1],D::R(100))==0,"SOURCE_FOOT_CLIP_TRANSLATION");
    }
    auto above=triangle;for(auto& p:above)p.authorHeight=limit+D::R(1);
    Need(S::Detail::Foot(above,limit).empty(),"SOURCE_FOOT_ABOVE_NOT_EMPTY");
    auto boundary=triangle;for(auto& p:boundary)p.authorHeight=limit;
    Need(S::Detail::Foot(boundary,limit).size()==3,"SOURCE_FOOT_EQUAL_BOUNDARY_LOST");
    SamePreparedClip(triangle,limit);SamePreparedClip(moved,limit);SamePreparedClip(above,limit);SamePreparedClip(boundary,limit);
    auto allBelow=triangle;for(auto& point:allBelow)point.authorHeight=D::R{};
    SamePreparedClip(allBelow,limit);
    // 恰位於切線的點會由原Foot重複加入；不得因預算sign而把退化的根點／根邊去重。
    auto oneBoundary=above;oneBoundary[0].authorHeight=limit;
    const auto repeatedPoint=S::Detail::Foot(oneBoundary,limit);
    Need(repeatedPoint.size()==3&&D::Exact::Equal(repeatedPoint[0].actual.point,repeatedPoint[1].actual.point)&&
        D::Exact::Equal(repeatedPoint[1].actual.point,repeatedPoint[2].actual.point),"ORIGINAL_FOOT_DEGENERATE_POINT_CONTRACT");
    SamePreparedClip(oneBoundary,limit);
    auto twoBoundary=oneBoundary;twoBoundary[1].authorHeight=limit;
    Need(S::Detail::Foot(twoBoundary,limit).size()==4,"ORIGINAL_FOOT_DEGENERATE_EDGE_CONTRACT");
    SamePreparedClip(twoBoundary,limit);
    auto mixedQuad=triangle;mixedQuad[1].authorHeight=D::R{};
    Need(S::Detail::Foot(mixedQuad,limit).size()==4,"ORIGINAL_FOOT_MIXED_QUAD_CONTRACT");
    for(unsigned first=0;first<3;++first){const std::array<S::Detail::Tagged,3> rotated{
        mixedQuad[first],mixedQuad[(first+1)%3],mixedQuad[(first+2)%3]};SamePreparedClip(rotated,limit);}
    ++negatives;
}
void PlaneBoundsCases(){
    Mesh3D control;for(const auto point:{Vec3{-1,0,-1},Vec3{1,.125f,-1},Vec3{1,0,1},Vec3{-1,.125f,1}}){
        Vertex3D v{};v.x=point.x;v.y=point.y;v.z=point.z;control.vertices.push_back(v);}
    // 同一8m broad-phase cell的遠面必須被新footprint模式排除，但不能遺失邊／點接觸。
    for(const auto point:{Vec3{6,.5f,6},Vec3{7,.5f,6},Vec3{6,.5f,7}}){
        Vertex3D vertex{};vertex.x=point.x;vertex.y=point.y;vertex.z=point.z;control.vertices.push_back(vertex);}
    control.indices={0,2,1,0,3,2,4,6,5};auto raised=control;raised.indices={0,3,1,1,3,2,4,6,5};
    const D::Surface ac(control),bd(raised);const auto source=D::Triangle(control,0);
    const std::array<D::P,4> footprint{D::Exact::ToExact(Vec3{-1,0,-1}),D::Exact::ToExact(Vec3{1,0,-1}),
        D::Exact::ToExact(Vec3{1,0,1}),D::Exact::ToExact(Vec3{-1,0,1})};
    Mistwatch::OuterUnderstory::SupportProof pa,pb,paLocal,pbLocal;
    Mistwatch::OuterUnderstory::Detail::GroundCache ca(ac,footprint,pa),cb(bd,footprint,pb);
    Mistwatch::OuterUnderstory::Detail::GroundCache caLocal(ac,footprint,paLocal,true),cbLocal(bd,footprint,pbLocal,true);
    Need(paLocal.cachedGroundFaces<pa.cachedGroundFaces&&pbLocal.cachedGroundFaces<pb.cachedGroundFaces&&
        paLocal.groundHeightPlanes<pa.groundHeightPlanes&&pbLocal.groundHeightPlanes<pb.groundHeightPlanes,
        "SAME_CELL_FAR_GROUND_NOT_FILTERED");
    for(const auto& vertex:source)Need(Compare(vertex[1]-bd.At({static_cast<float>(vertex[0].Approximate()),
        static_cast<float>(vertex[1].Approximate()),static_cast<float>(vertex[2].Approximate())}),D::R{})==0,
        "MIDSPAN_NEGATIVE_VERTEX_NOT_ZERO");
    const auto flat=ca.Overlay(source),hill=cb.Overlay(source),bf=ca.ConservativeRange(source),bh=cb.ConservativeRange(source);
    Need(flat.points&&hill.points&&bf.points&&bh.points,"PLANE_BOUND_EMPTY");
    Need(Compare(flat.minimum,D::R{})==0&&Compare(flat.maximum,D::R{})==0&&Compare(bf.minimum,D::R{})==0,
        "CONTROL_AC_GROUND_NOT_ZERO");
    Need(Compare(hill.minimum,-D::R::Float(.125f))==0&&Compare(bh.minimum,-D::R::Float(.25f))==0&&
        Compare(bh.minimum,-D::R::Float(.04f))<0,"MIDSPAN_PENETRATION_PRUNED");++negatives;
    Need(Compare(bf.maximum,D::R::Float(.04f))>0,"LOOSE_UPPER_BOUND_MUST_FALLBACK");
    const std::array<D::T,4> shapes{{source,{source[0],source[2],source[2]},
        {source[0],D::Exact::ToExact(Vec3{-1,1,-1}),source[1]},
        {D::Exact::ToExact(Vec3{1,0,1}),D::Exact::ToExact(Vec3{1,1,1}),D::Exact::ToExact(Vec3{1,2,1})}}};
    for(const auto& shape:shapes)for(const auto& delta:{D::R{},D::R::Float(.011f),-D::R::Float(.19f)}){
        auto shifted=shape;for(unsigned i=0;i<3;++i)shifted[i][1]=shifted[i][1]+delta*D::R(i)/D::R(2);
        for(unsigned policy=0;policy<2;++policy){auto& cache=policy?cb:ca;auto& local=policy?cbLocal:caLocal;const auto& surface=policy?bd:ac;
            const auto bound=cache.ConservativeRange(shifted),exact=cache.Overlay(shifted),reference=D::Overlay(shifted,surface);
            const auto localBound=local.ConservativeRange(shifted),localExact=local.Overlay(shifted);
            Need(exact.points&&reference.points&&bound.points,"BOUND_EDGE_OR_DEGENERATE_EMPTY");
            Need(Compare(exact.minimum,reference.minimum)==0&&Compare(exact.maximum,reference.maximum)==0,
                "CACHE_ORACLE_DIFFERENCE");
            Need(Compare(bound.minimum,exact.minimum)<=0&&Compare(bound.maximum,exact.maximum)>=0,"BOUND_NOT_CONSERVATIVE");
            Need(localBound.points==bound.points&&localExact.points==exact.points&&
                Compare(localBound.minimum,bound.minimum)==0&&Compare(localBound.maximum,bound.maximum)==0&&
                Compare(localExact.minimum,exact.minimum)==0&&Compare(localExact.maximum,exact.maximum)==0,
                "FOOTPRINT_CACHE_CHANGED_TOUCH_DEGENERATE_OR_MIDSPAN_RANGE");
        }
    }
    const D::T outside{D::Exact::ToExact(Vec3{20,0,20}),D::Exact::ToExact(Vec3{21,0,20}),D::Exact::ToExact(Vec3{20,0,21})};
    Need(!ca.ConservativeRange(outside).points&&!ca.Overlay(outside).points,"EMPTY_BOUND_IS_NOT_PROOF");++negatives;
    Throws([&]{(void)caLocal.ConservativeRange(outside);},"OUTER_PLANTS_CACHE_QUERY_OUTSIDE_FOOTPRINT");
    Throws([&]{(void)caLocal.Overlay(outside);},"OUTER_PLANTS_CACHE_QUERY_OUTSIDE_FOOTPRINT");
    auto onlyFar=control;onlyFar.indices={4,6,5};const D::Surface absent(onlyFar);
    Mistwatch::OuterUnderstory::SupportProof emptyProof;
    Mistwatch::OuterUnderstory::Detail::GroundCache empty(absent,footprint,emptyProof,true);
    Need(!emptyProof.cachedGroundFaces&&!empty.ConservativeRange(source).points&&!empty.Overlay(source).points,
        "IN_FOOTPRINT_EMPTY_CACHE_FABRICATED_SUPPORT");++negatives;
}
int main(int argc,char** argv){try{
    if(argc!=3||std::string_view(argv[1])!="--runtime-assets")throw std::runtime_error("absolute runtime assets required");
    const std::filesystem::path assets(argv[2]);
    Need(assets.is_absolute(),"ABSOLUTE_FIXTURE_PATHS_REQUIRED");
    std::array<Mesh3D,2> source;C::Sources pointers;
    for(unsigned slot=0;slot<2;++slot){
        auto loaded=LoadAevryndMeshFile(assets/"mistwatch-nettle/meshes"/Mistwatch::NettleMeshName(slot+2));
        Need(bool(loaded),"NORMALIZED_SOURCE_MISSING");Mistwatch::NormalizeNettleAsset(loaded.mesh,slot+2);
        source[slot]=std::move(loaded.mesh);pointers[slot]=&source[slot];
        Need(C::Detail::MeshFingerprint(source[slot])==C::SourceFingerprints[slot],"NORMALIZED_SOURCE_SEAL");
    }
    const auto terrain=Mistwatch::WestCliffRelief::Build(Mistwatch::CreateTerrain()).terrain;
    const auto ground=Mistwatch::CreateTerrainMesh(terrain,false);
    const auto seal=Mistwatch::OuterUnderstory::Detail::MeshFingerprint(ground);const D::Surface surface(ground);
    const std::array<BoxCollider3D,1> outsideFixture{{{{4000,4000,4000},{1,1,1}}}};
    // 只檢查真地表的支撐政策，刻意不宣稱此配置fixture是完整世界驗收。
    const auto plan=C::Create(pointers,terrain,outsideFixture,outsideFixture);
    Need(plan.complete&&plan.plants.size()==C::Target&&!plan.groupedWorldVerified&&!plan.wholeSupportProven,"FIXTURE_COMPLETENESS_OR_SCOPE");
    std::array<std::optional<unsigned>,2> selected;
    for(unsigned ordinal=0;ordinal<(std::min)(16u,static_cast<unsigned>(plan.plants.size()));++ordinal){
        const auto& plant=plan.plants[ordinal];const unsigned slot=plant.variant-2;
        if(selected[slot])continue;
        const auto proof=S::Check(surface,source[slot],plant.derivedMesh,plant.pose,plant.root,plant.radius,seal);
        if(proof.contact.safe)selected[slot]=ordinal;
    }
    for(unsigned slot=0;slot<2;++slot){
        Need(selected[slot].has_value(),"FIRST_16_REPRESENTATIVE_NOT_SAFE");const auto& p=plan.plants[*selected[slot]];
        const auto check=[&](const Mesh3D& raw,const Mesh3D& mesh,std::uint64_t identity=0){
            return S::Check(surface,raw,mesh,p.pose,p.root,p.radius,identity?identity:seal);
        };
        const auto normal=check(source[slot],p.derivedMesh);
        const auto broadReference=S::Check(surface,source[slot],p.derivedMesh,p.pose,p.root,p.radius,seal,true,nullptr,false);
        SameProofGeometry(normal,broadReference);
        SameProofGeometry(normal,S::Check(surface,source[slot],p.derivedMesh,p.pose,p.root,p.radius,seal,true,nullptr,true,false));
        const auto reference=S::Check(surface,source[slot],p.derivedMesh,p.pose,p.root,p.radius,seal,false);
        Need(reference.contact.safe==normal.contact.safe&&reference.allFaces==normal.allFaces&&
            reference.sourceFootPieces==normal.sourceFootPieces,"BOUNDS_BASELINE_DECISION_DIFFERENT");
        Need(normal.contact.safe&&normal.allFaces==p.derivedMesh.indices.size()/3&&normal.sourceFootPieces>0&&normal.windPinnedVertices>0,
            "REPRESENTATIVE_INCOMPLETE_SAFE_PROOF");
        Need(normal.sourceFingerprint==C::SourceFingerprints[slot]&&normal.derivedFingerprint==p.derivedFingerprint&&normal.surfaceFingerprint==seal,
            "REPRESENTATIVE_PROOF_IDENTITY");
        auto floating=p.derivedMesh;for(auto& vertex:floating.vertices)vertex.y+=.12f/p.pose.scale.x;
        const auto floated=check(source[slot],floating);
        SameProofGeometry(floated,S::Check(surface,source[slot],floating,p.pose,p.root,p.radius,seal,true,nullptr,false));
        SameProofGeometry(floated,S::Check(surface,source[slot],floating,p.pose,p.root,p.radius,seal,true,nullptr,true,false));
        Need(!floated.contact.safe&&floated.contact.rejection==Mistwatch::OuterUnderstory::Reject::Foot,"FLOATING_ROOT_ACCEPTED");++negatives;
        auto buried=p.derivedMesh;for(auto& vertex:buried.vertices)vertex.y-=.12f/p.pose.scale.x;
        const auto sunk=check(source[slot],buried);
        SameProofGeometry(sunk,S::Check(surface,source[slot],buried,p.pose,p.root,p.radius,seal,true,nullptr,false));
        SameProofGeometry(sunk,S::Check(surface,source[slot],buried,p.pose,p.root,p.radius,seal,true,nullptr,true,false));
        Need(!sunk.contact.safe&&sunk.contact.minimumGapBound<-Mistwatch::OuterUnderstory::ContactLimit,"BURIED_ALL_FACES_ACCEPTED");++negatives;
        auto wind=p.derivedMesh;wind.vertices.front().normal^=0x01000000u;
        Throws([&]{(void)check(source[slot],wind);},"CORE_HERB_SUPPORT_WIND_IDENTITY");
        auto raw=source[slot];raw.vertices.front().color^=1u;
        Throws([&]{(void)check(raw,p.derivedMesh);},"CORE_HERB_SUPPORT_SOURCE_IDENTITY");
        auto xz=p.derivedMesh;xz.vertices.front().x=std::nextafter(xz.vertices.front().x,std::numeric_limits<float>::infinity());
        Throws([&]{(void)check(source[slot],xz);},"CORE_HERB_SUPPORT_MATERIAL_OR_XZ_CHANGED");
        auto uv=p.derivedMesh;uv.vertices.front().u=std::nextafter(uv.vertices.front().u,std::numeric_limits<float>::infinity());
        Throws([&]{(void)check(source[slot],uv);},"CORE_HERB_SUPPORT_MATERIAL_OR_XZ_CHANGED");
        auto topology=p.derivedMesh;std::swap(topology.indices[0],topology.indices[1]);
        Throws([&]{(void)check(source[slot],topology);},"CORE_HERB_SUPPORT_INPUT");
        Throws([&]{(void)S::Check(surface,source[slot],p.derivedMesh,p.pose,p.root,p.radius,0);},"CORE_HERB_SUPPORT_INPUT");
        std::cout<<"REPRESENTATIVE variant="<<slot+2<<" ordinal="<<*selected[slot]<<" all_faces="<<normal.allFaces
            <<" source_foot_pieces="<<normal.sourceFootPieces<<" floating_rejected=1 buried_rejected=1"
            <<" ground_cache_optimized="<<normal.contact.cachedGroundFaces<<" ground_cache_reference="<<broadReference.contact.cachedGroundFaces
            <<" ground_planes_optimized="<<normal.contact.groundHeightPlanes<<" ground_planes_reference="<<broadReference.contact.groundHeightPlanes<<'\n';
    }
    ClipCases();PlaneBoundsCases();
    std::cout<<"CORE_HERB_SUPPORT_NEGATIVE checks="<<checks<<" negatives="<<negatives
        <<" variants=2 source_foot_exact_clip=1 vertex_reuse_equivalence=1 fixture_not_full_world=1 native_pass=0 art_pass=0\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"CORE_HERB_SUPPORT_NEGATIVE_FAIL "<<error.what()<<'\n';return 1;}}
