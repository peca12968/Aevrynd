#pragma once
#include "MistwatchCanopyCollision.h"
#include "MistwatchValleyTerrain.h"
#include <cstring>
#include <type_traits>

namespace Mistwatch::ValleyStartupBake {
// 此版本是目前木體作者與真實地形支撐作者的離線產物契約；修改作者算法必須重新烘焙／驗收。
inline constexpr std::uint32_t Version=1;
inline constexpr std::size_t MaximumBytes=64u*1024u*1024u;
struct Data {std::vector<CanopyWoodProxy> wood;std::vector<Aevrynd::BoxCollider3D> pads;std::vector<ValleyTerrain::SupportSurface> surfaces;};
inline void Require(bool v,const char* reason){if(!v)throw std::invalid_argument(reason);}
struct Writer {
    std::vector<std::byte> bytes;
    void Raw(std::span<const std::byte> input){Require(input.size()<=MaximumBytes-bytes.size(),"STARTUP_BAKE_SIZE");bytes.insert(bytes.end(),input.begin(),input.end());}
    void U32(std::uint32_t v){for(unsigned i=0;i<4;++i)bytes.push_back(std::byte((v>>(i*8))&255u));Require(bytes.size()<=MaximumBytes,"STARTUP_BAKE_SIZE");}
    void U64(std::uint64_t v){U32(std::uint32_t(v));U32(std::uint32_t(v>>32));}
    void Float(float v){Require(std::isfinite(v),"STARTUP_BAKE_NONFINITE");U32(std::bit_cast<std::uint32_t>(v));}
    void Pose(const Aevrynd::Transform3D& p){for(float v:{p.position.x,p.position.y,p.position.z,p.rotation.x,p.rotation.y,p.rotation.z,p.scale.x,p.scale.y,p.scale.z})Float(v);}
    void Box(const Aevrynd::BoxCollider3D& b){Require(b.extents.x>0&&b.extents.y>0&&b.extents.z>0,"STARTUP_BAKE_BOX");for(float v:{b.center.x,b.center.y,b.center.z,b.extents.x,b.extents.y,b.extents.z})Float(v);}
    void Mesh(const Aevrynd::Mesh3D& m){
        static_assert(sizeof(Aevrynd::Vertex3D)==28&&std::is_trivially_copyable_v<Aevrynd::Vertex3D>);
        static_assert(std::endian::native==std::endian::little);
        Require(m.Valid()&&m.vertices.size()<=600000&&m.indices.size()<=1800000,"STARTUP_BAKE_MESH");
        for(const auto& v:m.vertices)Require(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::isfinite(v.u)&&std::isfinite(v.v),"STARTUP_BAKE_VERTEX");
        U32(std::uint32_t(m.vertices.size()));Raw(std::as_bytes(std::span(m.vertices)));U32(std::uint32_t(m.indices.size()));Raw(std::as_bytes(std::span(m.indices)));
    }
    void Field(const Aevrynd::HeightField3D& t){const auto& s=t.Settings();Float(s.origin.x);Float(s.origin.y);Float(s.origin.z);U32(s.width);U32(s.depth);Float(s.cellSize);
        Require(t.SampleCount()<=1000000,"STARTUP_BAKE_FIELD");for(unsigned z=0;z<s.depth;++z)for(unsigned x=0;x<s.width;++x)Float(t.Height(x,z));}
};
struct Reader {
    std::span<const std::byte> bytes;std::size_t position{};
    std::span<const std::byte> Raw(std::size_t count){Require(position<=bytes.size()&&count<=bytes.size()-position,"STARTUP_BAKE_TRUNCATED");const auto r=bytes.subspan(position,count);position+=count;return r;}
    std::uint32_t U32(){const auto v=Raw(4);std::uint32_t r{};for(unsigned i=0;i<4;++i)r|=std::to_integer<std::uint32_t>(v[i])<<(i*8);return r;}
    std::uint64_t U64(){const auto lo=U32();return lo|(std::uint64_t(U32())<<32);}
    float Float(){const float v=std::bit_cast<float>(U32());Require(std::isfinite(v),"STARTUP_BAKE_NONFINITE");return v;}
    Aevrynd::BoxCollider3D Box(){Aevrynd::BoxCollider3D b;b.center={Float(),Float(),Float()};b.extents={Float(),Float(),Float()};Require(b.extents.x>0&&b.extents.y>0&&b.extents.z>0,"STARTUP_BAKE_BOX");return b;}
};
inline std::vector<std::byte> Encode(std::span<const std::byte> witness,const Data& d){
    Require(!witness.empty()&&witness.size()<=MaximumBytes&&d.wood.size()==8&&d.pads.size()==41&&d.surfaces.size()==41,"STARTUP_BAKE_SCOPE");
    Writer w;w.U32(0x31425356u);w.U32(Version);w.U32(std::uint32_t(witness.size()));w.Raw(witness);w.U32(8);std::size_t parts{};
    for(const auto& p:d.wood){Require(!p.parts.empty()&&p.parts.size()<=9000&&parts+p.parts.size()<=9000&&p.spacing==.15,"STARTUP_BAKE_WOOD_SCOPE");parts+=p.parts.size();w.U32(std::uint32_t(p.parts.size()));
        for(auto n:{p.sourceTriangles,p.triangleCellChecks,p.surfaceCells,p.enclosedGridCells,p.domainCells,p.fingerprint})w.U64(n);
        w.U64(std::bit_cast<std::uint64_t>(p.spacing));for(const auto& b:p.parts)w.Box(b);}
    w.U32(41);for(const auto& b:d.pads)w.Box(b);w.U32(41);
    for(const auto& s:d.surfaces){Require(!s.cells.empty()&&s.cells.size()<=48441&&s.contactCells>0&&s.contactCells<=s.cells.size(),"STARTUP_BAKE_SURFACE");
        w.U32(s.triangles);w.U32(s.intersections);w.U32(s.contactCells);w.U32(std::uint32_t(s.cells.size()));
        for(const auto& c:s.cells){w.U32(c.x);w.U32(c.z);w.Float(c.minimumGap);}}
    return std::move(w.bytes);
}
inline Data Decode(std::span<const std::byte> bytes,std::span<const std::byte> expectedWitness){
    Require(!bytes.empty()&&bytes.size()<=MaximumBytes&&!expectedWitness.empty(),"STARTUP_BAKE_SIZE");Reader r{bytes};
    Require(r.U32()==0x31425356u&&r.U32()==Version,"STARTUP_BAKE_VERSION");const auto length=r.U32();
    Require(length==expectedWitness.size(),"STARTUP_BAKE_INPUT_SIZE");const auto witness=r.Raw(length);
    Require(std::equal(witness.begin(),witness.end(),expectedWitness.begin()),"STARTUP_BAKE_EXACT_INPUT_MISMATCH");
    Require(r.U32()==8,"STARTUP_BAKE_GROUPS");Data d;d.wood.resize(8);std::size_t parts{};
    for(auto& p:d.wood){const auto count=r.U32();Require(count>0&&count<=9000&&parts+count<=9000,"STARTUP_BAKE_WOOD_SCOPE");parts+=count;
        p.sourceTriangles=r.U64();p.triangleCellChecks=r.U64();p.surfaceCells=r.U64();p.enclosedGridCells=r.U64();p.domainCells=r.U64();p.fingerprint=r.U64();p.spacing=std::bit_cast<double>(r.U64());
        Require(p.sourceTriangles>0&&p.sourceTriangles<=1200000&&p.triangleCellChecks<=10000000&&p.surfaceCells>0&&p.domainCells<=1000000&&p.surfaceCells<=p.domainCells&&p.enclosedGridCells<=p.domainCells&&p.surfaceCells+p.enclosedGridCells<=p.domainCells&&p.spacing==.15,"STARTUP_BAKE_WOOD_METADATA");
        std::uint64_t fingerprint=14695981039346656037ull;p.parts.reserve(count);
        for(unsigned i=0;i<count;++i){auto b=r.Box();for(float v:{b.center.x,b.center.y,b.center.z,b.extents.x,b.extents.y,b.extents.z}){
            const auto bits=std::bit_cast<std::uint32_t>(v);for(unsigned j=0;j<4;++j){fingerprint^=(bits>>(j*8))&255u;fingerprint*=1099511628211ull;}}
            p.parts.push_back(b);}
        Require(fingerprint==p.fingerprint,"STARTUP_BAKE_WOOD_FINGERPRINT");}
    Require(r.U32()==41,"STARTUP_BAKE_PADS");for(unsigned i=0;i<41;++i)d.pads.push_back(r.Box());
    Require(r.U32()==41,"STARTUP_BAKE_SURFACES");d.surfaces.resize(41);
    for(auto& s:d.surfaces){s.triangles=r.U32();s.intersections=r.U32();const auto contacts=r.U32(),count=r.U32();
        Require(s.triangles>0&&s.triangles<=1200000&&contacts>0&&contacts<=count&&count<=48441,"STARTUP_BAKE_SURFACE");
        unsigned previous{},actualContacts{};bool first=true;s.cells.reserve(count);
        for(unsigned i=0;i<count;++i){ValleyTerrain::SurfaceCell c{r.U32(),r.U32(),r.Float()};Require(c.x<200&&c.z<240,"STARTUP_BAKE_CELL_DOMAIN");
            const auto key=c.z*201+c.x;Require(first||key>previous,"STARTUP_BAKE_CELL_ORDER");first=false;previous=key;actualContacts+=c.minimumGap<=.15f;s.cells.push_back(c);}
        Require(actualContacts==contacts,"STARTUP_BAKE_CONTACT_COUNT");s.contactCells=contacts;}
    Require(r.position==bytes.size(),"STARTUP_BAKE_TRAILING_BYTES");return d;
}
inline constexpr std::size_t FileBytes=7948440;
inline constexpr std::string_view FileSha256="CC01776A1F99929A6AC61792BFA0B5EA7A73B72587E328CA727641722D559242";
inline constexpr std::string_view RelativePath="mistwatch-world/valley-startup-v1.bin";
}
