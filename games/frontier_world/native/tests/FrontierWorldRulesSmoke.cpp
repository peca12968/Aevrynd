#include "FrontierWorldRules.h"
#include <cmath>
#include <iostream>

using namespace FrontierWorldRules;
int main(){const auto foundation=Cost(BuildingKind::Foundation),wall=Cost(BuildingKind::Wall),roof=Cost(BuildingKind::Roof);if(foundation.wood!=8||wall.stone!=2||roof.wood!=14||!CanAfford(20,10,BuildingKind::Wall)||CanAfford(2,2,BuildingKind::Foundation)){std::cerr<<"Building economy failed\n";return 1;}const auto snapped=SnapBuildPosition({3.7f,0,6.4f});if(std::abs(snapped.x-2.5f)>.001f||std::abs(snapped.z-7.5f)>.001f){std::cerr<<"Building grid snap failed\n";return 2;}if(!(ChunkAt({-1,0,-33})==ChunkCoordinate{-1,-2})||ChunkSeed({2,3})==ChunkSeed({3,2})){std::cerr<<"World chunk mapping failed\n";return 3;}if(!CanMeleeHit({0,0,0},0,{0,0,2})||CanMeleeHit({0,0,0},0,{0,0,-2})||CanMeleeHit({0,0,0},0,{0,0,4})){std::cerr<<"Melee arc failed\n";return 4;}if(ExperienceForLevel(2)<=ExperienceForLevel(1)){std::cerr<<"Experience curve failed\n";return 5;}std::cout<<"foundation="<<foundation.wood<<'/'<<foundation.stone<<" chunkSeed="<<ChunkSeed({2,3})<<" level2Xp="<<ExperienceForLevel(2)<<'\n';return 0;}
