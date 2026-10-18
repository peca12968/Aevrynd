#pragma once
#include "MistwatchWorld.h"
#include "MistwatchSettlementGround.h"
#include <bit>
#include <limits>
#include <sstream>
#include <string_view>

namespace Mistwatch::WestCliffRelief {
inline constexpr std::string_view Profile="west-cliff-ledges-v1";
inline constexpr float MaximumCut=1.5f;
// 包含半公尺三角面及法線鄰域的保守範圍；場景支撐物仍須由呼叫端另行驗證。
inline constexpr float GuardMinX=-49.5f,GuardMaxX=-40.5f,GuardMinZ=50.5f,GuardMaxZ=65.5f;
struct Receipt {
    std::uint64_t canonicalFingerprint{},derivedFingerprint{};
    std::size_t changedSamples{},changedNormals{};
    std::size_t protectedCellsVerified{};
    float maximumCut{},minimumHeight{},maximumHeight{};
};
struct Result {Aevrynd::HeightField3D terrain;Receipt receipt;};
struct MeshReceipt {std::size_t changedPositions{},changedNormals{},changedColors{},changedVertices{};};
inline bool Bits(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
inline bool SameLayout(const Aevrynd::HeightField3D& a,const Aevrynd::HeightField3D& b){
    const auto& x=a.Settings();const auto& y=b.Settings();
    return x.width==y.width&&x.depth==y.depth&&Bits(x.cellSize,y.cellSize)&&
        Bits(x.origin.x,y.origin.x)&&Bits(x.origin.y,y.origin.y)&&Bits(x.origin.z,y.origin.z)&&a.SampleCount()==b.SampleCount();
}
inline bool SameExact(const Aevrynd::HeightField3D& a,const Aevrynd::HeightField3D& b){
    if(!SameLayout(a,b)||!Bits(a.MinimumHeight(),b.MinimumHeight())||!Bits(a.MaximumHeight(),b.MaximumHeight()))return false;
    for(unsigned z=0;z<a.Settings().depth;++z)for(unsigned x=0;x<a.Settings().width;++x)
        if(!std::isfinite(a.Height(x,z))||!std::isfinite(b.Height(x,z))||!Bits(a.Height(x,z),b.Height(x,z)))return false;
    return true;
}
inline std::uint64_t Fingerprint(const Aevrynd::HeightField3D& field){
    std::uint64_t hash=14695981039346656037ull;
    const auto word=[&](std::uint32_t value){for(unsigned i=0;i<4;++i){hash^=(value>>(i*8))&255u;hash*=1099511628211ull;}};
    const auto scalar=[&](float value){word(std::bit_cast<std::uint32_t>(value));};
    const auto& s=field.Settings();word(s.width);word(s.depth);scalar(s.cellSize);
    scalar(s.origin.x);scalar(s.origin.y);scalar(s.origin.z);scalar(field.MinimumHeight());scalar(field.MaximumHeight());
    for(unsigned z=0;z<s.depth;++z)for(unsigned x=0;x<s.width;++x)scalar(field.Height(x,z));
    return hash;
}
inline float Smooth(float value){const float t=std::clamp(value,0.f,1.f);return t*t*(3.f-2.f*t);}
inline float BrokenEdge(float z){
    // 固定的低頻折線構成沿岩層走向的斷裂，不以逐頂點亂數擾動地面。
    constexpr std::array<float,8> points{0.f,.45f,-.30f,.60f,.15f,-.40f,.35f,0.f};
    const float t=std::clamp((z-51.f)*.5f,0.f,7.f);
    const unsigned i=(std::min)(unsigned(t),6u);
    return points[i]+(points[i+1]-points[i])*(t-float(i));
}
inline float Cut(float x,float z){
    if(!std::isfinite(x)||!std::isfinite(z))throw std::invalid_argument("WEST_CLIFF_NONFINITE_COORDINATE");
    if(x<=-49.f||x>=-41.f||z<=51.f||z>=65.f)return 0.f;
    const float envelope=Smooth((x+49.f)/.75f)*Smooth((-41.f-x)/.75f)*
        Smooth((z-51.f)/1.5f)*Smooth((65.f-z)/1.5f);
    const float edge=BrokenEdge(z);
    const auto ledge=[](float distance){
        // 短陡壁接較長的侵蝕平台；不同側的坡度形成可辨識階差，而非圓丘。
        if(distance<=0.f||distance>=2.6f)return 0.f;
        if(distance<.55f)return distance/.55f;
        if(distance<1.35f)return 1.f;
        return (2.6f-distance)/1.25f;
    };
    const float first=ledge(x+48.4f-edge*.55f);
    const float second=ledge(x+44.8f+edge*.40f);
    return (std::min)(MaximumCut,envelope*(1.40f*first+1.15f*second));
}
inline bool RectanglesOverlap(float ax0,float ax1,float az0,float az1,float bx0,float bx1,float bz0,float bz1){
    return ax0<=bx1&&ax1>=bx0&&az0<=bz1&&az1>=bz0;
}
inline void ValidateProtectedRectangle(float x0,float x1,float z0,float z1){
    // RiverProfile 的解析包絡：中心 36 ± 3.5，半寬最多 4+2+4。
    // 保護整個矩形／三角面，不以有限個頂點採樣代替區域證明。
    constexpr float riverSouth=36.f-3.5f-(4.f+2.f+4.f),riverNorth=36.f+3.5f+(4.f+2.f+4.f);
    constexpr float trailHalfWidth=.35f+2.10f+.15f;
    if(!std::isfinite(x0)||!std::isfinite(x1)||!std::isfinite(z0)||!std::isfinite(z1)||x0>x1||z0>z1||
        !(z1<riverSouth||z0>riverNorth)||
        RectanglesOverlap(x0,x1,z0,z1,-trailHalfWidth,trailHalfWidth,-10.f,96.f))
        throw std::invalid_argument("WEST_CLIFF_PROTECTED_RIVER_TRAIL");
    const auto checkPath=[&](Aevrynd::Vec3 p){
        constexpr float clearance=1.9f;
        if(RectanglesOverlap(x0,x1,z0,z1,(std::min)(0.f,p.x)-clearance,(std::max)(0.f,p.x)+clearance,p.z-clearance,p.z+clearance))
            throw std::invalid_argument("WEST_CLIFF_PROTECTED_MISSION_PATH");
    };
    for(const auto p:{Session::fuse,Session::generator,Session::rangerLog})checkPath(p);
    for(const auto p:Session::relaySites)checkPath(p);
    const auto checkSite=[&](Aevrynd::Vec3 p){
        constexpr float clearance=3.f;
        if(RectanglesOverlap(x0,x1,z0,z1,p.x-clearance,p.x+clearance,p.z-clearance,p.z+clearance))
            throw std::invalid_argument("WEST_CLIFF_PROTECTED_MISSION_SITE");
    };
    for(const auto p:Session::supplies)checkSite(p);
    for(const auto p:Session::repairSites)checkSite(p);
    for(const auto p:{Session::bridge,Session::arena,Session::beacon})checkSite(p);
    const auto bounds=SettlementGround::Region();const float fade=EntranceRoadDomainFadeDistance;
    if(RectanglesOverlap(x0,x1,z0,z1,bounds.west-fade,bounds.east+fade,bounds.south-fade,bounds.north+fade))
        throw std::invalid_argument("WEST_CLIFF_PROTECTED_SETTLEMENT");
}
inline Result Build(const Aevrynd::HeightField3D& canonical){
    const auto expected=CreateTerrain();
    if(!SameExact(canonical,expected))throw std::invalid_argument("WEST_CLIFF_CANONICAL_IDENTITY");
    ValidateFallenLogTerrain(canonical);
    ValidateProtectedRectangle(GuardMinX,GuardMaxX,GuardMinZ,GuardMaxZ);
    Result result{canonical,{}};auto& receipt=result.receipt;
    receipt.canonicalFingerprint=Fingerprint(canonical);
    for(unsigned z=0;z<canonical.Settings().depth;++z)for(unsigned x=0;x<canonical.Settings().width;++x){
        const auto p=canonical.SamplePosition(x,z);const float cut=Cut(p.x,p.z);
        if(cut==0.f)continue;
        const float y=p.y-cut;
        if(!std::isfinite(y)||y>p.y||p.y-y>MaximumCut)throw std::invalid_argument("WEST_CLIFF_CUT_RANGE");
        result.terrain.SetHeight(x,z,y);receipt.changedSamples+=!Bits(y,p.y);
        receipt.maximumCut=(std::max)(receipt.maximumCut,p.y-y);
    }
    result.terrain.RecalculateRange();
    if(!Bits(result.terrain.MinimumHeight(),canonical.MinimumHeight())||!Bits(result.terrain.MaximumHeight(),canonical.MaximumHeight()))
        throw std::invalid_argument("WEST_CLIFF_RANGE_CHANGED");
    for(unsigned z=0;z+1<canonical.Settings().depth;++z)for(unsigned x=0;x+1<canonical.Settings().width;++x){
        bool changed=false;float lowest=(std::numeric_limits<float>::max)();
        for(unsigned dz=0;dz<2;++dz)for(unsigned dx=0;dx<2;++dx){
            changed|=!Bits(canonical.Height(x+dx,z+dz),result.terrain.Height(x+dx,z+dz));
            lowest=(std::min)(lowest,result.terrain.Height(x+dx,z+dz));
        }
        if(!changed)continue;
        const auto a=canonical.SamplePosition(x,z),d=canonical.SamplePosition(x+1,z+1);
        ValidateProtectedRectangle(a.x,d.x,a.z,d.z);
        // 單一三角面的線性插值最低點必在頂點，故此檢查涵蓋兩個完整面。
        if(lowest<=RiverHeight)throw std::invalid_argument("WEST_CLIFF_SUBMERGED_TRIANGLE");
        ++receipt.protectedCellsVerified;
    }
    for(unsigned z=0;z<canonical.Settings().depth;++z)for(unsigned x=0;x<canonical.Settings().width;++x){
        const auto p=canonical.SamplePosition(x,z);
        receipt.changedNormals+=Aevrynd::PackNormalSnorm8(canonical.SampleNormal(p.x,p.z))!=
            Aevrynd::PackNormalSnorm8(result.terrain.SampleNormal(p.x,p.z));
    }
    if(!receipt.changedSamples||!receipt.changedNormals)throw std::invalid_argument("WEST_CLIFF_EMPTY_RELIEF");
    receipt.derivedFingerprint=Fingerprint(result.terrain);
    receipt.minimumHeight=result.terrain.MinimumHeight();receipt.maximumHeight=result.terrain.MaximumHeight();
    return result;
}
inline Receipt ValidateExact(const Aevrynd::HeightField3D& terrain){
    const auto expected=Build(CreateTerrain());
    if(!SameExact(terrain,expected.terrain))throw std::invalid_argument("WEST_CLIFF_DERIVED_IDENTITY");
    return expected.receipt;
}
inline MeshReceipt UpdateMesh(Aevrynd::Mesh3D& mesh,const Aevrynd::HeightField3D& canonical,const Aevrynd::HeightField3D& derived){
    const auto expected=Build(canonical);
    if(!SameExact(derived,expected.terrain))throw std::invalid_argument("WEST_CLIFF_MESH_DERIVED_IDENTITY");
    if(!mesh.Valid()||mesh.vertices.size()>100000||mesh.indices.size()>600000)
        throw std::invalid_argument("WEST_CLIFF_MESH_LAYOUT");
    // 先檢查完整輸入，再一次性替換，錯誤不得留下部分改寫的網格。
    auto candidate=mesh;MeshReceipt receipt{};
    const auto& settings=canonical.Settings();
    for(auto& v:candidate.vertices){
        if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||!std::isfinite(v.u)||!std::isfinite(v.v)||
            !canonical.Contains(v.x,v.z))throw std::invalid_argument("WEST_CLIFF_MESH_VERTEX");
        const float gx=(v.x-settings.origin.x)/settings.cellSize,gz=(v.z-settings.origin.z)/settings.cellSize;
        // CreateMesh 使用儲存格高度，SampleHeight 的加零運算可能將 -0 轉為 +0。
        // 核對／更新必須沿用相同儲存來源，不藉容差抹除位元差異。
        if(gx!=std::floor(gx)||gz!=std::floor(gz)||!Bits(v.y,canonical.Height(unsigned(gx),unsigned(gz)))||
            v.normal!=Aevrynd::PackNormalSnorm8(canonical.SampleNormal(v.x,v.z))){
            std::ostringstream message;message<<"WEST_CLIFF_MESH_CANONICAL_SURFACE index="<<(&v-candidate.vertices.data())
                <<" x="<<v.x<<" z="<<v.z<<" gx="<<gx<<" gz="<<gz<<" y="<<v.y
                <<" sampleY="<<canonical.SampleHeight(v.x,v.z)<<" yBits="<<std::bit_cast<std::uint32_t>(v.y)
                <<" sampleYBits="<<std::bit_cast<std::uint32_t>(canonical.SampleHeight(v.x,v.z))
                <<" storedYBits="<<std::bit_cast<std::uint32_t>(canonical.Height(unsigned(gx),unsigned(gz)))
                <<" normal="<<v.normal<<" expectedNormal="<<Aevrynd::PackNormalSnorm8(canonical.SampleNormal(v.x,v.z));
            throw std::invalid_argument(message.str());
        }
        const float y=derived.Height(unsigned(gx),unsigned(gz));
        const auto normal=Aevrynd::PackNormalSnorm8(derived.SampleNormal(v.x,v.z));
        const bool position=!Bits(v.y,y),normalChanged=v.normal!=normal;
        if(!position&&!normalChanged)continue;
        const auto color=position?GroundLayerHeightColor(v.color,y,derived):v.color;
        receipt.changedPositions+=position;receipt.changedNormals+=normalChanged;receipt.changedColors+=color!=v.color;
        ++receipt.changedVertices;v.y=y;v.normal=normal;v.color=color;
    }
    for(std::size_t i=0;i<mesh.indices.size();i+=3){
        const auto& a=mesh.vertices[mesh.indices[i]];const auto& b=mesh.vertices[mesh.indices[i+1]];const auto& c=mesh.vertices[mesh.indices[i+2]];
        const float minX=(std::min)({a.x,b.x,c.x}),maxX=(std::max)({a.x,b.x,c.x});
        const float minZ=(std::min)({a.z,b.z,c.z}),maxZ=(std::max)({a.z,b.z,c.z});
        const float orientation=(b.x-a.x)*(c.z-a.z)-(b.z-a.z)*(c.x-a.x);
        const auto corner=[&](const auto& v){return 1u<<((v.x==maxX?1u:0u)+(v.z==maxZ?2u:0u));};
        const unsigned corners=corner(a)|corner(b)|corner(c);
        if(maxX-minX!=settings.cellSize||maxZ-minZ!=settings.cellSize||orientation!=-settings.cellSize*settings.cellSize||
            (corners!=7u&&corners!=14u))throw std::invalid_argument("WEST_CLIFF_MESH_TRIANGLE");
    }
    candidate.InvalidateRaycastIndex();mesh=std::move(candidate);return receipt;
}
}
