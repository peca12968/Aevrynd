#include "../fallen/MistwatchAuthoredNativeExpected.h"
#include <iostream>
#include <functional>

namespace N=::Mistwatch::AuthoredNativeExpected;
namespace Q=::Mistwatch::AuthoredQualification;
namespace W=::Mistwatch::FallenWorldSeat;
namespace {
unsigned negatives{};
template<class Action>void Reject(Action&& action,std::string_view name,std::string_view expected){
    bool rejected{};try{std::forward<Action>(action)();}catch(const std::exception& error){
        // 只有這個案例的確切語義 gate 才是成功；預算、時間、IO 與環境分歧一律失敗。
        if(std::string_view(error.what())!=expected)throw std::runtime_error(std::string("NEGATIVE_FIRST_DIVERGENCE name=")+
            std::string(name)+" actual="+error.what()+" expected="+std::string(expected));
        rejected=true;std::cout<<"CONTROL "<<name<<" rejected=true reason="<<error.what()<<'\n';}
    Q::Detail::Need(rejected,"AUTHORED_NATIVE_EXPECTED_NEGATIVE_ACCEPTED");++negatives;
}
void RejectionHarnessControls(){
    for(const auto failure:{std::string_view("WORLD_SEAT_OPERATION_LIMIT_UNRESOLVED"),
        std::string_view("WORLD_SEAT_TIME_LIMIT_UNRESOLVED"),std::string_view("AUTHORED_EXPECTED_SOURCE_OPEN"),
        std::string_view("injected_environment_failure")}){
        const auto before=negatives;bool failedCorrectly{};
        try{Reject([&]{throw std::runtime_error(std::string(failure));},"harness_wrong_reason","AUTHORED_EXPECTED_KEY_COUNT");}
        catch(const std::exception& error){const auto expected=std::string("NEGATIVE_FIRST_DIVERGENCE name=harness_wrong_reason actual=")+
            std::string(failure)+" expected=AUTHORED_EXPECTED_KEY_COUNT";failedCorrectly=error.what()==expected;}
        Q::Detail::Need(failedCorrectly&&negatives==before,"AUTHORED_NATIVE_EXPECTED_HARNESS_FALSE_NEGATIVE");
        std::cout<<"HARNESS failure="<<failure<<" counted_as_negative=false\n";}
}
std::vector<W::Placement> FixedMappings(){std::vector<W::Placement> result;
    // 本測試不替代四倒木 full-overlay 資格；只使用 production 固定 pose 作 context。
    for(const auto& p:Mistwatch::FallenLogPlacements()){W::Placement q;q.propId=p.propId;q.anchor=p.authoredAnchor;
        q.pose=p.pose;q.collider=p.collider;result.push_back(std::move(q));}return result;
}
N::FernSubmission IndependentFerns(const Q::Context& c,const Q::Assets& assets,float z){
    N::FernSubmission result;
    // 原渲染 policy 的最遠 38m，只做獨立距離與 source ordinal%4，不調用被測 scene helper。
    for(std::size_t i=0;i<c.author.understory.ferns.size();++i){const auto p=c.author.understory.ferns[i].position;
        const float dx=p.x,dy=p.y,dz=p.z-z;const float distance=std::sqrt(dx*dx+dy*dy+dz*dz);
        if(distance>=38.f)continue;const auto variant=i%4;++result.plants;++result.variants[variant];
        result.triangles+=assets.fern[variant].indices.size()/3;}return result;
}
void FieldControls(const N::Contract& canonical){
    for(const auto& [key,value]:canonical){auto copy=canonical;copy.erase(key);
        Reject([&]{N::ValidateFields(copy,canonical);},"missing_"+key,"AUTHORED_EXPECTED_KEY_COUNT");
        copy=canonical;copy[key]=value+" ";Reject([&]{N::ValidateFields(copy,canonical);},"noncanonical_"+key,"AUTHORED_EXPECTED_VALUE_OR_TYPE");
        copy=canonical;copy[key]="NaN";Reject([&]{N::ValidateFields(copy,canonical);},"wrong_type_"+key,"AUTHORED_EXPECTED_VALUE_OR_TYPE");}
    auto copy=canonical;copy.emplace("unreviewed_extra_key","1");Reject([&]{N::ValidateFields(copy,canonical);},"extra_key","AUTHORED_EXPECTED_KEY_COUNT");
    copy=canonical;copy["canopy_collision_parts"]="8670";Reject([&]{N::ValidateFields(copy,canonical);},"old_four_times_two","AUTHORED_EXPECTED_VALUE_OR_TYPE");
    copy=canonical;copy["forest_cover_provenance_fingerprint"]=copy.at("forest_cover_original_fingerprint");
    Reject([&]{N::ValidateFields(copy,canonical);},"old_grass_impersonates_retention","AUTHORED_EXPECTED_VALUE_OR_TYPE");
    copy=canonical;copy["ground_texture_layers"]="4";Reject([&]{N::ValidateFields(copy,canonical);},"invented_fourth_material_layer","AUTHORED_EXPECTED_VALUE_OR_TYPE");
    copy=canonical;copy["ground_loaded_texture_identity"]="1.234e19";
    Reject([&]{N::ValidateFields(copy,canonical);},"uint64_scientific_notation","AUTHORED_EXPECTED_VALUE_OR_TYPE");
    copy=canonical;copy["canopy_population_fingerprint"]="000"+copy.at("canopy_population_fingerprint");
    Reject([&]{N::ValidateFields(copy,canonical);},"uint64_leading_zero","AUTHORED_EXPECTED_VALUE_OR_TYPE");
    copy=canonical;copy["forest_cover_retained"]="true";Reject([&]{N::ValidateFields(copy,canonical);},"count_boolean","AUTHORED_EXPECTED_VALUE_OR_TYPE");
    copy=canonical;copy["world_profile"]+="\r";Reject([&]{N::ValidateFields(copy,canonical);},"embedded_carriage_return","AUTHORED_EXPECTED_FIELD_TYPE");
    copy=canonical;copy["world_profile"].clear();Reject([&]{N::ValidateFields(copy,canonical);},"empty_field","AUTHORED_EXPECTED_FIELD_BOUND");
    copy=canonical;copy.erase("world_profile");copy.emplace("unreviewed_replacement_key","forest-wood");
    Reject([&]{N::ValidateFields(copy,canonical);},"missing_key_with_same_count","AUTHORED_EXPECTED_MISSING_KEY");
}
void Scenes(const Q::Context& c,const Aevrynd::HeightField3D& terrain,const Q::Assets& assets){
    for(const auto scene:{std::string_view("initial"),std::string_view("river-overlook")}){
        const auto actual=N::SceneOriginalFerns(c,terrain,assets,scene),independent=IndependentFerns(c,assets,scene=="initial"?0.f:30.f);
        Q::Detail::Need(actual.plants==independent.plants&&actual.triangles==independent.triangles&&actual.variants==independent.variants,
            "AUTHORED_NATIVE_EXPECTED_SCENE_ORACLE");
        Q::Detail::Need(actual.plants==std::accumulate(actual.variants.begin(),actual.variants.end(),std::uint64_t{}),
            "AUTHORED_NATIVE_EXPECTED_SCENE_SUM");
        const auto json=N::SceneJson(actual);Q::Detail::Need(json.find("\"plants\":\"")!=std::string::npos&&
            json.find("\"triangles\":\"")!=std::string::npos,"AUTHORED_NATIVE_EXPECTED_SCENE_CANONICAL_STRINGS");
        std::cout<<"SCENE "<<scene<<" original_ferns="<<json<<'\n';}
    Reject([&]{(void)N::SceneOriginalFerns(c,terrain,assets,"walk");},"nonfixed_scene","AUTHORED_EXPECTED_FIXED_SCENE");
}
void SmallNegativeControls(const std::filesystem::path& root){
    const auto definition=Mistwatch::HabitatGroundDefinition(true,true);auto pin=definition.textures.integrity[0];++pin.expectedBytes;
    Reject([&]{N::Detail::VerifyMap(root,definition.textures.baseColorPath,pin);},"material_wrong_bytes","AUTHORED_EXPECTED_SOURCE_BYTES");
    pin=definition.textures.integrity[0];pin.expectedSha256[0]=pin.expectedSha256[0]=='0'?'1':'0';
    Reject([&]{N::Detail::VerifyMap(root,definition.textures.baseColorPath,pin);},"material_wrong_sha","AUTHORED_EXPECTED_SOURCE_SHA");
    pin=definition.textures.integrity[0];Reject([&]{N::Detail::VerifyMap(root,"../escaped.jpg",pin);},"material_path_escape","AUTHORED_EXPECTED_SOURCE_PATH");
    auto ground=definition;ground.textures.maximumResolution=0;
    Reject([&]{(void)N::Detail::MaterialSources(ground,root);},"material_zero_resolution","AUTHORED_EXPECTED_GROUND_RECIPE");
    ground=definition;ground.textures.alphaMode=Aevrynd::MaterialAlphaMode3D::Mask;
    Reject([&]{(void)N::Detail::MaterialSources(ground,root);},"material_wrong_alpha_mode","AUTHORED_EXPECTED_GROUND_RECIPE");
    ground=definition;ground.textures.additionalLayers[0].normalPath.clear();
    Reject([&]{(void)N::Detail::MaterialSources(ground,root);},"material_missing_normal","AUTHORED_EXPECTED_ASSET_ROOT");
    ground=definition;ground.material.normalScale=std::numeric_limits<float>::quiet_NaN();
    Reject([&]{(void)N::Detail::MaterialSources(ground,root);},"material_normal_scale_nan","AUTHORED_EXPECTED_NONFINITE");
    auto mesh=Mistwatch::ShelterWeatheredUv::Create();mesh.vertices[0].u=std::numeric_limits<float>::quiet_NaN();
    Reject([&]{(void)N::Detail::Uv(mesh);},"shelter_uv_nan","AUTHORED_NONFINITE");
    mesh=Mistwatch::WorkshopVisual::Create();mesh.indices[0]=static_cast<std::uint32_t>(mesh.vertices.size());
    Reject([&]{(void)N::Detail::Mesh(mesh);},"workshop_bad_index","AUTHORED_ASSET_TOPOLOGY");
    Reject([&]{(void)N::Detail::Scalar(std::numeric_limits<float>::infinity());},"material_scalar_infinite","AUTHORED_EXPECTED_NONFINITE");
}
void ForestSurfaceControls(const std::filesystem::path& root){
    const auto canonical=N::BuildForestSurface(root),repeated=N::BuildForestSurface(root);
    Q::Detail::Need(canonical==repeated&&canonical.size()==16,"SURFACE_EXPECTED_REPEAT_AND_KEYS");
    auto actual=Mistwatch::HabitatGroundDefinition(true,true);
    Aevrynd::TerrainDistanceMaterialSettings3D distance;distance.normalDetailStrength=.65f;distance.mesoPartitionEnabled=false;
    namespace P=::Mistwatch::ForestSurfaceProfile;
    const P::Selection selected{P::Mode::Capture,P::Profile::ForestMossLeafMetricV1};
    P::Apply(actual,distance,selected);const auto parallax=P::HeightSettings(selected);
    N::ValidateForestSurfaceRecipe(actual,distance,parallax);
    Q::Detail::Need(canonical.at("ground_loaded_texture_identity")==std::to_string(N::Detail::MaterialSources(actual,root/"mistwatch-materials"))&&
        canonical.at("parallax_settings_fingerprint")==std::to_string(Aevrynd::FingerprintTerrainParallaxSettings3D(parallax)),
        "SURFACE_EXPECTED_INDEPENDENT_IDENTITY");
    for(const auto& [key,value]:canonical){auto copy=canonical;copy.erase(key);
        Reject([&]{N::ValidateFields(copy,canonical);},"surface_missing_"+key,"AUTHORED_EXPECTED_KEY_COUNT");
        copy=canonical;copy[key]=value+" ";Reject([&]{N::ValidateFields(copy,canonical);},"surface_noncanonical_"+key,"AUTHORED_EXPECTED_VALUE_OR_TYPE");
        copy=canonical;copy[key]="NaN";Reject([&]{N::ValidateFields(copy,canonical);},"surface_type_"+key,"AUTHORED_EXPECTED_VALUE_OR_TYPE");}
    auto extra=canonical;extra.emplace("current_geometry_qualified","1");
    Reject([&]{N::ValidateFields(extra,canonical);},"surface_invented_geometry_pass","AUTHORED_EXPECTED_KEY_COUNT");
    for(unsigned layer=0;layer<2;++layer){
        auto copy=actual;auto& pin=layer?copy.textures.additionalLayers[0].integrity[1]:copy.textures.integrity[1];++pin.expectedBytes;
        Reject([&]{N::ValidateForestSurfaceRecipe(copy,distance,parallax);},"surface_pin_bytes","SURFACE_EXPECTED_PIN");
        copy=actual;auto& sha=layer?copy.textures.additionalLayers[0].integrity[1].expectedSha256:copy.textures.integrity[1].expectedSha256;
        sha[0]=sha[0]=='0'?'1':'0';Reject([&]{N::ValidateForestSurfaceRecipe(copy,distance,parallax);},"surface_pin_sha","SURFACE_EXPECTED_PIN");
        copy=actual;(layer?copy.textures.additionalLayers[0].roughnessPath:copy.textures.roughnessPath)="unreviewed.png";
        Reject([&]{N::ValidateForestSurfaceRecipe(copy,distance,parallax);},"surface_path","SURFACE_EXPECTED_BINDING");
        copy=actual;(layer?copy.textures.additionalLayers[0].surfaceEncoding:copy.textures.surfaceEncoding)=Aevrynd::MaterialSurfaceMapEncoding3D::PackedOrm;
        Reject([&]{N::ValidateForestSurfaceRecipe(copy,distance,parallax);},"surface_orm_without_height","SURFACE_EXPECTED_BINDING");}
    auto changed=distance;changed.soilLayerScale=1.21f;
    Reject([&]{N::ValidateForestSurfaceRecipe(actual,changed,parallax);},"surface_old_soil","SURFACE_EXPECTED_DISTANCE");
    changed=distance;changed.soilLayerScale=std::nextafter(1.75f,0.f);
    Reject([&]{N::ValidateForestSurfaceRecipe(actual,changed,parallax);},"surface_soil_one_ulp","SURFACE_EXPECTED_DISTANCE");
    changed=distance;changed.normalDetailStrength=std::numeric_limits<float>::quiet_NaN();
    Reject([&]{N::ValidateForestSurfaceRecipe(actual,changed,parallax);},"surface_settings_nan","SURFACE_EXPECTED_DISTANCE");
    auto height=parallax;height.enabled=false;
    Reject([&]{N::ValidateForestSurfaceRecipe(actual,distance,height);},"surface_disabled_POM","SURFACE_EXPECTED_PARALLAX");
    height=parallax;height.depthMeters=std::nextafter(.015f,0.f);
    Reject([&]{N::ValidateForestSurfaceRecipe(actual,distance,height);},"surface_POM_one_ulp","SURFACE_EXPECTED_PARALLAX");
    for(const auto& [key,value]:canonical)std::cout<<"SURFACE_EXPECTED "<<key<<'='<<value<<'\n';
    std::cout<<"FOREST_SURFACE_EXPECTED_CPU_PASS fields="<<canonical.size()<<" negatives="<<negatives
        <<" current_geometry_qualified=false native=false GPU=false full49=false\n";
}
}
int wmain(int argc,wchar_t** argv){try{
    Q::Detail::Need((argc==3||argc==5)&&std::wstring_view(argv[1])==L"--runtime-assets","USAGE: MistwatchAuthoredNativeExpectedTest --runtime-assets ABS [--surface-profile forest-moss-leaf-metric-v1]");
    const auto root=std::filesystem::path(argv[2]).lexically_normal();Q::Detail::Need(root.is_absolute(),"AUTHORED_EXPECTED_ASSET_ROOT");
    RejectionHarnessControls();
    if(argc==5){Q::Detail::Need(std::wstring_view(argv[3])==L"--surface-profile"&&std::wstring_view(argv[4])==L"forest-moss-leaf-metric-v1",
        "SURFACE_EXPECTED_EXPLICIT_PROFILE");ForestSurfaceControls(root);return 0;}
    W::Budget budget;const auto assets=Q::LoadAssets(root);const auto terrain=Mistwatch::CreateTerrain();
    const auto source=Q::CreateSourceContext(terrain,assets,budget);const auto context=Q::CreateContext(terrain,assets,source,FixedMappings(),budget);
    const auto canonical=N::Build(context,terrain,assets,budget,root);
    const auto repeated=N::Build(context,terrain,assets,budget,root);Q::Detail::Need(canonical==repeated,"AUTHORED_NATIVE_EXPECTED_REPEAT_CHANGED");
    N::Validate(canonical,context,terrain,assets,budget,root);FieldControls(canonical);Scenes(context,terrain,assets);SmallNegativeControls(root/"mistwatch-materials");
    Q::Detail::Need(canonical.size()==153&&canonical.at("canopy_collision_parts")=="8617"&&canonical.at("canopy_plants")=="8"&&
        canonical.at("fir_layout_population")=="95"&&canonical.at("middle_layer_population")=="23"&&canonical.at("understory_saplings")=="40"&&
        canonical.at("ground_cover_instances")=="3600"&&canonical.at("ground_texture_layers")=="3"&&canonical.at("grass_shape_profile")=="source-geometry"&&
        canonical.at("understory_mix_profile")=="original","AUTHORED_NATIVE_EXPECTED_CURRENT_WORLD");
    auto changed=context;changed.canopyMetadata.faceChecks++;
    Reject([&]{(void)N::Build(changed,terrain,assets,budget,root);},"context_canopy_metadata_changed","AUTHORED_CONTEXT_FP_CHANGED");
    changed=context;changed.author.understory.ferns[114].position.x=std::numeric_limits<float>::quiet_NaN();
    Reject([&]{N::Validate(canonical,changed,terrain,assets,budget,root);},"context_fern_nan","AUTHORED_FERN_SOURCE_BITS");
    changed=context;changed.retention.cover.plants[0].radius=std::numeric_limits<float>::infinity();
    Reject([&]{(void)N::Build(changed,terrain,assets,budget,root);},"context_grass_radius_infinite","AUTHORED_RETENTION_CANONICAL_POSE");
    auto corruptedAssets=assets;corruptedAssets.canopy[0].vertices[0].x=std::numeric_limits<float>::quiet_NaN();
    Reject([&]{(void)N::Build(context,terrain,corruptedAssets,budget,root);},"canopy_mesh_nonfinite","AUTHORED_NONFINITE");
    corruptedAssets=assets;corruptedAssets.grass[0].vertices[0].u=std::nextafter(corruptedAssets.grass[0].vertices[0].u,INFINITY);
    corruptedAssets.grassIdentities[0]=Q::Detail::MeshIdentity(corruptedAssets.grass[0]);
    corruptedAssets.fingerprint=Q::AssetsFingerprint(corruptedAssets);
    Reject([&]{N::Detail::PinnedAssets(corruptedAssets,root);},"forged_asset_fingerprint_not_raw_pin","AUTHORED_EXPECTED_PINNED_ASSETS");
    // SetHeight 是原始高度編輯入口；既有視線測試亦用它注入壞資料，資格層必須拒絕。
    auto changedTerrain=terrain;Q::Detail::Need(changedTerrain.SetHeight(0,0,std::numeric_limits<float>::quiet_NaN()),
        "AUTHORED_NATIVE_EXPECTED_HEIGHTFIELD_INJECTION_FAILED");
    Reject([&]{(void)N::Build(context,changedTerrain,assets,budget,root);},"terrain_nan","CURRENT_TERRAIN_FINGERPRINT_FAILED");
    changedTerrain=terrain;Q::Detail::Need(changedTerrain.SetHeight(0,0,std::numeric_limits<float>::infinity()),
        "AUTHORED_NATIVE_EXPECTED_HEIGHTFIELD_INJECTION_FAILED");
    Reject([&]{(void)N::Build(context,changedTerrain,assets,budget,root);},"terrain_infinite","CURRENT_TERRAIN_FINGERPRINT_FAILED");
    changedTerrain=terrain;
    Q::Detail::Need(changedTerrain.SetHeight(0,0,std::nextafter(changedTerrain.Height(0,0),INFINITY)),"AUTHORED_NATIVE_EXPECTED_HEIGHTFIELD_EDIT");
    changedTerrain.RecalculateRange();Reject([&]{(void)N::Build(context,changedTerrain,assets,budget,root);},"terrain_one_ulp","CURRENT_TERRAIN_FINGERPRINT_FAILED");
    Reject([&]{(void)N::Build(context,terrain,assets,budget,"relative-assets");},"relative_runtime_root","AUTHORED_EXPECTED_ASSET_ROOT");
    for(const auto& [key,value]:canonical)std::cout<<"EXPECTED "<<key<<'='<<value<<'\n';
    std::cout<<"AUTHORED_NATIVE_EXPECTED_CPU_PASS fields="<<canonical.size()<<" negatives="<<negatives<<" native=false full_overlay=false\n";return 0;
}catch(const std::exception& e){std::cerr<<"AUTHORED_NATIVE_EXPECTED_FIRST_DIVERGENCE "<<e.what()<<'\n';return 1;}}
