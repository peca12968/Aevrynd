#include "MistwatchUnderstoryRetention.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

// 僅驗證合成作者資料的 CPU 保存契約；不宣稱實際網格、世界碰撞或 GPU 已驗收。
namespace {
namespace R=Mistwatch::UnderstoryRetention;
using Aevrynd::Vec3;
unsigned checks{},negatives{};
void Need(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
constexpr std::array<unsigned,3> GoldenCounts{3600,96,192};
constexpr std::array<unsigned,3> Starts{0,3600,3696};
constexpr std::array<std::uint64_t,5> GoldenGrass{
    13676700745932842533ull,6708166984977766172ull,9741578749886758716ull,
    15330444761625695554ull,14455864278497895329ull};
constexpr std::array<std::uint64_t,2> GoldenNettle{5775871692697253043ull,3304028173848676471ull};
constexpr std::array<std::string_view,3> GoldenNames{"forest-grass","roadside-broadleaf","core-herb"};
float GrassScale(Vec3 anchor){return .5f+.25f*std::sin(anchor.x*1.7f+anchor.z);}
std::array<float,13> Fields(const R::Row& row){return {
    row.anchor.x,row.anchor.y,row.anchor.z,row.pose.position.x,row.pose.position.y,row.pose.position.z,
    row.pose.rotation.x,row.pose.rotation.y,row.pose.rotation.z,
    row.pose.scale.x,row.pose.scale.y,row.pose.scale.z,row.footprintRadius};}
// FNV 位元組順序與欄位順序由此獨立列出，不呼叫受測 Detail 或 Validate 產生預期。
std::array<std::uint64_t,3> Oracle(const std::vector<R::Row>& rows){
    std::array<std::uint64_t,3> result{};result.fill(14695981039346656037ull);
    for(const auto& row:rows){
        const unsigned family=static_cast<unsigned>(row.family);Need(family<3,"oracle family invalid");
        auto& h=result[family];
        const auto word=[&](std::uint32_t v){for(unsigned b=0;b<4;++b){h^=(v>>(b*8))&255u;h*=1099511628211ull;}};
        const auto wide=[&](std::uint64_t v){word(static_cast<std::uint32_t>(v));word(static_cast<std::uint32_t>(v>>32));};
        word(family);word(row.id);word(row.variant);
        for(float value:Fields(row))word(std::bit_cast<std::uint32_t>(value));
        word(row.suppressed?1u:0u);wide(row.sourceFingerprint);wide(row.actualFingerprint);
    }
    return result;
}
std::vector<R::Row> SyntheticRows(){
    std::vector<R::Row> rows;rows.reserve(3888);
    for(unsigned family=0;family<3;++family)for(unsigned id=0;id<GoldenCounts[family];++id){
        R::Row row;row.family=static_cast<R::Family>(family);row.id=id;
        row.anchor={-12.f+.125f*float(id%64),.03125f*float(id%7),-.5f+.25f*float(id/64)};
        row.pose.position=row.anchor;
        row.pose.rotation={0,family==0?row.anchor.x+row.anchor.z:.125f*float(id%23),0};
        const float scale=family==0?GrassScale(row.anchor):.5f+.03125f*float(id%9);
        row.pose.scale={scale,scale,scale};row.footprintRadius=.5f+.03125f*float(id%13);
        row.variant=family==0?id%5:2+id%2;
        row.sourceFingerprint=family==0?GoldenGrass[row.variant]:GoldenNettle[row.variant-2];
        row.actualFingerprint=family==2?0x3141592600000000ull+id+1:row.sourceFingerprint;
        row.handle=0x1020304000000000ull+rows.size()+1;
        row.suppressed=family==0?id%7==0:family==1?id%5==0:false;
        rows.push_back(row);
    }
    return rows;
}
template<class Work>void Reject(Work work,std::string_view expected){
    bool rejected=false;
    try{work();}catch(const std::invalid_argument& e){
        Need(std::string_view(e.what())==expected,"unexpected exact rejection reason");rejected=true;
    }
    Need(rejected,"invalid retention input accepted");++negatives;
}
template<class Edit>void RejectEdit(const std::vector<R::Row>& base,unsigned index,Edit edit,std::string_view expected){
    auto rows=base;edit(rows[index]);Reject([&]{(void)R::Validate(rows);},expected);
}
void CheckSummary(const std::vector<R::Row>& rows,std::array<unsigned,3> suppressed){
    const auto expected=Oracle(rows);const auto actual=R::Validate(rows);
    Need(actual.counts==GoldenCounts,"exact retained family counts changed");
    Need(actual.suppressed==suppressed,"suppressed rows were lost or miscounted");
    Need(actual.fingerprints==expected,"content seal differs from independent FNV oracle");
}
void ConstantsAndRows(const std::vector<R::Row>& rows){
    Need(R::Total==3888&&R::Counts==GoldenCounts,"population contract changed");
    Need(R::GrassSources==GoldenGrass&&R::NettleSources==GoldenNettle,"source constants changed");
    Need(rows.size()==3888,"synthetic fixture size wrong");
    for(unsigned family=0;family<3;++family){
        Need(R::Slot(static_cast<R::Family>(family))==family,"family slot wrong");
        Need(R::Name(static_cast<R::Family>(family))==GoldenNames[family],"family name wrong");
        for(unsigned id=0;id<GoldenCounts[family];++id){const auto& row=rows[Starts[family]+id];
            Need(static_cast<unsigned>(row.family)==family&&row.id==id,"fixture order wrong");
            if(family==0){
                Need(row.variant<5&&row.sourceFingerprint==GoldenGrass[row.variant],"fixture grass source wrong");
                Need(row.pose.position.x==row.anchor.x&&row.pose.position.y==row.anchor.y&&row.pose.position.z==row.anchor.z&&row.pose.scale.x==GrassScale(row.anchor)&&
                     row.pose.rotation.y==row.anchor.x+row.anchor.z,"fixture grass pose wrong");
            }else Need(row.variant==2+id%2&&row.sourceFingerprint==GoldenNettle[row.variant-2],"fixture nettle source wrong");
        }
    }
    CheckSummary(rows,{515,20,0});
    // 配置 handle 不屬內容封印，任何非零重新配置均不得改變保存的人口或內容。
    auto realloc=rows;for(unsigned i=0;i<realloc.size();++i)realloc[i].handle=900000ull+realloc.size()-i;
    Need(Oracle(realloc)==Oracle(rows),"independent oracle incorrectly includes handle");
    Need(R::Validate(realloc).fingerprints==R::Validate(rows).fingerprints,"allocation handle changed content seal");
    CheckSummary(realloc,{515,20,0});
    auto allSuppressed=rows;
    for(unsigned i=0;i<3696;++i)allSuppressed[i].suppressed=true;
    CheckSummary(allSuppressed,{3600,96,0});
    auto noneSuppressed=rows;for(auto& row:noneSuppressed)row.suppressed=false;
    CheckSummary(noneSuppressed,{0,0,0});
}
void ShapeNegatives(const std::vector<R::Row>& base){
    for(unsigned size:{0u,1u,3599u,3600u,3695u,3696u,3887u})
        Reject([&]{(void)R::Validate(std::span<const R::Row>(base.data(),size));},"UNDERSTORY_RETENTION_TOTAL");
    auto extra=base;extra.push_back(base.back());Reject([&]{(void)R::Validate(extra);},"UNDERSTORY_RETENTION_TOTAL");
    auto missing=base;missing.erase(missing.begin()+3600);Reject([&]{(void)R::Validate(missing);},"UNDERSTORY_RETENTION_TOTAL");
    for(unsigned family=0;family<3;++family){const unsigned start=Starts[family];
        for(unsigned index:{start,start+GoldenCounts[family]/2,start+GoldenCounts[family]-1}){
            RejectEdit(base,index,[](R::Row& r){++r.id;},"UNDERSTORY_RETENTION_ORDER");
            RejectEdit(base,index,[](R::Row& r){r.id=(std::numeric_limits<unsigned>::max)();},"UNDERSTORY_RETENTION_ORDER");
            RejectEdit(base,index,[](R::Row& r){r.family=static_cast<R::Family>((static_cast<unsigned>(r.family)+1)%3);},"UNDERSTORY_RETENTION_ORDER");
        }
        auto swapped=base;std::swap(swapped[start],swapped[start+1]);
        Reject([&]{(void)R::Validate(swapped);},"UNDERSTORY_RETENTION_ORDER");
        auto duplicate=base;duplicate[start+1]=duplicate[start];
        Reject([&]{(void)R::Validate(duplicate);},"UNDERSTORY_RETENTION_ORDER");
        for(unsigned value:{3u,4u,255u,(std::numeric_limits<unsigned>::max)()}){
            RejectEdit(base,start,[&](R::Row& r){r.family=static_cast<R::Family>(value);},"UNDERSTORY_RETENTION_FAMILY");
            Reject([&]{(void)R::Slot(static_cast<R::Family>(value));},"UNDERSTORY_RETENTION_FAMILY");
            Reject([&]{(void)R::Name(static_cast<R::Family>(value));},"UNDERSTORY_RETENTION_FAMILY");
        }
    }
}
void ResourceNegatives(const std::vector<R::Row>& base){
    for(unsigned family=0;family<3;++family){const unsigned index=Starts[family];
        for(unsigned field=0;field<3;++field)RejectEdit(base,index,[&](R::Row& r){
            if(field==0)r.handle=0;else if(field==1)r.sourceFingerprint=0;else r.actualFingerprint=0;
        },"UNDERSTORY_RETENTION_RESOURCE");
        for(unsigned variant:{0u,1u,4u,5u,255u,(std::numeric_limits<unsigned>::max)()}){
            if(family==0&&variant<5)continue;
            RejectEdit(base,index,[&](R::Row& r){r.variant=variant;},"UNDERSTORY_RETENTION_VARIANT");
        }
        RejectEdit(base,index,[](R::Row& r){r.sourceFingerprint^=0x100000001ull;},"UNDERSTORY_RETENTION_SOURCE");
        const unsigned other=family==0?1:3;
        RejectEdit(base,index,[&](R::Row& r){r.variant=other;},"UNDERSTORY_RETENTION_SOURCE");
        if(family<2)RejectEdit(base,index,[](R::Row& r){r.actualFingerprint^=0x100000001ull;},"UNDERSTORY_RETENTION_SOURCE");
        else RejectEdit(base,index,[](R::Row& r){r.suppressed=true;},"UNDERSTORY_RETENTION_CORE_SUPPRESSION");
    }
}
float& MutableField(R::Row& r,unsigned field){
    switch(field){
    case 0:return r.anchor.x;case 1:return r.anchor.y;case 2:return r.anchor.z;
    case 3:return r.pose.position.x;case 4:return r.pose.position.y;case 5:return r.pose.position.z;
    case 6:return r.pose.rotation.x;case 7:return r.pose.rotation.y;case 8:return r.pose.rotation.z;
    case 9:return r.pose.scale.x;case 10:return r.pose.scale.y;case 11:return r.pose.scale.z;
    case 12:return r.footprintRadius;default:throw std::runtime_error("test field invalid");
    }
}
void PoseNegatives(const std::vector<R::Row>& base){
    const float nan=(std::numeric_limits<float>::quiet_NaN)(),inf=(std::numeric_limits<float>::infinity)();
    for(unsigned family=0;family<3;++family){const unsigned index=Starts[family];
        for(unsigned field=0;field<13;++field)for(float invalid:{nan,inf,-inf})
            RejectEdit(base,index,[&](R::Row& r){MutableField(r,field)=invalid;},"UNDERSTORY_RETENTION_POSE");
        for(float invalid:{-1.f,0.f,.19f,4.0001f})
            RejectEdit(base,index,[&](R::Row& r){r.footprintRadius=invalid;},"UNDERSTORY_RETENTION_POSE");
        for(float invalid:{-1.f,0.f})RejectEdit(base,index,[&](R::Row& r){r.pose.scale={invalid,invalid,invalid};},"UNDERSTORY_RETENTION_POSE");
        for(unsigned axis:{9u,10u,11u})RejectEdit(base,index,[&](R::Row& r){MutableField(r,axis)+=.125f;},"UNDERSTORY_RETENTION_POSE");
        for(unsigned axis:{6u,8u})RejectEdit(base,index,[&](R::Row& r){MutableField(r,axis)=.001f;},"UNDERSTORY_RETENTION_POSE");
        for(unsigned axis:{0u,2u,3u,5u})RejectEdit(base,index,[&](R::Row& r){MutableField(r,axis)+=.125f;},"UNDERSTORY_RETENTION_ANCHOR");
    }
    RejectEdit(base,0,[](R::Row& r){r.pose.position.y+=.125f;},"UNDERSTORY_RETENTION_GRASS_RENDER_POSE");
    RejectEdit(base,0,[](R::Row& r){r.anchor.y+=.125f;},"UNDERSTORY_RETENTION_GRASS_RENDER_POSE");
    RejectEdit(base,0,[](R::Row& r){r.pose.rotation.y+=.125f;},"UNDERSTORY_RETENTION_GRASS_RENDER_POSE");
    RejectEdit(base,0,[](R::Row& r){r.pose.scale={.875f,.875f,.875f};},"UNDERSTORY_RETENTION_GRASS_RENDER_POSE");
    auto boundary=base;boundary[3600].footprintRadius=4.f;CheckSummary(boundary,{515,20,0});
    boundary[3600].footprintRadius=std::nextafter(.19f,1.f);CheckSummary(boundary,{515,20,0});
}
template<class Edit>void SealChange(const std::vector<R::Row>& base,unsigned index,Edit edit){
    auto changed=base;edit(changed[index]);const unsigned family=static_cast<unsigned>(base[index].family);
    const auto before=Oracle(base),after=Oracle(changed);
    Need(before[family]!=after[family],"independent field mutation did not change seal");
    for(unsigned slot=0;slot<3;++slot)if(slot!=family)Need(before[slot]==after[slot],"field mutation contaminated another family");
    const auto actual=R::Validate(changed);Need(actual.fingerprints==after,"valid field mutation not sealed");
    Need(actual.counts==GoldenCounts,"field mutation changed retained population");
}
void FieldSeals(const std::vector<R::Row>& base){
    // 受約束的族群、id、旋轉 X/Z 已以上述負控覆蓋；以下逐欄覆蓋合法內容變異。
    for(unsigned field:{0u,1u,2u,3u,4u,5u,7u,9u,10u,11u,12u})
        SealChange(base,3600,[&](R::Row& r){
            if(field==0||field==3){r.anchor.x+=.125f;r.pose.position.x+=.125f;}
            else if(field==2||field==5){r.anchor.z+=.125f;r.pose.position.z+=.125f;}
            else if(field>=9&&field<=11)r.pose.scale={.875f,.875f,.875f};
            else MutableField(r,field)+=.125f;
        });
    // 負零與正零通過同一數值政策，但封印須保留每一浮點欄的實際位元。
    for(unsigned field:{1u,4u,6u,8u}){
        auto positive=base;MutableField(positive[3600],field)=0.f;
        SealChange(positive,3600,[&](R::Row& r){MutableField(r,field)=-0.f;});
    }
    SealChange(base,3600,[](R::Row& r){r.variant=3;r.sourceFingerprint=GoldenNettle[1];r.actualFingerprint=GoldenNettle[1];});
    SealChange(base,3696,[](R::Row& r){r.actualFingerprint^=0x100000001ull;});
    for(unsigned index:{0u,3600u})SealChange(base,index,[](R::Row& r){r.suppressed=!r.suppressed;});
    // 五種草皆須封住實際 render pose，不允許來源根部與繪製矩陣分裂。
    for(unsigned variant=0;variant<5;++variant){
        SealChange(base,variant,[](R::Row& r){r.anchor.x+=.125f;r.pose.position=r.anchor;
            const float s=GrassScale(r.anchor);r.pose.scale={s,s,s};r.pose.rotation.y=r.anchor.x+r.anchor.z;});
        RejectEdit(base,variant,[](R::Row& r){r.pose.position.y+=.01f;},"UNDERSTORY_RETENTION_GRASS_RENDER_POSE");
        SealChange(base,variant,[&](R::Row& r){r.variant=(variant+1)%5;r.sourceFingerprint=GoldenGrass[r.variant];r.actualFingerprint=r.sourceFingerprint;});
    }
}
}
int main(){
    try{
        const auto syntheticRows=SyntheticRows();ConstantsAndRows(syntheticRows);
        ShapeNegatives(syntheticRows);ResourceNegatives(syntheticRows);PoseNegatives(syntheticRows);FieldSeals(syntheticRows);
        std::cout<<"UNDERSTORY_RETENTION_CPU_CONTRACT_OK; checks="<<checks<<"; negatives="<<negatives
            <<"; synthetic_rows=3888; grass=3600; roadside=96; core=192; geometry_verified=0; world_verified=0; GPU_verified=0; collision_verified=0\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"UNDERSTORY_RETENTION_CONTRACT_FAILED: "<<e.what()<<'\n';return 1;}
}
