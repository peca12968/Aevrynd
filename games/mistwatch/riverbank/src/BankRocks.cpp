#include "MistwatchBankRocks.h"
#include "MistwatchRiverProfile.h"
#include "MistwatchStormRiver.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
namespace Mistwatch::BankRocks {
namespace {
struct Anchor {float x;int side;float scale,yaw,tilt;};
constexpr std::array<Anchor,Count> Authored{{
    {-6.3f,-1,2.1f,.48f,.06f},{-9.1f,-1,1.7f,2.12f,-.07f},
    {6.4f,-1,1.9f,1.46f,-.04f},{9.5f,-1,2.3f,2.83f,.08f},
    {-6.7f,1,1.8f,2.58f,-.05f},{-10.1f,1,2.2f,.87f,.06f},
    {6.8f,1,2.4f,1.86f,.04f},{10.4f,1,1.7f,.18f,-.06f}}};
bool Overlap(const Aevrynd::BoxCollider3D&a,const Aevrynd::BoxCollider3D&b){
    return std::abs(double(a.center.x)-b.center.x)<double(a.extents.x)+b.extents.x&&
           std::abs(double(a.center.y)-b.center.y)<double(a.extents.y)+b.extents.y&&
           std::abs(double(a.center.z)-b.center.z)<double(a.extents.z)+b.extents.z;
}
float Extent(double value){float result=static_cast<float>(value);if(double(result)<value)result=std::nextafter(result,std::numeric_limits<float>::infinity());return result;}
}
static Model BuildCore(const Aevrynd::HeightField3D&field,const Aevrynd::Mesh3D&mesh,
            std::span<const RiverbankConstruction::Guard> guards,std::span<const Aevrynd::BoxCollider3D> solids,bool rejectedPreview){
    using namespace Aevrynd;
    if(mesh.vertices.size()!=81373||mesh.indices.size()!=198366||guards.size()!=223||solids.size()!=47014)
        throw std::invalid_argument("BANK_ROCKS_EXACT_CURRENT_MESH_AND_SOURCE_REQUIRED");
    Model result;
    const auto bounds=mesh.Bounds();const float low=bounds.center.y-bounds.extents.y;
    const float supportTop=low+bounds.extents.y*.24f;
    std::vector<std::uint32_t> supportIds;
    for(std::uint32_t i=0;i<mesh.vertices.size();++i)if(mesh.vertices[i].y<=supportTop)supportIds.push_back(i);
    unsigned number{};
    for(const auto&a:Authored){
        const auto fail=[&](const char* reason){throw std::runtime_error(std::string(reason)+":"+std::to_string(number));};
        const float center=RiverCenterZ(a.x),level=StormRiver::Height+.12f;
        float inner=0,outer=0;bool found=false;
        for(unsigned step=1;step<=64;++step){outer=float(step)*.25f;
            if(!field.Contains(a.x,center+float(a.side)*outer))fail("BANK_ROCK_SHORE_OUTSIDE_FIELD");
            if(field.SampleHeight(a.x,center+float(a.side)*outer)>=level){found=true;break;}inner=outer;
        }
        if(!found)fail("BANK_ROCK_SHORE_NOT_FOUND");
        for(unsigned step=0;step<24;++step){const float mid=(inner+outer)*.5f;
            if(field.SampleHeight(a.x,center+float(a.side)*mid)<level)inner=mid;else outer=mid;
        }
        Part part;part.pose={{a.x,0,center+float(a.side)*(inner+outer)*.5f},{a.tilt,a.yaw,0},{a.scale,a.scale,a.scale}};
        // First method's upright source foot cannot follow the steep bank.
        // Build a tangent rest frame from this actual final surface, preserving
        // the authored yaw/scale/anchor instead of shrinking or shifting it.
        const auto n=field.SampleNormal(part.pose.position.x,part.pose.position.z);
        // The source's long horizontal axis is local Z. Merely matching its
        // up vector left that long axis pointing down the bank; ground-relative
        // exposure failed at1.998384m. Align the full tangent frame instead:
        // localZ follows an actual horizontal contour, localX crosses slope.
        const auto contour=Normalize(Cross(Vec3{0,1,0},n));
        const float yaw=Length(contour)>.5f?std::atan2(contour.x,contour.z)+(number%2?3.14159265358979323846f:0.f):a.yaw;
        const float cy=std::cos(yaw),sy=std::sin(yaw);
        const Vec3 localNormal{cy*n.x-sy*n.z,n.y,sy*n.x+cy*n.z};
        part.pose.rotation={std::atan2(localNormal.z,localNormal.y)+a.tilt,yaw,-std::asin(std::clamp(localNormal.x,-1.f,1.f))};
        auto matrix=part.pose.Matrix();
        const auto actualUp=Normalize(TransformPoint(matrix,{0,1,0})-TransformPoint(matrix,{0,0,0}));
        if(Dot(actualUp,n)<.995f)fail("BANK_ROCK_ACTUAL_BED_FRAME_MISMATCH");
        float seat=std::numeric_limits<float>::infinity();
        for(const auto&v:mesh.vertices)if(v.y<=supportTop){
            const auto p=TransformPoint(matrix,{v.x,v.y,v.z});
            if(!field.Contains(p.x,p.z))fail("BANK_ROCK_SUPPORT_OUTSIDE_FIELD");
            seat=(std::min)(seat,field.SampleHeight(p.x,p.z)-p.y-.06f);++part.supportVertices;
        }
        if(!std::isfinite(seat)||part.supportVertices<100)fail("BANK_ROCK_INCOMPLETE_LOWER_SUPPORT");
        part.pose.position.y=seat;matrix=part.pose.Matrix();
        Vec3 minimum{1.e6f,1.e6f,1.e6f},maximum{-1.e6f,-1.e6f,-1.e6f};part.maximumSupportGap=-1.e6f;
        for(const auto&v:mesh.vertices){const auto p=TransformPoint(matrix,{v.x,v.y,v.z});
            if(!field.Contains(p.x,p.z))fail("BANK_ROCK_VERTEX_OUTSIDE_FIELD");
            const float gap=p.y-field.SampleHeight(p.x,p.z);part.maximumAbove=(std::max)(part.maximumAbove,gap);
            if(v.y<=supportTop)part.maximumSupportGap=(std::max)(part.maximumSupportGap,gap);
            minimum={(std::min)(minimum.x,p.x),(std::min)(minimum.y,p.y),(std::min)(minimum.z,p.z)};
            maximum={(std::max)(maximum.x,p.x),(std::max)(maximum.y,p.y),(std::max)(maximum.z,p.z)};
        }
        if(part.maximumSupportGap>-.0599f||part.maximumAbove<.18f)
            fail("BANK_ROCK_FLOATING_OR_INVISIBLE_SEATING");
        if(part.maximumAbove>1.8f){
            if(!rejectedPreview)throw std::runtime_error("BANK_ROCK_FLOATING_OR_INVISIBLE_SEATING:"+std::to_string(number)+":support_gap="+std::to_string(part.maximumSupportGap)+":visible="+std::to_string(part.maximumAbove));
            ++result.rejectedHeightParts; // Known FAILED gate, never passes Build.
        }
        auto&box=part.collider;box.center=(minimum+maximum)*.5f;
        box.extents={Extent((std::max)(double(box.center.x)-minimum.x,double(maximum.x)-box.center.x)),
                     Extent((std::max)(double(box.center.y)-minimum.y,double(maximum.y)-box.center.y)),
                     Extent((std::max)(double(box.center.z)-minimum.z,double(maximum.z)-box.center.z))};
        if(std::abs(box.center.x)-box.extents.x<4.5f)fail("BANK_ROCK_MAIN_CORRIDOR_INTERSECTION");
        for(const auto&g:guards)if(std::abs(box.center.x-g.x)<box.extents.x+g.ex&&std::abs(box.center.z-g.z)<box.extents.z+g.ez)fail("BANK_ROCK_PROTECTED_ROOT_OR_SOLID_DOMAIN");
        for(const auto&other:solids)if(Overlap(box,other))fail("BANK_ROCK_EXISTING_SOLID_INTERSECTION");
        for(const auto&other:result.additions)if(Overlap(box,other))fail("BANK_ROCK_MUTUAL_COLLIDER_INTERSECTION");
        RigidTerrainMetrics3D measured;
        if(!TryMeasureRigidTerrainMetrics3D(mesh,part.pose,field,supportIds,StormRiver::Height,measured)||
           measured.supportVertexCount!=part.supportVertices||measured.maximumSupportGap>-.0599||
           measured.worldHeight>1.8||measured.maximumEmergence<.18||measured.maximumEmergence>1.8)
            fail("BANK_ROCK_PREVIEW_SEPARATE_SOURCE_METRICS_FAILED");
        result.parts.push_back(part);result.additions.push_back(box);result.measurements.push_back(measured);++number;
    }
    if(result.parts.size()!=Count)throw std::runtime_error("BANK_ROCK_INCOMPLETE_AUTHORED_COMPOSITION");
    return result;
}
Model Build(const Aevrynd::HeightField3D&f,const Aevrynd::Mesh3D&m,std::span<const RiverbankConstruction::Guard>g,std::span<const Aevrynd::BoxCollider3D>s){return BuildCore(f,m,g,s,false);}
Model BuildRejectedPreview(const Aevrynd::HeightField3D&f,const Aevrynd::Mesh3D&m,std::span<const RiverbankConstruction::Guard>g,std::span<const Aevrynd::BoxCollider3D>s){return BuildCore(f,m,g,s,true);}
}
