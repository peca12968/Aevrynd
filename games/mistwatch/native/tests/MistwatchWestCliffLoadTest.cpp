#include "MistwatchLoadPreparation.h"
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
unsigned checks{},rejected{};
void Need(bool ok,const char* text){++checks;if(!ok)throw std::runtime_error(text);}
void Run(){
    using namespace Mistwatch;
    using namespace Aevrynd;
    const auto canonical=CreateTerrain();
    const auto relief=WestCliffRelief::Build(canonical);
    CharacterController3D controller;
    Explorer explorer;Boar boar;OrbitCamera orbit{-.65f,.3f,6};
    SaveGame save;
    const auto original=save.Serialize();
    const auto controllerBefore=controller.StateHash();
    const LoadPreparationInputs live{controller,explorer,boar,orbit};
    const auto prepare=[&](const HeightField3D& field,LoadTerrainProfile profile){
        auto result=PrepareSavedGame(save,live,field,{},{},FirLayoutSelector::ForestWood,
            FallenLogProfile::ScannedStraightV1,nullptr,{},profile);
        Need(save.Serialize()==original&&controller.StateHash()==controllerBefore,"load mutated live input");
        return result;
    };
    auto prior=prepare(canonical,LoadTerrainProfile::Canonical);
    Need(bool(prior),"original scanned world no longer loads");
    auto wrong=prepare(relief.terrain,LoadTerrainProfile::Canonical);
    Need(!wrong,"unqualified derived terrain accepted by old profile");++rejected;
    auto accepted=prepare(relief.terrain,LoadTerrainProfile::WestCliffLedgesV1);
    if(!accepted)std::cerr<<"derived_load_error="<<unsigned(accepted.error)<<'\n';
    Need(bool(accepted),"qualified derived terrain rejected");
    Need(accepted.candidate->snapshot.Serialize()==original,"derived load changed snapshot");
    Need(accepted.candidate->controller.Position().y==relief.terrain.SampleHeight(0,0),"wrong restored ground");
    wrong=prepare(canonical,LoadTerrainProfile::WestCliffLedgesV1);
    Need(!wrong&&wrong.error==LoadPreparationError::InvalidTerrain,"old field accepted under relief profile");++rejected;
    wrong=prepare(relief.terrain,static_cast<LoadTerrainProfile>(99));
    Need(!wrong&&wrong.error==LoadPreparationError::InvalidTerrain,"unknown terrain profile accepted");++rejected;
    for(const auto cell: {std::pair{10u,156u},std::pair{100u,40u},std::pair{0u,156u}}){
        auto changed=relief.terrain;
        Need(changed.SetHeight(cell.first,cell.second,changed.Height(cell.first,cell.second)+.01f),"tamper fixture");
        changed.RecalculateRange();
        wrong=prepare(changed,LoadTerrainProfile::WestCliffLedgesV1);
        Need(!wrong&&wrong.error==LoadPreparationError::InvalidTerrain,"tampered whole-field proof accepted");++rejected;
    }
    auto nonfinite=relief.terrain;
    Need(nonfinite.SetHeight(10,156,(std::numeric_limits<float>::quiet_NaN)()),"nonfinite fixture");
    wrong=prepare(nonfinite,LoadTerrainProfile::WestCliffLedgesV1);
    Need(!wrong&&wrong.error==LoadPreparationError::InvalidTerrain,"nonfinite field accepted");++rejected;
    auto routeMismatch=PrepareSavedGame(save,live,relief.terrain,{},{},FirLayoutSelector::Legacy,
        FallenLogProfile::LegacyCylinder,nullptr,{},LoadTerrainProfile::WestCliffLedgesV1);
    Need(!routeMismatch&&routeMismatch.error==LoadPreparationError::InvalidTerrain,"wrong world accepted relief");++rejected;
    // 候選控制器政策在讀檔準備中保留；非法設定必須在建立可提交狀態前拒絕。
    CharacterControllerSettings3D slopeSettings;slopeSettings.maximumTerrainUphillGrade=1;
    CharacterController3D slopeController(slopeSettings);
    const LoadPreparationInputs slopeLive{slopeController,explorer,boar,orbit};
    auto slopeLoad=PrepareSavedGame(save,slopeLive,relief.terrain,{},{},FirLayoutSelector::ForestWood,
        FallenLogProfile::ScannedStraightV1,nullptr,{},LoadTerrainProfile::WestCliffLedgesV1);
    Need(bool(slopeLoad)&&slopeLoad.candidate->controller.Settings().maximumTerrainUphillGrade==1,
        "load preparation lost terrain uphill policy");
    auto qualifiedSlope=PrepareSavedGame(save,slopeLive,relief.terrain,{},{},FirLayoutSelector::ForestWood,
        FallenLogProfile::ScannedStraightV1,nullptr,{},LoadTerrainProfile::WestCliffSlope45V1);
    Need(bool(qualifiedSlope)&&qualifiedSlope.candidate->controller.Settings().maximumTerrainUphillGrade==1,
        "qualified slope profile rejected or lost policy");
    auto missingSlope=prepare(relief.terrain,LoadTerrainProfile::WestCliffSlope45V1);
    Need(!missingSlope&&missingSlope.error==LoadPreparationError::InvalidControllerSettings,"slope profile accepted disabled policy");++rejected;
    auto wrongSlopeField=PrepareSavedGame(save,slopeLive,canonical,{},{},FirLayoutSelector::ForestWood,
        FallenLogProfile::ScannedStraightV1,nullptr,{},LoadTerrainProfile::WestCliffSlope45V1);
    Need(!wrongSlopeField&&wrongSlopeField.error==LoadPreparationError::InvalidTerrain,"slope profile accepted canonical terrain");++rejected;
    for(float bad:{-1.f,17.f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
        slopeSettings.maximumTerrainUphillGrade=bad;CharacterController3D invalid(slopeSettings);
        const auto hash=invalid.StateHash();const LoadPreparationInputs invalidLive{invalid,explorer,boar,orbit};
        auto invalidLoad=PrepareSavedGame(save,invalidLive,relief.terrain,{},{},FirLayoutSelector::ForestWood,
            FallenLogProfile::ScannedStraightV1,nullptr,{},LoadTerrainProfile::WestCliffLedgesV1);
        Need(!invalidLoad&&invalidLoad.error==LoadPreparationError::InvalidControllerSettings&&invalid.StateHash()==hash,
            "invalid uphill policy accepted or mutated during load");++rejected;
    }
    slopeSettings.maximumTerrainUphillGrade=1;slopeSettings.maximumTerrainStandingGrade=1;
    CharacterController3D standingController(slopeSettings);
    const LoadPreparationInputs standingLive{standingController,explorer,boar,orbit};
    const auto hashBefore=standingController.StateHash();
    auto standing=PrepareSavedGame(save,standingLive,relief.terrain,{},{},FirLayoutSelector::ForestWood,
        FallenLogProfile::ScannedStraightV1,nullptr,{},LoadTerrainProfile::WestCliffStanding45V1);
    Need(bool(standing)&&standing.candidate->controller.Settings().maximumTerrainStandingGrade==1&&
        standing.candidate->controller.Settings().maximumTerrainUphillGrade==1&&standingController.StateHash()==hashBefore,
        "standing profile preserves both policies without mutating live state");
    auto mixed=PrepareSavedGame(save,standingLive,relief.terrain,{},{},FirLayoutSelector::ForestWood,
        FallenLogProfile::ScannedStraightV1,nullptr,{},LoadTerrainProfile::WestCliffSlope45V1);
    Need(!mixed&&mixed.error==LoadPreparationError::InvalidControllerSettings,"old uphill-only profile rejects standing-policy mismatch");++rejected;
    for(float bad:{0.f,.5f,-1.f,17.f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
        slopeSettings.maximumTerrainStandingGrade=bad;CharacterController3D invalid(slopeSettings);
        const auto hash=invalid.StateHash();const LoadPreparationInputs invalidLive{invalid,explorer,boar,orbit};
        auto result=PrepareSavedGame(save,invalidLive,relief.terrain,{},{},FirLayoutSelector::ForestWood,
            FallenLogProfile::ScannedStraightV1,nullptr,{},LoadTerrainProfile::WestCliffStanding45V1);
        Need(!result&&result.error==LoadPreparationError::InvalidControllerSettings&&hash==invalid.StateHash(),
            "standing profile rejects disabled mismatched or invalid policy atomically");++rejected;
    }
    slopeSettings.maximumTerrainStandingGrade=1;slopeSettings.maximumTerrainUphillGrade=0;
    CharacterController3D missingUphill(slopeSettings);
    auto missing=PrepareSavedGame(save,{missingUphill,explorer,boar,orbit},relief.terrain,{},{},FirLayoutSelector::ForestWood,
        FallenLogProfile::ScannedStraightV1,nullptr,{},LoadTerrainProfile::WestCliffStanding45V1);
    Need(!missing&&missing.error==LoadPreparationError::InvalidControllerSettings,"standing profile also requires uphill policy");++rejected;
    std::cout<<"WEST_CLIFF_LOAD_PASS checks="<<checks<<" rejected="<<rejected
        <<" canonical_and_derived_start_load=1 native_F5_F9=0 local_cliff_movement=0\n";
}
}
int main(){try{Run();return 0;}catch(const std::exception& e){std::cerr<<"FIRST_DIVERGENCE: "<<e.what()<<'\n';return 1;}}
