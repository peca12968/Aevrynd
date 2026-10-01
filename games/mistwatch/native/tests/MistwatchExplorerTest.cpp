#include "MistwatchExplorer.h"
#include "MistwatchWorld.h"
#include <iostream>
int main(){
    using namespace Aevrynd;
    Mistwatch::Explorer explorer;
    const auto terrain=Mistwatch::CreateTerrain();
    const auto solids=Mistwatch::WorldSolids(true);
    Vec3 feet{};
    std::array<Vec3,2> previousTargets{};
    std::array<bool,2> previousPlanted{};
    unsigned lockedFrames=0,liftedFrames=0;
    float maximumSlip=0,maximumError=0;
    for(unsigned tick=0;tick<240;++tick){
        const Vec3 movement=tick<180?Vec3{0,0,.025f}:Vec3{};
        feet=feet+movement;
        explorer.Update(movement,1.f/60.f,false);
        if(!explorer.PlaceFeet(feet,true,false,terrain,solids))return 1;
        const auto transform=CreateTransformMatrix(feet,{0,explorer.yaw,0},{1,1,1});
        for(unsigned side=0;side<2;++side){
            const auto ankle=TransformPoint(transform*explorer.pose.globalMatrices[11+side*3],{});
            maximumError=(std::max)(maximumError,Length(ankle-explorer.footTargets[side]));
            if(explorer.planted[side] && previousPlanted[side] && Length(explorer.footTargets[side]-previousTargets[side])<.001f){
                maximumSlip=(std::max)(maximumSlip,Length(ankle-previousTargets[side]));++lockedFrames;
            }
            if(!explorer.planted[side] && ankle.y>.16f)++liftedFrames;
            previousTargets[side]=explorer.footTargets[side];previousPlanted[side]=explorer.planted[side];
        }
    }
    std::cout<<"Foot measurements locked="<<lockedFrames<<" lifted="<<liftedFrames<<" slip="<<maximumSlip<<" error="<<maximumError<<"\n";
    if(maximumSlip>.003f || maximumError>.003f || lockedFrames<50 || liftedFrames<20)return 2;
    // 河谷的地形低於橋面，腳底必須採橋面而不是向下探到河底。
    feet={0,0,36};explorer.Update({},1.f/60.f,false);
    if(!explorer.PlaceFeet(feet,true,false,terrain,solids))return 3;
    for(unsigned side=0;side<2;++side)if(std::abs(explorer.footTargets[side].y-.1f)>.001f)return 4;
    // 空中解除支撐，重回地面才重新取接觸點。
    if(!explorer.PlaceFeet({0,2,36},false,false,terrain,solids) || explorer.planted[0] || explorer.planted[1])return 5;
    explorer.Update({},1.f/60.f,true);
    if(!explorer.PlaceFeet(feet,true,true,terrain,solids))return 6;
    SkinningWorkspace3D workspace;Mesh3D posed;
    if(!SkinModelInto(explorer.model,explorer.pose,workspace,posed))return 7;
    if(posed.Bounds().center.y-posed.Bounds().extents.y<-.02f)return 8;
    if(posed.Bounds().center.y+posed.Bounds().extents.y>1.4f)return 9;
    std::cout<<"Explorer IK PASS lockedFrames="<<lockedFrames<<" liftedFrames="<<liftedFrames
        <<" maxSlip="<<maximumSlip<<" maxTargetError="<<maximumError<<"\n";
}
