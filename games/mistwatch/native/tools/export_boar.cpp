#include "MistwatchBoar.h"
#include <Aevrynd/SkinnedModelAsset.h>
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=2)return 1;
    const Mistwatch::Boar boar;
    if(!boar.model.Valid())return 2;
    std::string error;
    if(!Aevrynd::SaveSkinnedModelAssetFile(argv[1],boar.model,&error)){std::cerr<<error<<'\n';return 3;}
    const auto loaded=Aevrynd::LoadSkinnedModelAssetFile(argv[1]);
    if(!loaded||loaded.model.skeleton.joints.size()!=16||loaded.model.bindMesh.indices!=boar.model.bindMesh.indices)return 4;
    std::cout<<"BOAR_ASSET_READY; joints=16; vertices="<<loaded.model.bindMesh.vertices.size()<<'\n';
}
