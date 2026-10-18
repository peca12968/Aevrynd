#include "../fallen/MistwatchCurrentWorldExpected.h"
#include <iostream>

namespace C=::Mistwatch::CurrentWorldExpected;
namespace Q=::Mistwatch::AuthoredQualification;
namespace W=::Mistwatch::FallenWorldSeat;
namespace {
unsigned negatives{};
template<class Action>void Reject(Action&& action,const std::string& name,std::string_view expected){
    bool rejected{};try{std::forward<Action>(action)();}catch(const std::exception& error){
        if(std::string_view(error.what())!=expected)throw std::runtime_error("CURRENT_NEGATIVE_FIRST_DIVERGENCE name="+name+
            " actual="+error.what()+" expected="+std::string(expected));rejected=true;}
    Q::Detail::Need(rejected,"CURRENT_EXPECTED_NEGATIVE_ACCEPTED");++negatives;
    std::cout<<"CONTROL name="<<name<<" reason="<<expected<<" rejected=true\n";
}
std::vector<W::Placement> FixedMappings(){std::vector<W::Placement> out;
    // 此小測試只組合作者 context；正式 producer 仍必須先通過四倒木完整 overlay。
    for(const auto& p:Mistwatch::FallenLogPlacements()){W::Placement item;item.propId=p.propId;item.anchor=p.authoredAnchor;
        item.pose=p.pose;item.collider=p.collider;out.push_back(std::move(item));}return out;}
void Maps(const C::Result& expected){
    const std::array<C::Contract C::Result::*,5> members{&C::Result::legacy,&C::Result::fields,&C::Result::extras,&C::Result::surface,&C::Result::artProfiles};
    for(unsigned map=0;map<members.size();++map){const auto member=members[map];const auto& canonical=expected.*member;
        for(const auto& [key,value]:canonical){auto changed=expected;(changed.*member).erase(key);
            // 153 欄的投影先驗證完整性；其他 map 走既有嚴格鍵型別比較。
            const auto missing=(map<2)?"CURRENT_EXPECTED_153_KEYS":"AUTHORED_EXPECTED_KEY_COUNT";
            Reject([&]{C::ValidateFields(changed,expected);},"missing_"+std::to_string(map)+"_"+key,missing);
            changed=expected;(changed.*member)[key]=value+" ";
            const bool preserved=map==0||map==1;
            const bool projection=std::find(C::ProjectedKeys.begin(),C::ProjectedKeys.end(),key)!=C::ProjectedKeys.end();
            const auto reason=preserved&&!projection?"CURRENT_EXPECTED_UNREVIEWED_PROJECTION":"AUTHORED_EXPECTED_VALUE_OR_TYPE";
            Reject([&]{C::ValidateFields(changed,expected);},"changed_"+std::to_string(map)+"_"+key,reason);
        }
    }
    auto changed=expected;changed.fields["injected_field"]="1";
    Reject([&]{C::ValidateFields(changed,expected);},"extra_projection_key","CURRENT_EXPECTED_153_KEYS");
    changed=expected;changed.fields["shrubs_plants"]="64";
    Reject([&]{C::ValidateFields(changed,expected);},"old_64_impersonates_128","AUTHORED_EXPECTED_VALUE_OR_TYPE");
    changed=expected;changed.fields["planked_workshop_profile"]="planked-weathered-bands-v1";
    Reject([&]{C::ValidateFields(changed,expected);},"old_workshop_impersonates_frame","AUTHORED_EXPECTED_VALUE_OR_TYPE");
    changed=expected;changed.extras["canopy_cleanup_mesh_destroy_calls"]="7";
    Reject([&]{C::ValidateFields(changed,expected);},"old_four_tree_cleanup","AUTHORED_EXPECTED_VALUE_OR_TYPE");
    changed=expected;changed.suppressedGrass.push_back(3600);
    Reject([&]{C::ValidateFields(changed,expected);},"suppression_outside_population","CURRENT_EXPECTED_SUPPRESSION_IDS");
    changed=expected;changed.suppressedBroadleaf.push_back(96);
    Reject([&]{C::ValidateFields(changed,expected);},"broadleaf_suppression_outside_population","CURRENT_EXPECTED_SUPPRESSION_IDS");
    changed=expected;changed.surface["height_layer_mask"]="0";
    Reject([&]{C::ValidateFields(changed,expected);},"height_layers_disabled","AUTHORED_EXPECTED_VALUE_OR_TYPE");
}
void Profiles(const C::Result& expected){
    using F=Mistwatch::PlayableArt::Feature;
    constexpr std::array features{F::Shelter,F::Workshop,F::Road,F::Roadside,F::Understory,F::BoulderColor,F::BoulderGeometry,F::Shrubs,F::LandscapeTwig,F::LandscapeStand};
    constexpr std::array<const char*,10> names{"Shelter","Workshop","Road","Roadside","Understory","BoulderColor","BoulderGeometry","Shrubs","LandscapeTwig","LandscapeStand"};
    for(unsigned i=0;i<features.size();++i){const auto& token=expected.artProfiles.at(names[i]);std::wstring wide(token.begin(),token.end());
        Q::Detail::Need(Mistwatch::PlayableArt::Resolve(false,features[i],L"stale-inherited-value")==wide&&
            Mistwatch::PlayableArt::Resolve(true,features[i],wide)==wide,"CURRENT_EXPECTED_ACTUAL_PROFILE_RESOLUTION");}
}
}
int wmain(int argc,wchar_t** argv){try{
    Q::Detail::Need(argc==3&&std::wstring_view(argv[1])==L"--runtime-assets","USAGE: MistwatchCurrentWorldExpectedTest --runtime-assets ABS");
    const auto root=std::filesystem::path(argv[2]).lexically_normal();Q::Detail::Need(root.is_absolute(),"CURRENT_EXPECTED_ABSOLUTE_RUNTIME");
    W::Budget budget;const auto assets=Q::LoadAssets(root);const auto terrain=Mistwatch::CreateTerrain();
    const auto source=Q::CreateSourceContext(terrain,assets,budget);
    const auto context=Q::CreateContext(terrain,assets,source,FixedMappings(),budget);
    const auto seal=Q::ContextFingerprint(context),assetSeal=Q::AssetsFingerprint(assets);
    const auto current=C::Build(context,terrain,assets,budget,root),repeated=C::Build(context,terrain,assets,budget,root);
    C::ValidateFields(current,repeated);
    Q::Detail::Need(current.fields.size()==153&&current.legacy.size()==153&&current.surface.size()==16&&current.artProfiles.size()==10&&
        current.legacy.at("shrubs_plants")=="64"&&current.fields.at("shrubs_plants")=="128"&&
        current.fields.at("canopy_collision_parts")=="8617"&&current.fields.at("canopy_plants")=="8"&&
        current.extras.at("canopy_cleanup_mesh_destroy_calls")=="11"&&current.extras.at("boulder_instances")=="29"&&
        current.extras.at("boulder_mesh_vertices")=="81373"&&current.extras.at("boulder_mesh_triangles")=="66122"&&
        current.fields.at("planked_workshop_vertices")=="12480"&&current.fields.at("planked_workshop_indices")=="17160"&&
        current.fields.at("ground_habitat_layer_source")=="unrecognized","CURRENT_EXPECTED_COMPLETE_RECIPE");
    Maps(current);Profiles(current);
    auto changed=context;changed.canopyMetadata.faceChecks++;
    Reject([&]{(void)C::Build(changed,terrain,assets,budget,root);},"changed_author_context","AUTHORED_CONTEXT_FP_CHANGED");
    Reject([&]{(void)C::Build(context,terrain,assets,budget,"relative");},"relative_runtime","CURRENT_EXPECTED_ABSOLUTE_RUNTIME");
    bool harnessRejected{};const auto previous=negatives;
    try{Reject([]{throw std::runtime_error("unexpected_IO_or_budget");},"wrong_reason","AUTHORED_EXPECTED_VALUE_OR_TYPE");}
    catch(const std::exception& error){harnessRejected=std::string_view(error.what()).starts_with("CURRENT_NEGATIVE_FIRST_DIVERGENCE");}
    Q::Detail::Need(harnessRejected&&negatives==previous,"CURRENT_EXPECTED_HARNESS_FALSE_NEGATIVE");
    Q::Detail::Need(seal==Q::ContextFingerprint(context)&&assetSeal==Q::AssetsFingerprint(assets)&&
        context.shrubs.plants.size()==64&&context.worlds[0].size()==8853&&context.worlds[1].size()==8854,
        "CURRENT_EXPECTED_LEGACY_INPUT_MUTATED");
    for(const auto& [key,value]:current.extras)std::cout<<"CURRENT_EXTRA "<<key<<'='<<value<<'\n';
    std::cout<<"CURRENT_WORLD_EXPECTED_CPU_PASS fields="<<current.fields.size()<<" extras="<<current.extras.size()
        <<" surface_fields="<<current.surface.size()<<" art_profiles="<<current.artProfiles.size()<<" negatives="<<negatives
        <<" native=false GPU=false full49=false manual_play=false\n";return 0;
}catch(const std::exception& error){std::cerr<<"CURRENT_WORLD_EXPECTED_FIRST_DIVERGENCE "<<error.what()<<'\n';return 1;}}
