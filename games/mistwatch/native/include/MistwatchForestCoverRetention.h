#pragma once
#include "MistwatchForestCover.h"
#include "MistwatchGroupedCanopyVolume.h"
#include <bit>
#include <cstdint>
#include <memory>
#include <stdexcept>

namespace Mistwatch {
struct ForestCoverWorldView {
    const Aevrynd::HeightField3D& terrain;
    std::span<const Aevrynd::Vec3> trees;
    const ForestUnderstory& understory;
    std::span<const Aevrynd::BoxCollider3D> solids;
    std::span<const Aevrynd::BoxCollider3D> wood;
    std::size_t expectedWoodCount{};
    std::uint64_t expectedInputFingerprint{};
    const GroupedCanopyVolume::ValidatedWood* groupedWood=nullptr;
};
enum class ForestCoverRetentionGate : unsigned { Domain,GroundAnchor,Clearance,Volume,Terrain,Spacing };
enum class ForestCoverPlantOrigin : unsigned { Retained,OriginalPatchFill,NewPatchFill };
struct ForestCoverPlantProvenance {
    ForestCoverPlantOrigin origin{};
    unsigned sourcePlant{(std::numeric_limits<unsigned>::max)()};
    unsigned sourcePatch{(std::numeric_limits<unsigned>::max)()};
};
struct ForestCoverRetentionRejection {unsigned sourcePlant{},sourcePatch{};ForestCoverRetentionGate gate{};};
struct ForestCoverRetentionResult {
    ForestCoverResult cover;
    std::vector<ForestCoverPlantProvenance> provenance;
    std::vector<ForestCoverRetentionRejection> rejections;
    unsigned retained{},rejected{},filled{};
    std::uint64_t originalInputFingerprint{},finalInputFingerprint{},provenanceFingerprint{},fingerprint{};
};
namespace ForestCoverRetentionDetail {
struct Hasher {
    std::uint64_t value{1469598103934665603ull};
    void Word(std::uint32_t word) {for(unsigned byte=0;byte<4;++byte){value^=(word>>(byte*8))&255u;value*=1099511628211ull;}}
    void Count(std::size_t count) {const auto wide=static_cast<std::uint64_t>(count);Word(static_cast<std::uint32_t>(wide));Word(static_cast<std::uint32_t>(wide>>32));}
    void Float(float number) {if(!std::isfinite(number))throw std::invalid_argument("Nonfinite retention input");Word(std::bit_cast<std::uint32_t>(number));}
    void Vector(Aevrynd::Vec3 vector) {Float(vector.x);Float(vector.y);Float(vector.z);}
    void Pose(const Aevrynd::Transform3D& pose) {Vector(pose.position);Vector(pose.rotation);Vector(pose.scale);}
    void Plant(const ForestCoverPlant& plant) {Pose(plant.transform);Float(plant.radius);Word(plant.variant);Word(plant.patch);}
    void Patch(const ForestCoverPatch& patch) {Vector(patch.centre);Float(patch.radius);Float(patch.aspect);Float(patch.yaw);Word(patch.requested);Word(patch.accepted);}
};
inline bool Same(float a,float b) {return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
inline bool Same(Aevrynd::Vec3 a,Aevrynd::Vec3 b) {return Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z);}
inline bool Same(const ForestCoverPlant& a,const ForestCoverPlant& b) {
    return Same(a.transform.position,b.transform.position)&&Same(a.transform.rotation,b.transform.rotation)&&
        Same(a.transform.scale,b.transform.scale)&&Same(a.radius,b.radius)&&a.variant==b.variant&&a.patch==b.patch;
}
inline bool Same(const ForestCoverResult& a,const ForestCoverResult& b) {
    if(a.plants.size()!=b.plants.size()||a.patches.size()!=b.patches.size()||a.reachedBudget!=b.reachedBudget||
        a.rejectedClearance!=b.rejectedClearance||a.rejectedTerrain!=b.rejectedTerrain||a.rejectedSpacing!=b.rejectedSpacing)return false;
    for(std::size_t i=0;i<a.plants.size();++i)if(!Same(a.plants[i],b.plants[i]))return false;
    for(std::size_t i=0;i<a.patches.size();++i) {
        const auto& x=a.patches[i];const auto& y=b.patches[i];
        if(!Same(x.centre,y.centre)||!Same(x.radius,y.radius)||!Same(x.aspect,y.aspect)||!Same(x.yaw,y.yaw)||
            x.requested!=y.requested||x.accepted!=y.accepted)return false;
    }
    return true;
}
inline void Settings(Hasher& hash,const ForestCoverSettings& settings) {
    hash.Word(settings.seed);hash.Count(settings.targetPlants);hash.Word(settings.maximumPatches);hash.Word(settings.attemptsPerPatch);
    hash.Word(settings.independentAnchors);hash.Word(settings.minimumPlantsPerPatch);hash.Word(settings.maximumPlantsPerPatch);
    hash.Word(settings.prioritizeRouteShoulders?1u:0u);hash.Float(settings.minimumNormalY);
    hash.Float(settings.maximumFootprintHeightDelta);hash.Float(settings.rootSeparationRatio);
}
}
// 此完整輸入指紋不包含期望值；呼叫端須從獨立世界證書取得期望值與木件數。
inline std::uint64_t ForestCoverRetentionWorldFingerprint(const ForestCoverWorldView& world) {
    using namespace ForestCoverRetentionDetail;
    if(world.groupedWood&&(!world.wood.empty()||world.groupedWood->Identity().groupCount==0||
        world.groupedWood->Identity().groupCount>GroupedCanopyVolume::MaximumGroups||
        world.groupedWood->Groups().size()!=world.groupedWood->Identity().groupCount||
        world.groupedWood->Identity().totalParts==0||world.groupedWood->Identity().totalParts>GroupedCanopyVolume::MaximumTotalParts))
        throw std::invalid_argument("Invalid grouped retention wood");
    if(world.trees.size()>10000||world.understory.ferns.size()>10000||world.understory.saplings.size()>10000||
        !CanopyVolume::ValidWoodParts(world.solids)||!CanopyVolume::ValidWoodParts(world.wood))
        throw std::invalid_argument("Invalid complete retention world");
    Hasher hash;hash.Word(world.groupedWood?0x52545732u:0x52545731u);
    const auto& grid=world.terrain.Settings();hash.Vector(grid.origin);hash.Word(grid.width);hash.Word(grid.depth);hash.Float(grid.cellSize);
    hash.Count(world.terrain.SampleCount());
    for(unsigned z=0;z<grid.depth;++z)for(unsigned x=0;x<grid.width;++x)hash.Float(world.terrain.Height(x,z));
    hash.Count(world.trees.size());for(const auto tree:world.trees)hash.Vector(tree);
    for(const auto* population:{&world.understory.ferns,&world.understory.saplings}) {
        hash.Count(population->size());for(const auto& pose:*population)hash.Pose(pose);
    }
    if(!world.groupedWood){
        for(const auto boxes:{world.solids,world.wood}) {hash.Count(boxes.size());for(const auto& box:boxes){hash.Vector(box.center);hash.Vector(box.extents);}}
    }else{
        // RTW2 明確保留組序、邊界與全部木件；RTW1 位元流完全不改。
        hash.Count(world.solids.size());for(const auto& box:world.solids){hash.Vector(box.center);hash.Vector(box.extents);}
        const auto& identity=world.groupedWood->Identity();hash.Count(identity.groupCount);hash.Count(identity.totalParts);hash.Count(identity.fingerprint);
        std::size_t ordinal{};for(const auto& group:world.groupedWood->Groups()){
            hash.Count(ordinal++);hash.Count(group.size());for(const auto& box:group){hash.Vector(box.center);hash.Vector(box.extents);}}
    }
    return hash.value;
}
// 私有不可變原始快照只由原配置器建立；不能從新世界或呼叫端任意人口授權。
class ValidatedForestCoverOriginal final {
    struct State {
        Aevrynd::HeightField3D terrain;
        std::vector<Aevrynd::Vec3> trees;
        ForestUnderstory understory;
        std::vector<Aevrynd::BoxCollider3D> solids,wood;
        std::array<ForestCoverAsset,5> assets;
        std::array<std::uint64_t,5> assetIdentities;
        ForestCoverSettings settings;
        ForestCoverResult original;
        State(const Aevrynd::HeightField3D& t,std::span<const Aevrynd::Vec3> treesIn,const ForestUnderstory& u,
            std::span<const Aevrynd::BoxCollider3D> s,const std::array<ForestCoverAsset,5>& a,
            const std::array<std::uint64_t,5>& ids,ForestCoverSettings settingsIn,
            std::span<const Aevrynd::BoxCollider3D> w,ForestCoverResult result)
            :terrain(t),trees(treesIn.begin(),treesIn.end()),understory(u),solids(s.begin(),s.end()),
            wood(w.begin(),w.end()),assets(a),assetIdentities(ids),settings(settingsIn),original(std::move(result)){}
    };
    std::shared_ptr<const State> state_;
    static bool Same(const Aevrynd::Transform3D& a,const Aevrynd::Transform3D& b){
        using ForestCoverRetentionDetail::Same;
        return Same(a.position,b.position)&&Same(a.rotation,b.rotation)&&Same(a.scale,b.scale);
    }
    static bool Same(const Aevrynd::BoxCollider3D& a,const Aevrynd::BoxCollider3D& b){
        using ForestCoverRetentionDetail::Same;return Same(a.center,b.center)&&Same(a.extents,b.extents);
    }
    static bool Same(Aevrynd::Vec3 a,Aevrynd::Vec3 b){return ForestCoverRetentionDetail::Same(a,b);}
    template<class T>static bool SameRange(std::span<const T> a,std::span<const T> b){
        if(a.size()!=b.size())return false;
        for(std::size_t i=0;i<a.size();++i)if(!Same(a[i],b[i]))return false;
        return true;
    }
    static bool SameTerrain(const Aevrynd::HeightField3D& a,const Aevrynd::HeightField3D& b){
        using ForestCoverRetentionDetail::Same;
        const auto& x=a.Settings();const auto& y=b.Settings();
        if(x.width!=y.width||x.depth!=y.depth||!Same(x.origin,y.origin)||!Same(x.cellSize,y.cellSize)||
            a.SampleCount()!=b.SampleCount()||!Same(a.MinimumHeight(),b.MinimumHeight())||!Same(a.MaximumHeight(),b.MaximumHeight()))return false;
        for(unsigned z=0;z<x.depth;++z)for(unsigned i=0;i<x.width;++i)if(!Same(a.Height(i,z),b.Height(i,z)))return false;
        return true;
    }
    static bool SameSettings(const ForestCoverSettings& a,const ForestCoverSettings& b){
        using ForestCoverRetentionDetail::Same;
        return a.seed==b.seed&&a.targetPlants==b.targetPlants&&a.maximumPatches==b.maximumPatches&&
            a.attemptsPerPatch==b.attemptsPerPatch&&a.independentAnchors==b.independentAnchors&&
            a.minimumPlantsPerPatch==b.minimumPlantsPerPatch&&a.maximumPlantsPerPatch==b.maximumPlantsPerPatch&&
            a.prioritizeRouteShoulders==b.prioritizeRouteShoulders&&Same(a.minimumNormalY,b.minimumNormalY)&&
            Same(a.maximumFootprintHeightDelta,b.maximumFootprintHeightDelta)&&Same(a.rootSeparationRatio,b.rootSeparationRatio);
    }
public:
    ValidatedForestCoverOriginal()=default;
    [[nodiscard]]static ValidatedForestCoverOriginal Create(const Aevrynd::HeightField3D& terrain,
        std::span<const Aevrynd::Vec3> trees,const ForestUnderstory& understory,
        std::span<const Aevrynd::BoxCollider3D> solids,const std::array<ForestCoverAsset,5>& assets,
        const std::array<std::uint64_t,5>& assetIdentities,ForestCoverSettings settings={},
        std::span<const Aevrynd::BoxCollider3D> wood={}){
        // 原入口、亂數與完整體積旗標不變；不足額仍由原 retention 的同一位置拒絕。
        auto original=CreateForestCover(terrain,trees,understory,solids,assets,settings,wood,true);
        ValidatedForestCoverOriginal token;
        token.state_=std::make_shared<const State>(terrain,trees,understory,solids,assets,assetIdentities,
            settings,wood,std::move(original));return token;
    }
    [[nodiscard]]const ForestCoverResult& Original()const{
        if(!state_)throw std::invalid_argument("RETENTION_ORIGINAL_TOKEN_REQUIRED");
        return state_->original;
    }
    void Validate(const ForestCoverResult& canonicalOld,const ForestCoverWorldView& original,
        const std::array<ForestCoverAsset,5>& assets,const std::array<std::uint64_t,5>& assetIdentities,
        ForestCoverSettings settings={})const{
        const auto& generated=Original();const auto& s=*state_;
        // 與舊 verifiedOld 一樣，獨立逐 bit 核對完整人口，含未寫入放置雜湊的欄位。
        if(!generated.reachedBudget||!ForestCoverRetentionDetail::Same(generated,canonicalOld))
            throw std::invalid_argument("Retention canonical source mismatch or shortfall");
        if(original.groupedWood||!SameTerrain(s.terrain,original.terrain)||
            !SameRange<Aevrynd::Vec3>(s.trees,original.trees)||
            !SameRange<Aevrynd::Transform3D>(s.understory.ferns,original.understory.ferns)||
            !SameRange<Aevrynd::Transform3D>(s.understory.saplings,original.understory.saplings)||
            !SameRange<Aevrynd::BoxCollider3D>(s.solids,original.solids)||
            !SameRange<Aevrynd::BoxCollider3D>(s.wood,original.wood)||!SameSettings(s.settings,settings)||
            s.assetIdentities!=assetIdentities)throw std::invalid_argument("RETENTION_ORIGINAL_INPUT_CHANGED");
        for(unsigned i=0;i<5;++i){const auto& a=s.assets[i];const auto& b=assets[i];
            if(!ForestCoverRetentionDetail::Same(a.radius,b.radius)||!ForestCoverRetentionDetail::Same(a.height,b.height)||
                !ForestCoverRetentionDetail::Same(a.minimumY,b.minimumY)||!ForestCoverRetentionDetail::Same(a.maximumY,b.maximumY))
                throw std::invalid_argument("RETENTION_ORIGINAL_INPUT_CHANGED");}
    }
};
namespace ForestCoverRetentionDetail {
// 新重用入口仍在舊重建位置執行同序的便宜輸入驗證，不能讓 token 掩蓋原例外。
inline void ValidateOriginalGeneratorInputs(const ForestCoverWorldView& original,
    const std::array<ForestCoverAsset,5>& assets,const ForestCoverSettings& settings){
    if(!CanopyVolume::ValidWoodParts(original.wood))throw std::invalid_argument("Invalid canopy wood parts for grass");
    if(settings.targetPlants>10000||!settings.maximumPatches||settings.maximumPatches>1000||
       !settings.attemptsPerPatch||settings.attemptsPerPatch>10000||
       settings.independentAnchors>2000||!settings.minimumPlantsPerPatch||
       settings.maximumPlantsPerPatch<settings.minimumPlantsPerPatch||settings.maximumPlantsPerPatch>128||
       !std::isfinite(settings.minimumNormalY)||settings.minimumNormalY<.88f||settings.minimumNormalY>1.f||
       !std::isfinite(settings.maximumFootprintHeightDelta)||settings.maximumFootprintHeightDelta<=0||
       settings.maximumFootprintHeightDelta>.24f||!std::isfinite(settings.rootSeparationRatio)||
       settings.rootSeparationRatio<.65f||settings.rootSeparationRatio>2.f)
        throw std::invalid_argument("Invalid forest cover settings");
    for(const auto& asset:assets)if(!std::isfinite(asset.radius)||asset.radius<GrassMinimumRadius-.0001f||
       asset.radius>4.f||!std::isfinite(asset.height)||asset.height<GrassAssetHeight-.0001f)
        throw std::invalid_argument("Invalid normalized grass asset footprint");
    if(!original.wood.empty())for(const auto& asset:assets)
        if(!CanopyVolume::Detail::ValidLocal(CanopyVolume::ConservativeLocalVolume(asset.radius,asset.minimumY,asset.maximumY)))
            throw std::invalid_argument("Invalid normalized grass full volume");
}
inline ForestCoverRetentionResult CreateRetained(const ForestCoverResult& canonicalOld,
    const ForestCoverWorldView& original,const ForestCoverWorldView& finalWorld,
    const std::array<ForestCoverAsset,5>& assets,const std::array<std::uint64_t,5>& assetIdentities,
    ForestCoverSettings settings,const ValidatedForestCoverOriginal* token) {
    using namespace ForestCoverRetentionDetail;
    if(original.groupedWood)throw std::invalid_argument("Retention original world must use flat canopy wood");
    if(finalWorld.groupedWood&&(!finalWorld.wood.empty()||finalWorld.groupedWood->Identity().groupCount==0||
        finalWorld.groupedWood->Identity().groupCount>GroupedCanopyVolume::MaximumGroups||
        finalWorld.groupedWood->Groups().size()!=finalWorld.groupedWood->Identity().groupCount||
        finalWorld.groupedWood->Identity().totalParts==0||finalWorld.groupedWood->Identity().totalParts>GroupedCanopyVolume::MaximumTotalParts))
        throw std::invalid_argument("Invalid grouped retention wood");
    // 保留作者不增加舊的最大嘗試數，也不接受缺件／自行縮減的木件集合。
    if(settings.maximumPatches>400||settings.attemptsPerPatch>240||!original.expectedInputFingerprint||
        !finalWorld.expectedInputFingerprint||original.wood.size()!=original.expectedWoodCount||
        (finalWorld.groupedWood?finalWorld.groupedWood->Identity().totalParts:finalWorld.wood.size())!=finalWorld.expectedWoodCount)
        throw std::invalid_argument("Retention world identity or finite budget mismatch");
    ForestCoverRetentionResult output;
    output.originalInputFingerprint=ForestCoverRetentionWorldFingerprint(original);
    output.finalInputFingerprint=ForestCoverRetentionWorldFingerprint(finalWorld);
    if(output.originalInputFingerprint!=original.expectedInputFingerprint||output.finalInputFingerprint!=finalWorld.expectedInputFingerprint)
        throw std::invalid_argument("Retention complete world fingerprint mismatch");
    for(unsigned i=0;i<5;++i) {
        if(!assetIdentities[i]||!std::isfinite(assets[i].height)||assets[i].height<GrassAssetHeight-.0001f||
            !CanopyVolume::Detail::ValidLocal(CanopyVolume::ConservativeLocalVolume(assets[i].radius,assets[i].minimumY,assets[i].maximumY)))
            throw std::invalid_argument("Retention asset identity or volume invalid");
        for(unsigned j=0;j<i;++j)if(assetIdentities[i]==assetIdentities[j])throw std::invalid_argument("Retention asset identities aliased");
    }
    if(token){
        ValidateOriginalGeneratorInputs(original,assets,settings);
        token->Validate(canonicalOld,original,assets,assetIdentities,settings);
    }else{
        // 舊 API 保留獨立重建與逐位核對，作為新入口的非同源對照。
        const auto verifiedOld=CreateForestCover(original.terrain,original.trees,original.understory,original.solids,assets,
            settings,original.wood,true);
        if(!verifiedOld.reachedBudget||!Same(verifiedOld,canonicalOld))throw std::invalid_argument("Retention canonical source mismatch or shortfall");
    }
    output.cover.plants.reserve(settings.targetPlants);output.provenance.reserve(settings.targetPlants);
    output.cover.patches=canonicalOld.patches;
    for(auto& patch:output.cover.patches)patch.accepted=0;
    const auto safe=[&](const ForestCoverPlant& plant,ForestCoverRetentionGate& gate) {
        const auto root=plant.transform.position;
        gate=ForestCoverRetentionGate::Domain;
        if(root.x< -32.f||root.x>32.f||root.z< -10.f||root.z>88.f)return false;
        gate=ForestCoverRetentionGate::GroundAnchor;
        if(!Same(root.y,finalWorld.terrain.SampleHeight(root.x,root.z)))return false;
        gate=ForestCoverRetentionGate::Clearance;
        if(!ForestCoverClearance(root,plant.radius,finalWorld.solids))return false;
        const auto& asset=assets[plant.variant];
        const CanopyVolume::PlantVolume volume{root,CanopyVolume::ConservativeLocalVolume(asset.radius,asset.minimumY,asset.maximumY),
            plant.transform.scale.x,plant.transform.rotation};
        gate=ForestCoverRetentionGate::Volume;
        if(!CanopyVolume::ClearsCanopyWood(volume,finalWorld.solids)||
            !(finalWorld.groupedWood?finalWorld.groupedWood->Clears(volume):CanopyVolume::ClearsCanopyWood(volume,finalWorld.wood)))return false;
        gate=ForestCoverRetentionGate::Terrain;
        if(!ForestCoverFootprint(finalWorld.terrain,root,plant.radius,settings))return false;
        gate=ForestCoverRetentionGate::Spacing;
        for(const auto& accepted:output.cover.plants)
            if(std::hypot(root.x-accepted.transform.position.x,root.z-accepted.transform.position.z)<
                settings.rootSeparationRatio*(plant.radius+accepted.radius))return false;
        return true;
    };
    const auto reject=[&](ForestCoverRetentionGate gate) {
        if(gate==ForestCoverRetentionGate::Spacing)++output.cover.rejectedSpacing;
        else if(gate==ForestCoverRetentionGate::Terrain||gate==ForestCoverRetentionGate::GroundAnchor||gate==ForestCoverRetentionGate::Domain)
            ++output.cover.rejectedTerrain;
        else ++output.cover.rejectedClearance;
    };
    for(std::size_t ordinal=0;ordinal<canonicalOld.plants.size();++ordinal) {
        const auto& plant=canonicalOld.plants[ordinal];ForestCoverRetentionGate gate{};
        if(safe(plant,gate)) {
            output.cover.plants.push_back(plant);++output.cover.patches[plant.patch].accepted;++output.retained;
            output.provenance.push_back({ForestCoverPlantOrigin::Retained,static_cast<unsigned>(ordinal),plant.patch});
        }else {++output.rejected;reject(gate);output.rejections.push_back({static_cast<unsigned>(ordinal),plant.patch,gate});}
    }
    std::uint32_t fillSeed=settings.seed^0x52544631u;
    const auto random=[&](){fillSeed=fillSeed*1664525u+1013904223u;return float(fillSeed>>8)/16777216.f;};
    const auto fillPatch=[&](unsigned patchIndex,ForestCoverPlantOrigin origin,unsigned sourcePatch) {
        auto& patch=output.cover.patches[patchIndex];const float cosine=std::cos(patch.yaw),sine=std::sin(patch.yaw);
        for(unsigned attempt=0;attempt<settings.attemptsPerPatch&&patch.accepted<patch.requested&&
            output.cover.plants.size()<settings.targetPlants;++attempt) {
            const float distance=patch.radius*std::sqrt(random()),angle=random()*6.2831853f;
            const float dx=distance*std::cos(angle),dz=distance*std::sin(angle)*patch.aspect;
            Aevrynd::Vec3 root{patch.centre.x+dx*cosine-dz*sine,0,patch.centre.z+dx*sine+dz*cosine};
            if(root.x< -32.f||root.x>32.f||root.z< -10.f||root.z>88.f)continue;
            root.y=finalWorld.terrain.SampleHeight(root.x,root.z);
            const float scale=ForestCoverScale(root);const unsigned variant=(std::min)(unsigned(random()*5.f),4u);
            const ForestCoverPlant plant{{root,{0,root.x+root.z,0},{scale,scale,scale}},variant,patchIndex,assets[variant].radius*scale};
            ForestCoverRetentionGate gate{};
            if(!safe(plant,gate)){reject(gate);continue;}
            output.cover.plants.push_back(plant);++patch.accepted;++output.filled;
            output.provenance.push_back({origin,(std::numeric_limits<unsigned>::max)(),sourcePatch});
        }
    };
    for(unsigned patch=0;patch<canonicalOld.patches.size()&&output.cover.plants.size()<settings.targetPlants;++patch)
        fillPatch(patch,ForestCoverPlantOrigin::OriginalPatchFill,patch);
    std::vector<Aevrynd::Vec3> anchors;
    for(const auto& fern:finalWorld.understory.ferns)anchors.push_back(fern.position);
    for(const auto& tree:finalWorld.understory.saplings)anchors.push_back(tree.position);
    for(const auto tree:finalWorld.trees)for(float sign:{-1.f,1.f})anchors.push_back(tree+Aevrynd::Vec3{sign*2.2f,0,sign*.8f});
    for(float z=-8.f;z<=20.f;z+=4.f)for(float x:{6.f,9.f,12.f})for(float sign:{-1.f,1.f})
        anchors.push_back({sign*x+(random()-.5f)*1.4f,0,z+(random()-.5f)*1.4f});
    for(unsigned index=0;index<settings.independentAnchors;++index)anchors.push_back({-32.f+64.f*random(),0,-10.f+98.f*random()});
    if(settings.prioritizeRouteShoulders) {
        const auto priority=[](Aevrynd::Vec3 p) {
            if(p.z>=-10.f&&p.z<=20.f&&std::abs(p.x)>=4.5f&&std::abs(p.x)<=12.f)return 3;
            if(p.z>=0.f&&p.z<=26.f&&std::abs(p.x)>=4.5f&&std::abs(p.x)<=16.f)return 2;
            if(p.x<=-8.f&&p.x>=-24.f&&p.z>=20.f&&p.z<=52.f)return 1;return 0;
        };
        std::stable_sort(anchors.begin(),anchors.end(),[&](Aevrynd::Vec3 a,Aevrynd::Vec3 b){return priority(a)>priority(b);});
    }
    unsigned activePatches{};for(const auto& patch:output.cover.patches)activePatches+=patch.accepted!=0;
    for(auto centre:anchors) {
        if(output.cover.plants.size()>=settings.targetPlants||activePatches>=settings.maximumPatches)break;
        centre.y=finalWorld.terrain.SampleHeight(centre.x,centre.z);
        if(!ForestCoverClearance(centre,.5f,finalWorld.solids))continue;
        bool overlaps=false;
        for(const auto& patch:output.cover.patches)if(patch.accepted&&std::hypot(centre.x-patch.centre.x,centre.z-patch.centre.z)<3.2f){overlaps=true;break;}
        if(overlaps)continue;
        const ForestCoverPatch patch{centre,1.5f+1.3f*random(),.6f+.4f*random(),random()*6.2831853f,
            settings.minimumPlantsPerPatch+unsigned(random()*float(settings.maximumPlantsPerPatch-settings.minimumPlantsPerPatch+1)),0};
        const auto patchIndex=static_cast<unsigned>(output.cover.patches.size());output.cover.patches.push_back(patch);
        fillPatch(patchIndex,ForestCoverPlantOrigin::NewPatchFill,patchIndex);
        if(output.cover.patches.back().accepted)++activePatches;else output.cover.patches.pop_back();
    }
    // 空來源 patch 不能冒充作用中群落；保留來源 ordinal，僅重新映射儲存索引。
    std::vector<unsigned> mapping(output.cover.patches.size(),(std::numeric_limits<unsigned>::max)());
    std::vector<ForestCoverPatch> compact;
    for(std::size_t index=0;index<output.cover.patches.size();++index)if(output.cover.patches[index].accepted) {
        mapping[index]=static_cast<unsigned>(compact.size());compact.push_back(output.cover.patches[index]);
    }
    for(auto& plant:output.cover.plants)plant.patch=mapping[plant.patch];
    output.cover.patches=std::move(compact);output.cover.reachedBudget=output.cover.plants.size()==settings.targetPlants;
    Hasher provenance;provenance.Word(0x52544631u);provenance.Count(output.originalInputFingerprint);provenance.Count(output.finalInputFingerprint);
    Settings(provenance,settings);
    for(unsigned index=0;index<5;++index) {provenance.Count(assetIdentities[index]);provenance.Float(assets[index].radius);
        provenance.Float(assets[index].height);provenance.Float(assets[index].minimumY);provenance.Float(assets[index].maximumY);}
    provenance.Count(canonicalOld.plants.size());for(const auto& plant:canonicalOld.plants)provenance.Plant(plant);
    provenance.Count(canonicalOld.patches.size());for(const auto& patch:canonicalOld.patches)provenance.Patch(patch);
    provenance.Word(canonicalOld.rejectedClearance);provenance.Word(canonicalOld.rejectedTerrain);provenance.Word(canonicalOld.rejectedSpacing);
    provenance.Word(canonicalOld.reachedBudget?1u:0u);
    provenance.Count(output.cover.plants.size());
    for(std::size_t index=0;index<output.cover.plants.size();++index) {provenance.Plant(output.cover.plants[index]);
        const auto& identity=output.provenance[index];provenance.Word(static_cast<unsigned>(identity.origin));
        provenance.Word(identity.sourcePlant);provenance.Word(identity.sourcePatch);}
    provenance.Count(output.cover.patches.size());for(const auto& patch:output.cover.patches)provenance.Patch(patch);
    provenance.Word(output.retained);provenance.Word(output.rejected);provenance.Word(output.filled);
    provenance.Word(output.cover.rejectedClearance);provenance.Word(output.cover.rejectedTerrain);provenance.Word(output.cover.rejectedSpacing);
    provenance.Word(output.cover.reachedBudget?1u:0u);
    for(const auto& rejection:output.rejections) {provenance.Word(rejection.sourcePlant);provenance.Word(rejection.sourcePatch);provenance.Word(static_cast<unsigned>(rejection.gate));}
    output.provenanceFingerprint=provenance.value;output.fingerprint=provenance.value;return output;
}
}
inline ForestCoverRetentionResult CreateForestCoverRetained(const ForestCoverResult& canonicalOld,
    const ForestCoverWorldView& original,const ForestCoverWorldView& finalWorld,
    const std::array<ForestCoverAsset,5>& assets,const std::array<std::uint64_t,5>& assetIdentities,
    ForestCoverSettings settings={}){
    return ForestCoverRetentionDetail::CreateRetained(canonicalOld,original,finalWorld,assets,assetIdentities,settings,nullptr);
}
inline ForestCoverRetentionResult CreateForestCoverRetainedFromValidatedOriginal(const ValidatedForestCoverOriginal& token,
    const ForestCoverResult& canonicalOld,const ForestCoverWorldView& original,const ForestCoverWorldView& finalWorld,
    const std::array<ForestCoverAsset,5>& assets,const std::array<std::uint64_t,5>& assetIdentities,
    ForestCoverSettings settings={}){
    return ForestCoverRetentionDetail::CreateRetained(canonicalOld,original,finalWorld,assets,assetIdentities,settings,&token);
}
}
