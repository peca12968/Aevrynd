#pragma once
#include "MistwatchFallenSourceSupport.h"
#include "MistwatchWorld.h"
#include <bit>
#include <functional>
#include <sstream>

namespace Mistwatch::FallenWorldSeat {
namespace Support = ::Mistwatch::FullSourceSupport;
namespace Exact = Aevrynd::ExactTriangleContactDetail3D;
using Q = Exact::Rational;
using Id = std::uint32_t;
using V = Aevrynd::Vec3;
using Pose = Aevrynd::Transform3D;
using Box = Aevrynd::BoxCollider3D;
inline void Gate(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
struct Budget {
    static constexpr std::uint64_t MaximumOperations = 400000000, MaximumPairs = 5000000;
    static constexpr std::uint64_t MaximumContacts = 3000000, MaximumMilliseconds = 600000;
    static constexpr std::size_t MaximumLiveContacts = 250000, MaximumLiveWords = 16777216;
    std::chrono::steady_clock::time_point start{std::chrono::steady_clock::now()};
    std::uint64_t operations{}, pairs{}, contacts{}, evaluations{};
    std::size_t maximumLiveContacts{}, maximumLiveWords{};
    std::uint64_t Elapsed() const { return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count()); }
    void Time() const { Gate(Elapsed()<MaximumMilliseconds,"WORLD_SEAT_TIME_LIMIT_UNRESOLVED"); }
    void Op() { Gate(operations<MaximumOperations,"WORLD_SEAT_OPERATION_LIMIT_UNRESOLVED"); if (!(++operations&255u)) Time(); }
    void Pair() { Gate(pairs<MaximumPairs,"WORLD_SEAT_PAIR_LIMIT_UNRESOLVED"); ++pairs; Op(); }
    void Contact() { Gate(contacts<MaximumContacts,"WORLD_SEAT_CONTACT_LIMIT_UNRESOLVED"); ++contacts; Op(); }
};
inline Q QA(const Q& a,const Q& b,Budget& w){w.Op();return a+b;}
inline Q QS(const Q& a,const Q& b,Budget& w){w.Op();return a-b;}
inline Q QM(const Q& a,const Q& b,Budget& w){w.Op();return a*b;}
inline Q QD(const Q& a,const Q& b,Budget& w){w.Op();return a/b;}
inline int QC(const Q& a,const Q& b,Budget& w){w.Op();return Compare(a,b);}
inline Q QF(float a,Budget& w){w.Op();return Q::Float(a);}
inline std::size_t Words(const Q& a){return a.numerator.words.size()+a.denominator.words.size();}
inline bool InNearestFloatCell(const Q& exact,float stored,Budget& w){
    if(!std::isfinite(stored))return false;const float before=std::nextafter(stored,-(std::numeric_limits<float>::infinity)()),after=std::nextafter(stored,(std::numeric_limits<float>::infinity)());if(!std::isfinite(before)||!std::isfinite(after))return false;
    const Q lower=QD(QA(QF(before,w),QF(stored,w),w),Q(2),w),upper=QD(QA(QF(stored,w),QF(after,w),w),Q(2),w);const int lo=QC(exact,lower,w),hi=QC(exact,upper,w);const bool even=(std::bit_cast<std::uint32_t>(stored)&1u)==0;return(lo>0||(lo==0&&even))&&(hi<0||(hi==0&&even));
}
inline std::array<Q,3> QV(V a,Budget& w){return{{QF(a.x,w),QF(a.y,w),QF(a.z,w)}};}
inline bool SameV(V a,V b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
inline bool SameBox(const Box& a,const Box& b){return SameV(a.center,b.center)&&SameV(a.extents,b.extents);}
inline bool SamePose(const Pose& a,const Pose& b){return SameV(a.position,b.position)&&SameV(a.rotation,b.rotation)&&SameV(a.scale,b.scale);}
inline constexpr std::array<Id,4> CurrentLogIds{{0,8,34,35}};
inline constexpr std::uint64_t CurrentFirFingerprint=8236695966185867318ull;
inline constexpr std::uint64_t CurrentTerrainFingerprint=11561441978197049099ull;
inline constexpr std::uint64_t CurrentPropFingerprint=13639393489331150506ull;
inline constexpr std::array<std::array<std::uint32_t,9>,4> CurrentLogPoseBits{{
    {{0xc0f00000u,0x3e4f6d47u,0x40e00000u,0x3fc90fdau,0x00000000u,0x00000000u,0x3f015038u,0x4080fb8eu,0x3f015038u}},
    {{0x41131dd0u,0x3e5b02afu,0x3df65880u,0x3fc90fdau,0x3e0c812du,0x00000000u,0x3f071aecu,0x406462d2u,0x3f071aecu}},
    {{0xc10ebf78u,0x3e4c83a5u,0x42a8a094u,0x3fc90fdau,0x404b1102u,0x00000000u,0x3effb6cfu,0x4067ab1cu,0x3effb6cfu}},
    {{0xc0dfa578u,0x3e4937c5u,0x42811758u,0x3fc90fdau,0x409a1202u,0x00000000u,0x3efc6aedu,0x40913c7bu,0x3efc6aedu}}
}};
// 與 m1426 原作者觀察使用同一 bit-word byte-wise fingerprint；不是無碰撞雜湊證書。
inline void CohortMix(std::uint64_t& hash,std::uint32_t word){for(unsigned byte=0;byte<4;++byte){hash^=(word>>(byte*8))&255u;hash*=1099511628211ull;}}
inline void CohortMix(std::uint64_t& hash,float value){CohortMix(hash,std::bit_cast<std::uint32_t>(value));}
inline void CohortMix(std::uint64_t& hash,V value){CohortMix(hash,value.x);CohortMix(hash,value.y);CohortMix(hash,value.z);}
inline std::array<std::uint32_t,9> PoseBits(const Pose& pose){return{{std::bit_cast<std::uint32_t>(pose.position.x),std::bit_cast<std::uint32_t>(pose.position.y),std::bit_cast<std::uint32_t>(pose.position.z),std::bit_cast<std::uint32_t>(pose.rotation.x),std::bit_cast<std::uint32_t>(pose.rotation.y),std::bit_cast<std::uint32_t>(pose.rotation.z),std::bit_cast<std::uint32_t>(pose.scale.x),std::bit_cast<std::uint32_t>(pose.scale.y),std::bit_cast<std::uint32_t>(pose.scale.z)}};}
inline std::vector<Id> LogRoles(std::span<const ForestProp> props){Gate(props.size()<=10000,"CURRENT_COHORT_OBSERVATION_DOMAIN");std::vector<Id> result;for(Id id=0;id<props.size();++id)if(props[id].FallenLog())result.push_back(id);return result;}
inline std::uint64_t PropFingerprint(std::span<const ForestProp> props){Gate(props.size()<=10000,"PROP_FINGERPRINT_DOMAIN");std::uint64_t hash=14695981039346656037ull;for(const auto& p:props){CohortMix(hash,static_cast<std::uint32_t>(p.mesh));CohortMix(hash,p.transform.position);CohortMix(hash,p.transform.rotation);CohortMix(hash,p.transform.scale);CohortMix(hash,p.collider.center);CohortMix(hash,p.collider.extents);}return hash;}
inline std::uint64_t TerrainFingerprint(const Aevrynd::HeightField3D& terrain){const auto& g=terrain.Settings();Gate(g.width==201&&g.depth==241&&g.cellSize==.5f&&SameV(g.origin,{-50,0,-20}),"CURRENT_TERRAIN_GRID_IDENTITY_FAILED");std::uint64_t hash=14695981039346656037ull;for(unsigned z=0;z<g.depth;++z)for(unsigned x=0;x<g.width;++x)CohortMix(hash,terrain.Height(x,z));return hash;}
inline void VerifyCurrentTerrain(const Aevrynd::HeightField3D& terrain){Gate(TerrainFingerprint(terrain)==CurrentTerrainFingerprint,"CURRENT_TERRAIN_FINGERPRINT_FAILED");}
inline void VerifyCurrentProps(std::span<const ForestProp> props){
    Gate(props.size()==36,"CURRENT_PROP_COUNT_IDENTITY_FAILED");const auto logs=LogRoles(props);Gate(logs.size()==CurrentLogIds.size()&&std::equal(logs.begin(),logs.end(),CurrentLogIds.begin()),"CURRENT_LOG_COHORT_IDS_FAILED");
    std::size_t rocks{};for(const auto& p:props)if(!p.FallenLog()){Gate(p.mesh<3,"CURRENT_NONLOG_ROLE_FAILED");++rocks;}Gate(rocks==32,"CURRENT_ROCK_COHORT_FAILED");
    for(std::size_t at=0;at<CurrentLogIds.size();++at)Gate(PoseBits(props[CurrentLogIds[at]].transform)==CurrentLogPoseBits[at],"CURRENT_LOG_POSE_BITS_FAILED");
    Gate(PropFingerprint(props)==CurrentPropFingerprint,"CURRENT_PROP_FINGERPRINT_FAILED");
}
inline void VerifyCurrentBaseline(const MatureFirLayout& firs,std::span<const ForestProp> props,const ForestUnderstory& understory,std::span<const BroadleafTree> broadleaf,const Aevrynd::HeightField3D& terrain){
    Gate(firs.reachedBudget&&firs.firs.size()==100&&firs.roots.size()==100&&firs.fingerprint==CurrentFirFingerprint,"CURRENT_FIR_COHORT_FINGERPRINT_FAILED");for(std::size_t id=0;id<firs.roots.size();++id)Gate(SameV(firs.roots[id],firs.firs[id].root),"CURRENT_FIR_ROOT_ROLE_FAILED");
    Gate(understory.saplings.size()==42&&understory.ferns.size()==420&&broadleaf.size()==12,"RETAINED_BASELINE_QUOTAS");VerifyCurrentTerrain(terrain);VerifyCurrentProps(props);
}

inline bool Overlap3(const Box& a,const Box& b){return std::abs(a.center.x-b.center.x)<a.extents.x+b.extents.x&&std::abs(a.center.y-b.center.y)<a.extents.y+b.extents.y&&std::abs(a.center.z-b.center.z)<a.extents.z+b.extents.z;}
inline bool OverlapExact3(const Box& a,const Box& b,Budget& w){const auto ac=QV(a.center,w),ae=QV(a.extents,w),bc=QV(b.center,w),be=QV(b.extents,w);for(unsigned axis=0;axis<3;++axis)if(QC(QA(ac[axis],ae[axis],w),QS(bc[axis],be[axis],w),w)<=0||QC(QA(bc[axis],be[axis],w),QS(ac[axis],ae[axis],w),w)<=0)return false;return true;}
inline bool OverlapXZ(const Box& a,const Box& b,float margin){return std::abs(a.center.x-b.center.x)<a.extents.x+b.extents.x+margin&&std::abs(a.center.z-b.center.z)<a.extents.z+b.extents.z+margin;}

// 只生成一次理想 proper frame；stored Euler round-trip 不成立便拒絕，不試角度。
struct D3 { double x{},y{},z{}; };
inline D3 DAdd(D3 a,D3 b){return{a.x+b.x,a.y+b.y,a.z+b.z};}
inline D3 DScale(D3 a,double s){return{a.x*s,a.y*s,a.z*s};}
inline double DDot(D3 a,D3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline D3 DCross(D3 a,D3 b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline D3 DUnit(D3 a){const double n=std::sqrt(DDot(a,a));Gate(std::isfinite(n)&&n>0,"POSE_SINGULAR_DIRECTION");return DScale(a,1/n);}
inline void VerifyPose(const Pose& pose,Budget& w){
    Gate(Support::Finite(pose.position)&&Support::Finite(pose.rotation)&&Support::Finite(pose.scale)&&pose.scale.x>0&&pose.scale.x==pose.scale.y&&pose.scale.x==pose.scale.z&&pose.scale.x<=64,"POSE_NOT_FINITE_PROPER_UNIFORM");
    const auto r=Aevrynd::CreateRotationMatrix(pose.rotation);
    const auto a=QV({r[0][0],r[1][0],r[2][0]},w),b=QV({r[0][1],r[1][1],r[2][1]},w),c=QV({r[0][2],r[1][2],r[2][2]},w);
    const Q determinant=QA(QS(QM(a[0],QS(QM(b[1],c[2],w),QM(b[2],c[1],w),w),w),QM(a[1],QS(QM(b[0],c[2],w),QM(b[2],c[0],w),w),w),w),QM(a[2],QS(QM(b[0],c[1],w),QM(b[1],c[0],w),w),w),w);
    Gate(determinant.Sign()>0,"POSE_REFLECTION_OR_SINGULAR_FORMAL_MATRIX");
    Gate(std::abs(double(r[1][2]))<1,"POSE_YXZ_GIMBAL_UNRESOLVED");
    // 先解析 pitch，再除回實際 cos(pitch) 因子；保持原角度精確等值，不搜尋或改姿態。
    const float pitch=static_cast<float>(std::atan2(-double(r[1][2]),std::hypot(double(r[1][0]),double(r[1][1]))));
    const float cx=std::cos(pitch);
    Gate(std::isfinite(cx)&&cx>0,"POSE_INVERSE_COMMON_FACTOR_INVALID");
    const float sinZ=r[1][0]/cx,cosZ=r[1][1]/cx;
    Gate(std::isfinite(sinZ)&&std::isfinite(cosZ)&&!(sinZ==0&&cosZ==0),"POSE_INVERSE_FACTOR_PAIR_INVALID");
    const V recovered{pitch,static_cast<float>(std::atan2(double(r[0][2]),double(r[2][2]))),static_cast<float>(std::atan2(double(sinZ),double(cosZ)))};
    Gate(SameV(recovered,pose.rotation),"POSE_STORED_EULER_ROUNDTRIP_UNRESOLVED");
}
inline Pose OnePose(const Aevrynd::Mesh3D& raw,const Support::Plane& winner,const ForestProp& original,Budget& w){
    Gate(original.FallenLog()&&winner.eligible&&winner.xSpan.Sign()>0,"POSE_SOURCE_WINNER_OR_ROLE_INVALID");
    D3 n{-winner.outwardNormal[0].Approximate(),-winner.outwardNormal[1].Approximate(),-winner.outwardNormal[2].Approximate()}; n=DUnit(n);
    const D3 x=DUnit(DAdd({1,0,0},DScale(n,-n.x))),z=DUnit(DCross(x,n));
    const auto old=original.transform.Matrix(); const D3 h=DUnit({double(old[0][1]),0,double(old[2][1])}),k=DCross(h,{0,1,0});
    const std::array<D3,3> columns{{DAdd(DAdd(DScale(h,x.x),{0,n.x,0}),DScale(k,z.x)),DAdd(DAdd(DScale(h,x.y),{0,n.y,0}),DScale(k,z.y)),DAdd(DAdd(DScale(h,x.z),{0,n.z,0}),DScale(k,z.z))}};
    Gate(std::abs(columns[2].y)<1,"POSE_IDEAL_YXZ_GIMBAL_UNRESOLVED");
    Pose pose; pose.rotation={static_cast<float>(std::asin(-columns[2].y)),static_cast<float>(std::atan2(columns[2].x,columns[2].z)),static_cast<float>(std::atan2(columns[0].y,columns[1].y))};
    const float scale=static_cast<float>(double(original.transform.scale.y)/winner.xSpan.Approximate());Gate(InNearestFloatCell(QD(QF(original.transform.scale.y,w),winner.xSpan,w),scale,w),"UNIFORM_SCALE_EXACT_ROUNDING_CELL_UNRESOLVED");pose.scale={scale,scale,scale};
    Gate(!raw.vertices.empty(),"POSE_EMPTY_SOURCE"); V lo=Support::SourceRaw(raw,0),hi=lo;
    for(Id id=0;id<raw.vertices.size();++id){w.Op();const auto p=Support::SourceRaw(raw,id);Gate(Support::Finite(p)&&std::abs(p.x)<=64&&std::abs(p.y)<=64&&std::abs(p.z)<=64,"POSE_RAW_COORDINATE_DOMAIN");lo={(std::min)(lo.x,p.x),(std::min)(lo.y,p.y),(std::min)(lo.z,p.z)};hi={(std::max)(hi.x,p.x),(std::max)(hi.y,p.y),(std::max)(hi.z,p.z)};}
    const V centre=(lo+hi)*.5f; const auto moved=Aevrynd::TransformPoint(pose.Matrix(),centre);
    pose.position={original.transform.position.x-moved.x,0,original.transform.position.z-moved.z}; VerifyPose(pose,w);return pose;
}
inline std::vector<V> ActualPoints(const Aevrynd::Mesh3D& raw,const Pose& pose,Budget& w){
    Gate(raw.vertices.size()<=10000&&!raw.vertices.empty(),"ACTUAL_SOURCE_VERTEX_DOMAIN");
    const auto matrix=pose.Matrix(); std::vector<V> result;result.reserve(raw.vertices.size());
    for(const auto& p:raw.vertices){w.Op();const auto value=Aevrynd::TransformPoint(matrix,{p.x,p.y,p.z});Gate(Support::Finite(value),"ACTUAL_TRANSFORM_NONFINITE");result.push_back(value);}return result;
}

// 在來源三角 barycentric 域裁地形 halfplanes：XZ 投影為點、線、面都不降階。
struct UV {Q u,v;};
inline Q Linear(const std::array<Q,3>& value,const UV& p,Budget& w){return QA(value[0],QA(QM(p.u,QS(value[1],value[0],w),w),QM(p.v,QS(value[2],value[0],w),w),w),w);}
inline Q OrientXZ(const std::array<Q,3>& a,const std::array<Q,3>& b,const std::array<Q,3>& c,Budget& w){return QS(QM(QS(b[0],a[0],w),QS(c[2],a[2],w),w),QM(QS(b[2],a[2],w),QS(c[0],a[0],w),w),w);}
inline std::vector<UV> Clip(const std::vector<UV>& polygon,const std::array<Q,3>& side,Budget& w){
    std::vector<UV> result;if(polygon.empty())return result;
    for(std::size_t id=0;id<polygon.size();++id){w.Op();const auto& a=polygon[id];const auto& b=polygon[(id+1)%polygon.size()];const auto da=Linear(side,a,w),db=Linear(side,b,w);const bool insideA=da.Sign()>=0,insideB=db.Sign()>=0;
        if(insideA)result.push_back(a);if(insideA!=insideB){const auto t=QD(da,QS(da,db,w),w);result.push_back({QA(a.u,QM(t,QS(b.u,a.u,w),w),w),QA(a.v,QM(t,QS(b.v,a.v,w),w),w)});}}
    std::vector<UV> unique;for(const auto& point:result)if(unique.empty()||QC(unique.back().u,point.u,w)||QC(unique.back().v,point.v,w))unique.push_back(point);if(unique.size()>1&&!QC(unique.front().u,unique.back().u,w)&&!QC(unique.front().v,unique.back().v,w))unique.pop_back();
    Gate(unique.size()<=8,"CLIPPED_PARAMETER_POLYGON_BOUND");return unique;
}
struct Constraint {std::array<Id,3> sourceIds{};Id face{},cellX{},cellZ{},terrainHalf{};UV parameter;Q terrainY;};
struct Footprint {std::vector<V> zeroY;std::vector<Constraint> contacts;std::size_t words{},sourceFaces{},nonemptyPairs{};};
inline Footprint MakeFootprint(const Aevrynd::Mesh3D& raw,const Pose& pose,const Aevrynd::HeightField3D& terrain,Budget& w){
    Gate(raw.indices.size()<=50000&&!raw.indices.empty()&&!(raw.indices.size()%3),"FOOTPRINT_SOURCE_INDEX_DOMAIN");
    const auto& grid=terrain.Settings();Gate(grid.width==201&&grid.depth==241&&grid.cellSize==.5f&&SameV(grid.origin,{-50,0,-20}),"FORMAL_TERRAIN_GRID_CONTRACT_CHANGED");
    Pose zero=pose;zero.position.y=0;Footprint result;result.zeroY=ActualPoints(raw,zero,w);
    const auto low=terrain.SamplePosition(0,0),high=terrain.SamplePosition(grid.width-1,grid.depth-1);
    for(const auto p:result.zeroY)Gate(p.x>=low.x&&p.x<=high.x&&p.z>=low.z&&p.z<=high.z,"ACTUAL_SCAN_FOOTPRINT_OUTSIDE_TERRAIN");
    const auto cell=[&](float p,float origin,unsigned bound,int expand){return std::clamp(static_cast<int>(std::floor((double(p)-origin)/grid.cellSize))+expand,0,static_cast<int>(bound)-2);};
    for(std::size_t face=0;face<raw.indices.size()/3;++face){w.Time();++result.sourceFaces;std::array<Id,3> ids;std::array<V,3> positions;std::array<std::array<Q,3>,3> source;
        for(unsigned j=0;j<3;++j){ids[j]=raw.indices[face*3+j];Gate(ids[j]<result.zeroY.size(),"FOOTPRINT_SOURCE_INDEX_OUTSIDE");positions[j]=result.zeroY[ids[j]];source[j]=QV(positions[j],w);}
        float minX=positions[0].x,maxX=minX,minZ=positions[0].z,maxZ=minZ;for(const auto p:positions){minX=(std::min)(minX,p.x);maxX=(std::max)(maxX,p.x);minZ=(std::min)(minZ,p.z);maxZ=(std::max)(maxZ,p.z);}
        bool covered{};
        for(int zi=cell(minZ,grid.origin.z,grid.depth,-1);zi<=cell(maxZ,grid.origin.z,grid.depth,1);++zi)for(int xi=cell(minX,grid.origin.x,grid.width,-1);xi<=cell(maxX,grid.origin.x,grid.width,1);++xi){
            const auto x=static_cast<unsigned>(xi),z=static_cast<unsigned>(zi);const auto a=terrain.SamplePosition(x,z),b=terrain.SamplePosition(x+1,z),c=terrain.SamplePosition(x,z+1),d=terrain.SamplePosition(x+1,z+1);
            // 按真正 CreateMesh directed indices，不以 SampleHeight 的 f32 lerp 代替幾何。
            const std::array<std::array<V,3>,2> triangles{{{{a,c,b}},{{b,c,d}}}};
            for(unsigned half=0;half<2;++half){w.Pair();const auto& tri=triangles[half];
                if(maxX<(std::min)({tri[0].x,tri[1].x,tri[2].x})||minX>(std::max)({tri[0].x,tri[1].x,tri[2].x})||maxZ<(std::min)({tri[0].z,tri[1].z,tri[2].z})||minZ>(std::max)({tri[0].z,tri[1].z,tri[2].z}))continue;
                std::array<std::array<Q,3>,3> ground{{QV(tri[0],w),QV(tri[1],w),QV(tri[2],w)}};const auto determinant=OrientXZ(ground[0],ground[1],ground[2],w);Gate(determinant.Sign()!=0,"DEGENERATE_FORMAL_TERRAIN_TRIANGLE");
                std::vector<UV> polygon{{Q(),Q()},{Q(1),Q()},{Q(),Q(1)}};
                for(unsigned edge=0;edge<3&&!polygon.empty();++edge){std::array<Q,3> side;for(unsigned j=0;j<3;++j){side[j]=OrientXZ(ground[edge],ground[(edge+1)%3],source[j],w);if(determinant.Sign()<0)side[j]=-side[j];}polygon=Clip(polygon,side,w);}
                if(polygon.empty())continue;covered=true;++result.nonemptyPairs;
                std::array<Q,3> heights;for(unsigned j=0;j<3;++j){const auto u=QD(OrientXZ(ground[0],source[j],ground[2],w),determinant,w),v=QD(OrientXZ(ground[0],ground[1],source[j],w),determinant,w);heights[j]=Linear({{ground[0][1],ground[1][1],ground[2][1]}},{u,v},w);}
                for(const auto& uv:polygon){w.Contact();Constraint contact{ids,static_cast<Id>(face),x,z,half,uv,Linear(heights,uv,w)};result.words+=Words(contact.parameter.u)+Words(contact.parameter.v)+Words(contact.terrainY);
                    Gate(result.contacts.size()<Budget::MaximumLiveContacts&&result.words<=Budget::MaximumLiveWords,"FOOTPRINT_CACHE_LIMIT_UNRESOLVED");result.contacts.push_back(std::move(contact));}
            }
        }
        Gate(covered,"SOURCE_TRIANGLE_HAS_NO_TERRAIN_COVERAGE");
    }
    Gate(!result.contacts.empty()&&result.sourceFaces==raw.indices.size()/3,"COMPLETE_FOOTPRINT_NOT_CERTIFIED");
    w.maximumLiveContacts=(std::max)(w.maximumLiveContacts,result.contacts.size());w.maximumLiveWords=(std::max)(w.maximumLiveWords,result.words);return result;
}
struct Range {Q minimum,maximum;std::size_t minimumContact{},zero{},negative{},tested{};bool first{true};};
inline Range Evaluate(const Aevrynd::Mesh3D& raw,const Pose& pose,const Footprint& footprint,Budget& w,std::ostream* certificate=nullptr){
    w.Time();++w.evaluations;const auto points=ActualPoints(raw,pose,w);Gate(points.size()==footprint.zeroY.size(),"EVALUATION_SOURCE_COUNT_CHANGED");std::vector<Q> heights;heights.reserve(points.size());
    for(std::size_t id=0;id<points.size();++id){Gate(points[id].x==footprint.zeroY[id].x&&points[id].z==footprint.zeroY[id].z,"Y_CHANGED_TRUE_XZ_FOOTPRINT");
        Gate(points[id].y==footprint.zeroY[id].y+pose.position.y,"FORMAL_Y_TRANSLATION_EXPRESSION_CHANGED");heights.push_back(QF(points[id].y,w));}
    Range result;for(std::size_t index=0;index<footprint.contacts.size();++index){const auto& c=footprint.contacts[index];const Q gap=QS(Linear({{heights[c.sourceIds[0]],heights[c.sourceIds[1]],heights[c.sourceIds[2]]}},c.parameter,w),c.terrainY,w);
        if(result.first||QC(gap,result.minimum,w)<0){result.minimum=gap;result.minimumContact=index;}if(result.first||QC(gap,result.maximum,w)>0)result.maximum=gap;result.first=false;++result.tested;result.zero+=gap.Sign()==0?1u:0u;result.negative+=gap.Sign()<0?1u:0u;
        if(certificate)*certificate<<index<<' '<<gap.Sign()<<'\n';}
    Gate(!result.first&&result.tested==footprint.contacts.size(),"INCOMPLETE_Y_CERTIFICATE");return result;
}
inline std::uint32_t Ordered(float value){const auto bits=std::bit_cast<std::uint32_t>(value);return(bits&0x80000000u)?~bits:(bits^0x80000000u);}
inline float Unordered(std::uint32_t value){return std::bit_cast<float>((value&0x80000000u)?(value^0x80000000u):~value);}
struct Placement {Id propId{};V anchor{};Pose pose;Box collider;float predecessorY{};Range finalRange,predecessorRange;std::uint32_t evaluations{};};
inline void VerifyCurrentActiveRoles(std::span<const Placement> scan){Gate(scan.size()==CurrentLogIds.size(),"ACTIVE_COHORT_IDENTITY_FAILED");for(std::size_t id=0;id<scan.size();++id)Gate(scan[id].propId==CurrentLogIds[id],"ACTIVE_COHORT_IDENTITY_FAILED");}

inline Box ConservativeBox(std::span<const V> points,Budget& w){
    Gate(!points.empty(),"EMPTY_CONSERVATIVE_BOX");V lo=points.front(),hi=lo;for(const auto p:points){w.Op();Gate(Support::Finite(p),"BOX_NONFINITE_SOURCE");lo={(std::min)(lo.x,p.x),(std::min)(lo.y,p.y),(std::min)(lo.z,p.z)};hi={(std::max)(hi.x,p.x),(std::max)(hi.y,p.y),(std::max)(hi.z,p.z)};}
    Box box;const std::array<float,3> lows{{lo.x,lo.y,lo.z}},highs{{hi.x,hi.y,hi.z}};std::array<float*,3> centres{{&box.center.x,&box.center.y,&box.center.z}},extents{{&box.extents.x,&box.extents.y,&box.extents.z}};
    for(unsigned axis=0;axis<3;++axis){const float centre=static_cast<float>((double(lows[axis])+highs[axis])*.5);float extent=static_cast<float>((std::max)(double(centre)-lows[axis],double(highs[axis])-centre));
        const auto contains=[&]{return centre-extent<=lows[axis]&&centre+extent>=highs[axis]&&QC(QS(QF(centre,w),QF(extent,w),w),QF(lows[axis],w),w)<=0&&QC(QA(QF(centre,w),QF(extent,w),w),QF(highs[axis],w),w)>=0;};
        for(unsigned step=0;step<8&&!contains();++step)extent=std::nextafter(extent,(std::numeric_limits<float>::infinity)());Gate(contains(),"BOX_OUTWARD_ROUNDING_UNRESOLVED");*centres[axis]=centre;*extents[axis]=extent;}
    return box;
}
inline bool ContainsActual(const Box& box,std::span<const V> points){for(const auto p:points)if(p.x<box.center.x-box.extents.x||p.x>box.center.x+box.extents.x||p.y<box.center.y-box.extents.y||p.y>box.center.y+box.extents.y||p.z<box.center.z-box.extents.z||p.z>box.center.z+box.extents.z)return false;return true;}
using Audit = std::function<void(const Placement&,const Footprint&)>;
inline Placement SeatOne(Id id,const Aevrynd::Mesh3D& raw,const Support::Plane& winner,const ForestProp& original,const Aevrynd::HeightField3D& terrain,Budget& w,const Audit& audit){
    Placement result;result.propId=id;result.anchor=original.transform.position;result.pose=OnePose(raw,winner,original,w);const auto footprint=MakeFootprint(raw,result.pose,terrain,w);
    // 單一固定 Y 格點區間。各頂點均是同一常數加 stored Y 的 IEEE 單調函數。
    std::uint32_t lo=Ordered(-1048576.f),hi=Ordered(1048576.f);Pose trial=result.pose;trial.position.y=Unordered(lo);auto below=Evaluate(raw,trial,footprint,w);trial.position.y=Unordered(hi);auto above=Evaluate(raw,trial,footprint,w);result.evaluations=2;
    Gate(below.minimum.Sign()<0&&above.minimum.Sign()>=0,"FINITE_Y_LATTICE_NOT_BRACKETED");
    while(hi-lo>1){w.Time();const auto middle=lo+(hi-lo)/2;trial.position.y=Unordered(middle);const auto range=Evaluate(raw,trial,footprint,w);++result.evaluations;Gate(result.evaluations<=36,"Y_LATTICE_OPERATION_BOUND");if(range.minimum.Sign()<0)lo=middle;else hi=middle;}
    result.pose.position.y=Unordered(hi);result.predecessorY=std::nextafter(result.pose.position.y,-(std::numeric_limits<float>::infinity)());Gate(Ordered(result.predecessorY)==lo,"Y_LATTICE_PREDECESSOR_CHANGED");
    result.finalRange=Evaluate(raw,result.pose,footprint,w);trial=result.pose;trial.position.y=result.predecessorY;result.predecessorRange=Evaluate(raw,trial,footprint,w);result.evaluations+=2;Gate(result.evaluations<=36,"TOTAL_Y_CERTIFICATE_OPERATION_BOUND");
    Gate(result.finalRange.minimum.Sign()>=0&&result.finalRange.negative==0&&result.predecessorRange.minimum.Sign()<0,"ADJACENT_Y_CERTIFICATE_FAILED");
    result.collider=ConservativeBox(ActualPoints(raw,result.pose,w),w);Gate(ContainsActual(result.collider,ActualPoints(raw,result.pose,w)),"SCAN_BOX_NOT_CONSERVATIVE");
    if(audit)audit(result,footprint);return result;
}
enum class Role {Facility,Bridge,MatureFir,ForestProp,Sapling,Broadleaf,Middle,CanopyWood};
struct Solid {Role role{};Id id{};Box box;};
using TypedWorld = std::vector<Solid>;
inline TypedWorld BaseWorld(bool repaired,const MatureFirLayout& layout,std::span<const ForestProp> props,const ForestUnderstory& understory,std::span<const BroadleafTree> broadleaf){
    TypedWorld result;Id facility{};const auto add=[&](std::span<const Box> boxes){for(const auto& box:boxes)result.push_back({Role::Facility,facility++,box});};
    const auto workshop=WorkshopSolids(),hut=RangerHutSolids(),shelter=RepairShelterSolids();add(workshop);const std::array fuse{FuseCaseSolid()};add(fuse);add(hut);add(shelter);const std::array generator{GeneratorSolid()};add(generator);
    for(unsigned id=0;id<3;++id){const auto relay=RelaySolids(id);add(relay);}if(repaired)result.push_back({Role::Bridge,0,{{0,-.2f,36},{2,.2f,4}}});
    for(Id id=0;id<layout.firs.size();++id)result.push_back({Role::MatureFir,id,layout.firs[id].collider});for(Id id=0;id<props.size();++id)result.push_back({Role::ForestProp,id,props[id].collider});
    for(Id id=0;id<understory.saplings.size();++id)result.push_back({Role::Sapling,id,SaplingCollider(understory.saplings[id])});for(Id id=0;id<broadleaf.size();++id)result.push_back({Role::Broadleaf,id,broadleaf[id].trunk});return result;
}
inline std::vector<Box> Boxes(const TypedWorld& world){std::vector<Box> result;result.reserve(world.size());for(const auto& value:world)result.push_back(value.box);return result;}
inline TypedWorld Bind(const TypedWorld& original,bool repaired,std::span<const ForestProp> props,std::span<const Placement> scan){
    Gate(original.size()<=10000,"TYPED_WORLD_DOMAIN");VerifyCurrentProps(props);VerifyCurrentActiveRoles(scan);TypedWorld result=original;std::vector<unsigned> mapped(scan.size());std::array<unsigned,36> originalRoles{};unsigned bridges{};
    for(auto& solid:result){bridges+=solid.role==Role::Bridge?1u:0u;if(solid.role!=Role::ForestProp)continue;Gate(solid.id<props.size()&&SameBox(solid.box,props[solid.id].collider),"TYPED_PROP_ROLE_ORIGINAL_BOX_MISMATCH");++originalRoles[solid.id];if(!props[solid.id].FallenLog())continue;
        const auto found=std::find_if(scan.begin(),scan.end(),[&](const Placement& p){return p.propId==solid.id;});Gate(found!=scan.end(),"MISSING_ACTIVE_LOG_MAPPING");const auto index=static_cast<std::size_t>(found-scan.begin());Gate(!mapped[index]++,"DUPLICATED_ACTIVE_LOG_ROLE");solid.box=found->collider;}
    Gate(bridges==(repaired?1u:0u),"WRONG_BRIDGE_STATE_TYPED_WORLD");for(auto count:mapped)Gate(count==1,"INCOMPLETE_ACTIVE_LOG_MAPPING");for(auto count:originalRoles)Gate(count==1,"ORIGINAL_PROP_ROLE_COHORT_INCOMPLETE");return result;
}
inline void OriginalGuards(const Placement& scan,const Aevrynd::HeightField3D& terrain,std::span<const V> roots,std::span<const ForestProp> props,std::span<const Placement> all,std::span<const Solid> retained,Budget& w){
    const auto& b=scan.collider;Gate(PropClearsObjectives(b),"SCAN_OBJECTIVE_ROAD_RIVER_BRANCH_GUARD_FAILED");
    const float ground=terrain.SampleHeight(scan.anchor.x,scan.anchor.z);float minimum=ground,maximum=ground;
    for(float x:{-1.f,1.f})for(float z:{-1.f,1.f}){const float px=b.center.x+x*b.extents.x,pz=b.center.z+z*b.extents.z;Gate(terrain.Contains(px,pz),"SCAN_BOX_CORNER_OUTSIDE_TERRAIN");const float height=terrain.SampleHeight(px,pz);minimum=(std::min)(minimum,height);maximum=(std::max)(maximum,height);}
    Gate(maximum-minimum<=.15f,"SCAN_ORIGINAL_POINT15_TERRAIN_GUARD_FAILED");Gate(b.center.y-b.extents.y<=ground+.01f&&b.center.y+b.extents.y>=ground,"SCAN_ORIGINAL_BOX_GROUND_RELATION_FAILED");
    for(const auto root:roots){w.Op();Gate(!(std::abs(root.x-b.center.x)<b.extents.x+.6f&&std::abs(root.z-b.center.z)<b.extents.z+.6f),"SCAN_ORIGINAL_TREE_POINT6_GUARD_FAILED");}
    for(Id id=0;id<props.size();++id){w.Op();if(id==scan.propId||props[id].FallenLog())continue;Gate(!OverlapXZ(b,props[id].collider,.7f),"SCAN_ORIGINAL_ROCK_POINT7_GUARD_FAILED");}
    for(const auto& other:all){w.Op();if(other.propId!=scan.propId)Gate(!OverlapXZ(b,other.collider,.7f),"SCAN_ORIGINAL_LOG_POINT7_GUARD_FAILED");}
    for(const auto& value:retained){w.Op();Gate(value.role!=Role::ForestProp,"RETAINED_SET_CONTAINS_REPLACED_PROP_ROLE");if(OverlapExact3(b,value.box,w))throw std::runtime_error("SCAN_RETAINED_FULL_VOLUME_OVERLAP prop="+std::to_string(scan.propId)+" retained_role="+std::to_string(static_cast<unsigned>(value.role))+" retained_id="+std::to_string(value.id));}
}
inline std::vector<Placement> OneCandidate(const Aevrynd::Mesh3D& raw,const Support::Plane& winner,const Aevrynd::HeightField3D& terrain,std::span<const ForestProp> props,std::span<const V> roots,std::span<const Solid> retained,Budget& w,const Audit& audit={}){
    Support::CheckFloatingEnvironment();Gate(roots.size()==100&&retained.size()<=12000,"ONE_CANDIDATE_FORMAL_POPULATION_DOMAIN");VerifyCurrentTerrain(terrain);VerifyCurrentProps(props);const std::size_t logs=CurrentLogIds.size();
    std::vector<Placement> result;result.reserve(logs);for(Id id=0;id<props.size();++id)if(props[id].FallenLog()){w.Time();try{result.push_back(SeatOne(id,raw,winner,props[id],terrain,w,audit));OriginalGuards(result.back(),terrain,roots,props,result,retained,w);}catch(const std::exception& error){throw std::runtime_error("PROP_FIRST_DIVERGENCE id="+std::to_string(id)+" cause="+error.what());}}
    VerifyCurrentActiveRoles(result);for(const auto& scan:result)OriginalGuards(scan,terrain,roots,props,result,retained,w);return result;
}
inline std::vector<ForestProp> VisualProps(std::span<const ForestProp> baseline,std::span<const Placement> scan){VerifyCurrentProps(baseline);VerifyCurrentActiveRoles(scan);std::vector<ForestProp> result(baseline.begin(),baseline.end());for(const auto& p:scan){Gate(p.propId<result.size()&&result[p.propId].FallenLog(),"VISUAL_PROP_ROLE_MISMATCH");result[p.propId].transform=p.pose;result[p.propId].collider=p.collider;}return result;}
}
