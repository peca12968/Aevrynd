#include "MistwatchWorld.h"
#include "MistwatchExplorer.h"
#include "MistwatchPlayerLocomotion.h"
#include "MistwatchForestMaterial.h"
#include <Aevrynd/CharacterController3D.h>
#include <iostream>
#include <limits>
int main() {
    const auto foliage=Mistwatch::FirTwigMaterial();
    if(foliage.alphaMode!=Aevrynd::MaterialAlphaMode3D::Mask || foliage.alphaCutoff!=.25f ||
       !foliage.doubleSided || !foliage.castsShadow || !foliage.useUv || foliage.textureScale!=1.f ||
       foliage.materialClass!=Aevrynd::MaterialClass3D::Foliage)return 31;
    Mistwatch::Explorer explorer;
    if(!explorer.model.Valid() || explorer.model.skeleton.joints.size()!=15)return 25;
    Aevrynd::SkinningWorkspace3D skinning;
    Aevrynd::Mesh3D posed;
    if(!Aevrynd::SkinModelInto(explorer.model,explorer.pose,skinning,posed))return 26;
    if(posed.Bounds().extents.y<.8f || posed.Bounds().extents.y>.95f)return 27;
    // 正式所有者逐步轉身並提交足部支撐；不再要求單幀瞬轉九十度。
    Aevrynd::HeightField3D poseTerrain{{{-8,0,-8},32,32,1}};poseTerrain.Fill(0);
    Aevrynd::CharacterController3D poseActor;poseActor.Reset({});
    Mistwatch::Evasion poseEvasion;Mistwatch::PlayerLocomotion poseOwner;
    if(!poseActor.Step({}, {},{},poseTerrain,Mistwatch::LocomotionMotor::FixedStep)||
       !explorer.PlaceFeet(poseActor.Position(),poseActor.Grounded(),false,poseTerrain,{}))return 28;
    Mistwatch::PlayerLocomotionInput turn;turn.movement={1,0,0};
    const auto initialLeg=explorer.pose.globalMatrices[9];
    bool animated=false;
    for(unsigned tick=0;tick<120;++tick){
        const float previousYaw=explorer.yaw;
        const auto result=poseOwner.Advance(poseActor,poseEvasion,explorer,turn,1.f/60.f,{}, {},poseTerrain,[]{return true;});
        if(!result.accepted||!std::isfinite(explorer.yaw)||
           std::abs(std::remainder(explorer.yaw-previousYaw,6.2831853f))>9.42477796f/60.f+.00001f){
            std::cerr<<"FIRST_DIVERGENCE owner turn tick="<<tick<<" pose_line="<<result.poseFailureLine<<'\n';return 28;}
        for(unsigned row=0;row<4;++row)for(unsigned column=0;column<4;++column)
            animated|=std::abs(explorer.pose.globalMatrices[9][row][column]-initialLeg[row][column])>.0001f;
        if(!Aevrynd::SkinModelInto(explorer.model,explorer.pose,skinning,posed))return 29;
    }
    if(std::abs(std::remainder(explorer.yaw-1.57079637f,6.2831853f))>.05f||poseActor.Position().x<.5f||!animated)return 28;
    std::cout<<"Bounded owner turn PASS yaw="<<explorer.yaw<<" root_x="<<poseActor.Position().x<<'\n';
    if(!Aevrynd::SkinModelInto(explorer.model,explorer.pose,skinning,posed))return 29;
    Mistwatch::PlayerLocomotionInput crouch;crouch.crouched=true;
    const auto crouchResult=poseOwner.Advance(poseActor,poseEvasion,explorer,crouch,1.f/60.f,{}, {},poseTerrain,[]{return true;});
    if(!crouchResult.accepted)return 30;
    if(!Aevrynd::SkinModelInto(explorer.model,explorer.pose,skinning,posed) || posed.Bounds().center.y+posed.Bounds().extents.y>1.4f)return 30;
    const auto terrain=Mistwatch::CreateTerrain();
    Aevrynd::CharacterController3D crouching;
    crouching.Reset({0,0,0});
    const std::vector<Aevrynd::BoxCollider3D> lowRoof{{{0,1.5f,0},{2,.1f,2}}};
    if(!crouching.TrySetHeight(1.1f,lowRoof))return 16;
    const auto beforeStand=crouching.StateHash();
    if(crouching.TrySetHeight(1.8f,lowRoof) || crouching.Settings().height!=1.1f || crouching.StateHash()!=beforeStand)return 17;
    crouching.Reset({4,0,0});
    if(!crouching.TrySetHeight(1.8f,lowRoof) || crouching.Settings().height!=1.8f)return 18;
    const std::vector<Aevrynd::BoxCollider3D> floor{{{4,-.1f,0},{2,.1f,2}}};
    if(!crouching.TrySetHeight(1.1f,floor) || !crouching.TrySetHeight(1.8f,floor))return 19;
    const auto unchanged=crouching.StateHash();
    if(!crouching.TrySetHeight(1.8f,floor) || crouching.StateHash()!=unchanged)return 20;
    if(crouching.TrySetHeight(std::numeric_limits<float>::quiet_NaN(),floor) ||
       crouching.TrySetHeight(.5f,floor) || crouching.TrySetHeight(3.1f,floor) ||
       crouching.Settings().height!=1.8f || crouching.StateHash()!=unchanged)return 21;
    // 使用實際移動穿過低矮空間，再確認離開後才可站起。
    crouching.Reset({0,0,0});
    if(!crouching.TrySetHeight(1.1f,lowRoof))return 22;
    for(int tick=0;tick<45;++tick)crouching.Step({{1,0,0},false,.45f},lowRoof,terrain,1.f/60.f);
    if(crouching.TrySetHeight(1.8f,lowRoof))return 23;
    for(int tick=0;tick<30;++tick)crouching.Step({{1,0,0},false,.45f},lowRoof,terrain,1.f/60.f);
    if(!crouching.TrySetHeight(1.8f,lowRoof))return 24;
    const auto baseMesh=terrain.CreateMesh(Mistwatch::GroundMeshSettings());
    const auto trailMesh=Mistwatch::CreateTerrainMesh(terrain);
    if(!trailMesh.Valid() || trailMesh.indices!=baseMesh.indices || trailMesh.vertices.size()!=baseMesh.vertices.size()) return 12;
    unsigned changedColors=0;
    const auto surfaceMesh=Mistwatch::CreateTerrainMesh(terrain,false);
    if(surfaceMesh.indices!=trailMesh.indices||surfaceMesh.vertices.size()!=trailMesh.vertices.size())return 34;
    for(std::size_t i=0;i<trailMesh.vertices.size();++i) {
        const auto& a=baseMesh.vertices[i];const auto& b=trailMesh.vertices[i];
        if(a.x!=b.x || a.y!=b.y || a.z!=b.z || a.normal!=b.normal) return 13;
        const auto& surface=surfaceMesh.vertices[i];
        if(surface.x!=b.x||surface.y!=b.y||surface.z!=b.z||surface.normal!=b.normal||
           (surface.color>>24)!=255u||(surface.color&0x00ffffffu)!=(b.color&0x00ffffffu))return 35;
        // 高度只佔 alpha；泥土步道的舊 RGB 範圍驗證仍完整保留。
        const auto expectedAlpha=unsigned(std::lround(std::clamp((b.y-terrain.MinimumHeight())/
            (terrain.MaximumHeight()-terrain.MinimumHeight()),0.f,1.f)*255.f));
        if((b.color>>24)!=expectedAlpha) {
            std::cerr<<"FIRST_DIVERGENCE: terrain normalized height alpha at vertex="<<i
                     <<" actual="<<(b.color>>24)<<" expected="<<expectedAlpha<<'\n';return 33;
        }
        if((a.color&0x00ffffffu)!=(b.color&0x00ffffffu)) {
            ++changedColors;
            const bool workshopBranch=a.x>=-9.f&&a.x<=2.f&&a.z>=57.f&&a.z<=61.f;
            const bool generatorBranch=a.x>=-2.f&&a.x<=9.f&&a.z>=63.f&&a.z<=67.f;
            const bool rangerBranch=a.x>=-2.f&&a.x<=10.f&&a.z>=77.f&&a.z<=81.f;
            // 獨立計算三條中繼支路範圍；新增步道不得改動範圍外的地表。
            bool relayBranch=false;
            for(const auto site:Mistwatch::Session::relaySites) {
                const float closestX=std::clamp(a.x,(std::min)(0.f,site.x),(std::max)(0.f,site.x));
                relayBranch|=std::hypot(a.x-closestX,a.z-site.z)<=1.15001f;
            }
            if((std::abs(a.x)>4.f&&!workshopBranch&&!generatorBranch&&!rangerBranch&&!relayBranch) ||
               a.z< -10.f || a.z>96.f || (a.z>32.f && a.z<40.f)) return 14;
        }
    }
    if(changedColors<40) return 15;
    for(const Aevrynd::Vec3 point:std::array<Aevrynd::Vec3,6>{{{-6,0,59},{6,0,65},{7,0,79},
            {-22,0,62},{22,0,86},{-20,0,94}}}) {
        bool tinted=false;
        for(std::size_t i=0;i<trailMesh.vertices.size();++i) {
            const auto& vertex=trailMesh.vertices[i];
            if(vertex.x==point.x&&vertex.z==point.z)tinted=(vertex.color&0x00ffffffu)!=(baseMesh.vertices[i].color&0x00ffffffu);
        }
        if(!tinted)return 32;
    }
    const auto trees=Mistwatch::TreePositions(), repeated=Mistwatch::TreePositions();
    if(trees.size()<60 || trees.size()!=repeated.size()) return 6;
    for(std::size_t i=0;i<trees.size();++i) {
        if(Aevrynd::Length(trees[i]-repeated[i])!=0 || std::abs(trees[i].x)<4.f) return 7;
        if(std::abs(trees[i].y-terrain.SampleHeight(trees[i].x,trees[i].z))>.001f) return 8;
        for(const auto supply:Mistwatch::Session::supplies)
            if(std::hypot(trees[i].x-supply.x,trees[i].z-supply.z)<3.f) return 9;
    }
    if(terrain.SampleHeight(0,36)>-5 || terrain.SampleHeight(30,10)<2) return 4;
    for(bool repaired:{false,true}) {
        auto solids=Mistwatch::WorldSolids(repaired);
        Aevrynd::CharacterController3D actor;
        actor.Reset({0,0,30});
        for(int tick=0;tick<150;++tick) {
            actor.Step({{0,0,1},false,1},solids,terrain,1.f/60.f);
            if(repaired && actor.Position().y<-.1f) return 5;
            // 與遊戲相同：首次墜落即死亡，不繼續移動至對岸。
            if(actor.Position().y< -5.f) break;
        }
        std::cout<<"repaired="<<repaired<<" position="<<actor.Position().x<<","<<actor.Position().y<<","<<actor.Position().z<<"\n";
        if(repaired && (actor.Position().z<44.f || actor.Position().y<-.1f)) return 1;
        if(!repaired && actor.Position().y>=-5.f) return 2;
    }
    Aevrynd::CharacterController3D actor;
    actor.Reset({-9,0,-5});
    const auto forest=Mistwatch::WorldSolids(false);
    for(int tick=0;tick<90;++tick) actor.Step({{1,0,0},false,1},forest,terrain,1.f/60.f);
    if(actor.Position().x> -7.5f || actor.Position().x< -8.f) return 3;
    actor.Reset(Mistwatch::Session::generator+Aevrynd::Vec3{-3,0,0});
    for(int tick=0;tick<90;++tick) actor.Step({{1,0,0},false,1},forest,terrain,1.f/60.f);
    if(actor.Position().x>6.f || actor.Position().x<5.f) return 10;
    // 起點在新增櫃體的西側，持續獨立驗證工棚西牆，而不是先撞到櫃子。
    actor.Reset(Mistwatch::Session::fuse+Aevrynd::Vec3{-1.75f,0,0});
    for(int tick=0;tick<90;++tick) actor.Step({{-1,0,0},false,1},forest,terrain,1.f/60.f);
    if(actor.Position().x< -9.1f || actor.Position().x> -8.f) return 11;
    std::cout<<"Bridge repair changes actual controller traversal PASS\n";
    std::cout<<"Visible tree trunk blocks actual controller traversal PASS\n";
}
