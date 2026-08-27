#include <Aevrynd/SkinnedModelAsset.h>
#include <iostream>

using namespace Aevrynd;

int main(int argumentCount,char** arguments){
    if(argumentCount!=2){std::cerr<<"Expected one AESKIN v2 path\n";return 1;}
    const SkinnedModelAssetDetailedResult loaded=LoadSkinnedModelAssetFileDetailed(arguments[1]);
    if(!loaded){std::cerr<<loaded.error<<'\n';return 2;}
    std::cout<<"vertices="<<loaded.asset.model.bindMesh.vertices.size()
             <<" indices="<<loaded.asset.model.bindMesh.indices.size()
             <<" joints="<<loaded.asset.jointNames.size()
             <<" submeshes="<<loaded.asset.submeshes.size()
             <<" materials="<<loaded.asset.materialSlotNames.size()
             <<" clips="<<loaded.asset.clips.size()<<'\n';
    return 0;
}
