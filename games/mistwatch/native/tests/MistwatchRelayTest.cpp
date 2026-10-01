#include "MistwatchWorld.h"
#include <Aevrynd/CharacterController3D.h>
#include <iostream>
int main() {
    Mistwatch::Session game;
    if(!game.Restore("MISTWATCH 5 7 100 1 1 0 0 0 70 1 1 7 7 1 50"))return 2;
    if(game.LightBeacon()) {
        std::cerr<<"FIRST_DIVERGENCE: rescue beacon activates immediately after power without repairing the ridge relay network\n";
        return 1;
    }
    using namespace Mistwatch;
    Session early;
    early.feet=Session::relaySites[0];
    if(early.TuneRelay(0,1)||early.ConnectRelay(0))return 3;
    const auto powered=game.Serialize();
    if(game.CurrentStage()!=Stage::Relays||game.relayLinked||game.relaySectors)return 4;
    unsigned rejected{};
    for(unsigned index=0;index<3;++index) {
        if(!game.Restore(powered))return 5;
        game.feet=Session::relaySites[index];
        if(game.TuneRelay(index,0)||game.TuneRelay(3,1)||game.ConnectRelay(3))return 6;
        for(unsigned sector=0;sector<16;++sector) {
            Session candidate;
            if(!candidate.Restore(game.Serialize())||candidate.RelaySector(index)!=sector||
                (candidate.RelaySignal(index)==100)!=(sector==Session::RelayTargetSector(index)))return 7;
            const auto before=candidate.Serialize();
            if(sector==Session::RelayTargetSector(index)) {
                if(!candidate.ConnectRelay(index)||candidate.ConnectRelay(index)||candidate.TuneRelay(index,1)||!candidate.Valid())return 8;
            } else {
                if(candidate.ConnectRelay(index)||candidate.Serialize()!=before)return 9;
                ++rejected;
            }
            if(!game.TuneRelay(index,1))return 10;
        }
        if(game.RelaySector(index))return 11;
        if(!game.TuneRelay(index,-1)||game.RelaySector(index)!=15)return 12;
        const auto checkpoint=game.Serialize();
        game.feet.y+=.2f;
        if(game.TuneRelay(index,1)||game.ConnectRelay(index))return 13;
        if(!game.Restore(checkpoint))return 14;
        game.Damage(100);
        if(game.TuneRelay(index,1)||game.ConnectRelay(index))return 15;
    }
    if(!game.Restore(powered))return 16;
    const auto terrain=CreateTerrain();const auto solids=WorldSolids(true);
    Aevrynd::CharacterController3D actor;
    actor.Reset({0,0,62});
    unsigned routeTicks{};
    const auto walk=[&](Aevrynd::Vec3 destination) {
        for(unsigned tick=0;tick<1800;++tick) {
            auto direction=destination-actor.Position();direction.y=0;
            if(Aevrynd::Length(direction)<.12f)return true;
            actor.Step({Aevrynd::Normalize(direction),false,1},solids,terrain,1.f/60.f);++routeTicks;
        }
        std::cerr<<"Relay route stalled at "<<actor.Position().x<<','<<actor.Position().y<<','<<actor.Position().z<<'\n';return false;
    };
    for(unsigned index=0;index<3;++index) {
        const auto p=Session::relaySites[index];
        const Aevrynd::Vec3 approach=p+Aevrynd::Vec3{p.x<0?1.15f:-1.15f,0,0};
        if(!walk({0,0,p.z})||!walk(approach))return 17;
        game.feet=actor.Position();
        if(!game.NearRelay(index)) {
            std::cerr<<"NearRelay divergence at index "<<index<<" y="<<game.feet.y<<" ground="<<GroundElevation(game.feet.x,game.feet.z)<<'\n';return 18;
        }
        for(unsigned tick=0;tick<30;++tick) {
            actor.Step({{p.x<0?-1.f:1.f,0,0},false,1},solids,terrain,1.f/60.f);++routeTicks;
        }
        if(std::abs(actor.Position().x-p.x)<.65f+actor.Settings().radius-.02f||actor.Stats().collisions==0)return 30;
        game.feet=actor.Position();
        if(!game.NearRelay(index))return 31;
        for(unsigned turn=0;turn<Session::RelayTargetSector(index);++turn)if(!game.TuneRelay(index,1))return 19;
        if(!game.ConnectRelay(index))return 20;
        Session loaded;if(!loaded.Restore(game.Serialize())||loaded.relayLinked!=game.relayLinked||loaded.relaySectors!=game.relaySectors)return 21;
        const auto mask=game.relayLinked,angles=game.relaySectors;game.Damage(100);game.Retry();
        if(game.relayLinked!=mask||game.relaySectors!=angles)return 22;
        // 上述重試只驗證狀態；路線仍由目前控制器連續行走，沒有傳送至下一站。
        game.feet=actor.Position();
        if(!walk({0,0,p.z}))return 23;
    }
    if(game.CurrentStage()!=Stage::Beacon||!walk(Session::beacon))return 24;
    game.feet=actor.Position();if(!game.LightBeacon()||game.CurrentStage()!=Stage::Complete)return 25;
    const auto completed=game.Serialize();
    for(const auto invalid:{"MISTWATCH 6 7 100 1 1 0 0 0 70 1 1 7 7 1 50 8 0",
        "MISTWATCH 6 7 100 1 1 0 0 0 70 1 1 7 7 1 50 0 4096",
        "MISTWATCH 6 7 100 1 1 0 0 0 70 1 1 7 7 1 50 1 0",
        "MISTWATCH 6 7 100 1 1 1 0 0 70 1 1 7 7 1 50 0 0",
        "MISTWATCH 6 7 100 1 1 0 0 0 70 1 0 7 7 1 50 0 1",
        "MISTWATCH 6 7 100 1 1 0 0 0 70 1 1 7 7 1 50 0",
        "MISTWATCH 7 7 100 1 1 0 0 0 70 1 1 7 7 1 50 0 0"})
        if(game.Restore(invalid)||game.Serialize()!=completed)return 26;
    for(const auto legacy:{"MISTWATCH 1 7 100 1 1 1 0 0 70",
        "MISTWATCH 2 7 100 1 1 1 0 0 70 1 1",
        "MISTWATCH 3 7 100 1 1 1 0 0 70 1 1 7",
        "MISTWATCH 4 7 100 1 1 1 0 0 70 1 1 7 7",
        "MISTWATCH 5 7 100 1 1 1 0 0 70 1 1 7 7 1 50"})
        if(!game.Restore(legacy)||game.CurrentStage()!=Stage::Complete||game.relayLinked!=7||!game.Valid())return 27;
    const auto dish=CreateRelayDish();
    if(dish.vertices.size()!=65||dish.indices.size()!=336)return 28;
    for(const auto& vertex:dish.vertices) {
        const auto normal=Aevrynd::UnpackNormalSnorm8(vertex.normal);
        if(normal.z<=0||std::abs(Aevrynd::Length(Aevrynd::Vec3{normal.x,normal.y,normal.z})-1.f)>.02f)return 33;
        const auto color=Aevrynd::UnpackColorRgba8(vertex.color);
        if(color.r<.70f||color.r>.74f||color.g<.74f||color.g>.78f||color.b<.71f||color.b>.75f)return 34;
    }
    for(unsigned i=0;i<dish.indices.size();i+=3) {
        const auto& a=dish.vertices[dish.indices[i]];const auto& b=dish.vertices[dish.indices[i+1]];const auto& c=dish.vertices[dish.indices[i+2]];
        const auto normal=Aevrynd::Cross(Aevrynd::Vec3{b.x-a.x,b.y-a.y,b.z-a.z},Aevrynd::Vec3{c.x-a.x,c.y-a.y,c.z-a.z});
        if(normal.z<=.00001f)return 29;
    }
    std::cout<<"Relays PASS; rejected sectors="<<rejected<<" save_v6=passed legacy_v1_to_v5=passed collision-backed ridge route seconds="<<float(routeTicks)/60.f<<'\n';
    return 0;
}
