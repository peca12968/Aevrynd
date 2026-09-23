#include "GameplayBuildingIdentities3D.h"
#include <iostream>

int main() {
  using namespace GameplayLab;
  constexpr std::uint64_t domain=73;
  for(const std::size_t landCount:{456u,148u,7u}) {
    std::vector<ShowcaseSceneDescriptor3D> descriptors(landCount+2);
    descriptors[0].kind=ShowcaseSceneKind3D::Terrain;
    descriptors[1].kind=ShowcaseSceneKind3D::Water;
    for(std::size_t i=2;i<descriptors.size();++i)descriptors[i].kind=ShowcaseSceneKind3D::Tree;
    for(unsigned i=0;i<6;++i){ShowcaseSceneDescriptor3D building{};building.kind=ShowcaseSceneKind3D::Building;descriptors.push_back(building);}
    ShowcaseSceneDescriptor3D bridge{};bridge.kind=ShowcaseSceneKind3D::Building;bridge.meshAsset="procedural/forest-river-bridge";descriptors.push_back(bridge);
    const auto identities=BuildGameplayBuildingIdentities3D(descriptors,domain);
    for(std::size_t part=0;part<identities.size();++part){
      const auto identity=identities[part];
      if(identity.id.low!=landCount+3+part||descriptors[identity.id.low-1].kind!=ShowcaseSceneKind3D::Building){
        std::cerr<<"building identity classified land/tree: land="<<landCount<<" expected="<<landCount+3+part<<" actual="<<identity.id.low<<'\n';return 1;
      }
      if(GameplayBuildingPart3D(identities,identity)!=part)return 2;
      auto wrong=identity;wrong.generation=2;if(GameplayBuildingPart3D(identities,wrong)!=6)return 3;
      wrong=identity;wrong.id.high++;if(GameplayBuildingPart3D(identities,wrong)!=6)return 4;
    }
    if(GameplayBuildingPart3D(identities,{{domain,3},1})!=6)return 5;
    if(GameplayBuildingPart3D(identities,{{domain,descriptors.size()},1})!=6)return 10;
  }
  std::vector<ShowcaseSceneDescriptor3D> mixed(12);
  for(std::size_t i=0;i<mixed.size();++i)mixed[i].kind=i%2?ShowcaseSceneKind3D::Building:ShowcaseSceneKind3D::Tree;
  const auto mapped=BuildGameplayBuildingIdentities3D(mixed,domain);
  std::array<unsigned,6> coverage{};
  for(std::size_t i=0;i<mixed.size();++i){
    const Aevrynd::RenderObjectIdentity3D identity{{domain,i+1},1};
    const auto part=GameplayBuildingPart3D(mapped,identity);
    if(i%2){if(part!=i/2||mapped[part]!=identity)return 6;coverage[part]+=7;}
    else if(part!=mapped.size())return 7;
  }
  for(auto pixels:coverage)if(pixels!=7)return 8;
  for(const unsigned count:{0u,5u,7u}){
    std::vector<ShowcaseSceneDescriptor3D> invalid(count);
    for(auto& descriptor:invalid)descriptor.kind=ShowcaseSceneKind3D::Building;
    bool rejected=false;
    try{(void)BuildGameplayBuildingIdentities3D(invalid,domain);}catch(const std::runtime_error&){rejected=true;}
    if(!rejected)return 9;
  }
  std::cout<<"Gameplay building identities: three land counts, interleaved coverage/removal/receipt mapping, full identity and incomplete/overflow rejection passed\n";
}
