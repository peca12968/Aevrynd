#include "RiverbankPartyMarker.h"
#include <array>
#include <iostream>

using namespace Mistwatch;
using namespace Aevrynd;
int main() {
    constexpr std::uint32_t flag = RiverbankBattleWire::DownedEntityFlag;
    const auto project = [](float x,float y) {return ProjectedPoint{{x,y},.5f,true};};
    const RiverbankPartyWire::Member ranger{2,RiverbankClass::Ranger,63};
    const RiverbankPartyWire::Member guardian{3,RiverbankClass::Guardian,0};
    const RiverbankPartyWire::Member priest{4,RiverbankClass::Priest,21};
    const auto living = MakeRiverbankPartyMarker(ranger,0,project(620,470),5.f);
    const auto downed = MakeRiverbankPartyMarker(guardian,flag,project(625,475),7.f);
    const auto revived = MakeRiverbankPartyMarker(guardian,0,project(620,410),5.f);
    const auto another = MakeRiverbankPartyMarker(priest,0,project(850,445),7.f);
    if (!living || !living->showHealth || living->downed || std::abs(living->healthFraction-.6f)>.0001f ||
        !downed || downed->showHealth || !downed->downed || !revived || revived->downed ||
        revived->showHealth || !another) return 1;
    if (MakeRiverbankPartyMarker(ranger,0,project(640,450),12.1f) ||
        MakeRiverbankPartyMarker(ranger,0,project(50,470),2.f) ||
        MakeRiverbankPartyMarker(ranger,0,project(640,568),2.f) ||
        MakeRiverbankPartyMarker(ranger,0,ProjectedPoint{{640,455},.5f,false},2.f)) return 2;
    std::array markers{*living,*another,*downed};
    if (ArrangeRiverbankPartyMarkers(markers)!=2 || markers[0].actor!=guardian.actor || markers[1].actor!=priest.actor)
        return 3;
    std::cout << "PARTY_MARKERS_HEALTH_RECOVERY_VISIBILITY_PRIORITY_VERIFIED\n";
}
